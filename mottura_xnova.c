#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#define MODULE_NAME "mottura_xnova"

// Pulse configuration
#define WAKEUP_GPIO close_gpio

// Module parameters
static int open_gpio_num = 133;
static int close_gpio_num = 119;
static int autoclose_gpio_num = 201;
static int pulse_duration = 60;   // ms
static int pulse_interval = 2500; // ms

module_param(open_gpio_num, int, 0660);
MODULE_PARM_DESC(open_gpio_num, "GPIO for open operation");
module_param(close_gpio_num, int, 0660);
MODULE_PARM_DESC(close_gpio_num, "GPIO for close operation");
module_param(autoclose_gpio_num, int, 0660);
MODULE_PARM_DESC(autoclose_gpio_num, "GPIO for autoclose operation");
module_param(pulse_duration, int, 0660);
MODULE_PARM_DESC(pulse_duration, "Wakeup pulse duration in ms");
module_param(pulse_interval, int, 0660);
MODULE_PARM_DESC(pulse_interval, "Wakeup pulse interval in ms");

// GPIO descriptors
static struct gpio_desc *open_gpio;
static struct gpio_desc *close_gpio;
static struct gpio_desc *autoclose_gpio;
static struct gpio_desc *pulse_work_gpio;

// Timer and workqueue structures
static struct timer_list wakeup_timer;
static struct work_struct pulse_on_work;
static struct delayed_work pulse_off_work;

// Character device structures
static dev_t dev_num;
static struct cdev xnova_cdev;
static struct class *xnova_class;

// Wakeup pulse state
static bool wakeup_pulse_period = false;

// Error handling macro
#define CHECK_ERROR(call, message)                                             \
  do {                                                                         \
    int ret = call;                                                            \
    if (ret < 0) {                                                             \
      pr_err(MODULE_NAME ": %s: %d\n", message, ret);                          \
      return ret;                                                              \
    }                                                                          \
  } while (0)

// Timer callback function to send a wakeup pulse
static void wakeup_time_func(struct timer_list *unused) {
  gpiod_set_value(WAKEUP_GPIO, wakeup_pulse_period);
  wakeup_pulse_period = !wakeup_pulse_period;

  wakeup_timer.expires =
      jiffies + (wakeup_pulse_period ? msecs_to_jiffies(pulse_interval)
                                     : msecs_to_jiffies(pulse_duration));
  mod_timer(&wakeup_timer, wakeup_timer.expires);
}

// Workqueue: assert pulse immediately
static void pulse_on_work_func(struct work_struct *ws) {
  gpiod_set_value(pulse_work_gpio, 1);
  schedule_delayed_work(&pulse_off_work, msecs_to_jiffies(1000));
}

// Workqueue: deassert pulse after delay
static void pulse_off_work_func(struct work_struct *ws) {
  gpiod_set_value(pulse_work_gpio, 0);

  // Activate periodic wake pulses after command pulse
  mod_timer(&wakeup_timer, jiffies + msecs_to_jiffies(2000));
}

// Character device write function
static ssize_t xnova_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos) {
  struct gpio_desc *gpio = NULL;
  char command[12];
  char *newline;

  if (copy_from_user(command, buf, min(count, sizeof(command) - 1)))
    return -EFAULT;

  command[min(count, sizeof(command) - 1)] = '\0';

  newline = strchr(command, '\n');
  if (newline)
    *newline = '\0';

  if (strcmp(command, "open") == 0) {
    gpio = open_gpio;
  } else if (strcmp(command, "close") == 0) {
    gpio = close_gpio;
  } else if (strcmp(command, "autoclose") == 0) {
    gpio = autoclose_gpio;
  }

  if (!gpio)
    return -EINVAL;

  // Stop background wake pulses while command is active
  del_timer_sync(&wakeup_timer);

  // Cancel any in-flight pulse jobs to avoid overlap 
  cancel_work_sync(&pulse_on_work);
  cancel_delayed_work_sync(&pulse_off_work);

  pulse_work_gpio = gpio;
 
  // Start immediate ON; OFF will be queued as delayed_work
  schedule_work(&pulse_on_work);

  return count;
}

// File operations structure
static struct file_operations xnova_fops = {
    .owner = THIS_MODULE,
    .write = xnova_write,
};

// Create character device
static int create_chrdev(const char *name) {
  CHECK_ERROR(alloc_chrdev_region(&dev_num, 0, 1, name),
              "Failed to allocate device number");

  cdev_init(&xnova_cdev, &xnova_fops);
  xnova_cdev.owner = THIS_MODULE;

  CHECK_ERROR(cdev_add(&xnova_cdev, dev_num, 1), "Failed to add cdev");

  xnova_class = class_create(name);
  if (IS_ERR(xnova_class)) {
    pr_err(MODULE_NAME ": Failed to create class\n");
    return PTR_ERR(xnova_class);
  }

  device_create(xnova_class, NULL, dev_num, NULL, name);

  INIT_WORK(&pulse_on_work, pulse_on_work_func);
  INIT_DELAYED_WORK(&pulse_off_work, pulse_off_work_func);

  return 0;
}

// Initialize GPIO
static int init_gpio(struct gpio_desc **gpiod, int gpio_num) {
  *gpiod = gpio_to_desc(gpio_num);
  if (gpiod == NULL) {
	pr_err(MODULE_NAME ": GPIO %d not found\n", gpio_num);
	return -ENODEV;
  }
  CHECK_ERROR(gpiod_direction_output(*gpiod, 0),
              "Failed to set GPIO as output");

  return 0;
}

// Module initialization function
static int __init mottura_xnova_init(void) {
  const char *gpio_err = "Failed to initialize GPIO";

  CHECK_ERROR(init_gpio(&open_gpio, open_gpio_num), gpio_err);
  CHECK_ERROR(init_gpio(&close_gpio, close_gpio_num), gpio_err);
  CHECK_ERROR(init_gpio(&autoclose_gpio, autoclose_gpio_num), gpio_err);

  CHECK_ERROR(create_chrdev(MODULE_NAME), "Failed to create character device");

  timer_setup(&wakeup_timer, wakeup_time_func, 0);
  wakeup_timer.expires = jiffies + msecs_to_jiffies(pulse_interval);
  add_timer(&wakeup_timer);

  pr_info(MODULE_NAME ": module loaded\n");
  pr_info(MODULE_NAME ": pins - OPEN=%d, CLOSE=%d, AUTOCLOSE=%d\n",
          open_gpio_num, close_gpio_num, autoclose_gpio_num);

  return 0;
}

// Module cleanup function
static void __exit mottura_xnova_exit(void) {
  // cancel_work_sync(&pulse_work);
  cancel_work_sync(&pulse_on_work);
  cancel_delayed_work_sync(&pulse_off_work);
  del_timer_sync(&wakeup_timer);

  gpiod_set_value(open_gpio, 0);
  gpiod_put(open_gpio);

  gpiod_set_value(close_gpio, 0);
  gpiod_put(close_gpio);

  gpiod_set_value(autoclose_gpio, 0);
  gpiod_put(autoclose_gpio);

  device_destroy(xnova_class, dev_num);
  class_destroy(xnova_class);
  cdev_del(&xnova_cdev);
  unregister_chrdev_region(dev_num, 1);

  pr_info(MODULE_NAME ": module unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mottura XNova GPIO Control Module");
MODULE_VERSION("1.1");

module_init(mottura_xnova_init);
module_exit(mottura_xnova_exit);
