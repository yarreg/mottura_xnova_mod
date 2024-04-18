#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>


static struct timer_list wakeup_timer;

// chrdev variables
static dev_t dev_num;
static struct cdev xnova_cdev;
static struct class *xnova_class;

// XNova GPIOs
static struct gpio_desc *xnova_open_gpio;
static struct gpio_desc *xnova_close_gpio;
static struct gpio_desc *xnova_autoclose_gpio;

// Pulse work
static struct work_struct pulse_work;
static struct gpio_desc *pulse_work_gpio;

// Configuration
#define MOTTURA_XNOVA_OPEN_GPIO 79      // WB6 A1
#define MOTTURA_XNOVA_CLOSE_GPIO 80     // WB6 A2
#define MOTTURA_XNOVA_AUTOCLOSE_GPIO 81 // WB6 A3

#define WAKEUP_GPIO xnova_close_gpio
#define PULSE_DURATION 60
#define PULSE_INTERVAL 2500

#define CHECK_ERROR(call, message) \
    do { \
        int ret = call; \
        if (ret < 0) { \
            pr_err("Mottura XNova: %s: %d\n", message, ret); \
            return ret; \
        } \
    } while (0)

static bool wakeup_pulse_period = false;


// Mottura XNova goes to sleep after ~3 seconds. So we need to keep the device working.
// To do this, we will use a timer that sends a pulse to the XNova every 2.5 seconds.
// The pulse will be 60ms long.
static void wakeup_time_func(struct timer_list *unused)
{
    // Toggle wakeup pin
    gpiod_set_value(WAKEUP_GPIO, wakeup_pulse_period);
    wakeup_pulse_period = !wakeup_pulse_period;

    // Set the next timer expiry
    wakeup_timer.expires = jiffies + (wakeup_pulse_period ? msecs_to_jiffies(PULSE_INTERVAL) : msecs_to_jiffies(PULSE_DURATION));
    mod_timer(&wakeup_timer, wakeup_timer.expires);
}

static void pulse_work_func(struct work_struct *ws) {
    gpiod_set_value(pulse_work_gpio, 1);
    msleep(1000);
    gpiod_set_value(pulse_work_gpio, 0);

    // Restart the wakeup timer 2000 ms after operation
    mod_timer(&wakeup_timer, jiffies + msecs_to_jiffies(2000));
}

static ssize_t xnova_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    struct gpio_desc *gpio = NULL;
    char *newline;
    char command[12];

    if (copy_from_user(command, buf, min(count, sizeof(command) - 1)))
        return -EFAULT;

    // Remove newline character if present
    newline = strchr(command, '\n');
    if (newline)
        *newline = '\0';

    if (strcmp(command, "open") == 0) {
        gpio = xnova_open_gpio;
    } else if (strcmp(command, "close") == 0) {
        gpio = xnova_close_gpio;
    } else if (strcmp(command, "autoclose") == 0) {
        gpio = xnova_autoclose_gpio;
    }

    if (gpio == NULL)
        return -EINVAL;

    // Stop the wakeup timer during operation
    del_timer_sync(&wakeup_timer);

    // Start the pulse work
    pulse_work_gpio = gpio;
    schedule_work(&pulse_work);

    return count;
}

static struct file_operations xnova_fops = {
    .owner = THIS_MODULE,
    .write = xnova_write,
};

static int create_chrdev(const char *name) {
    int ret;

    // Allocate device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, name);
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        return ret;
    }

    // Initialize cdev
    cdev_init(&xnova_cdev, &xnova_fops);
    xnova_cdev.owner = THIS_MODULE;

    // Add cdev to the system
    ret = cdev_add(&xnova_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        return ret;
    }

    // Create class
    xnova_class = class_create(THIS_MODULE, name);
    if (IS_ERR(xnova_class)) {
        pr_err("Failed to create class\n");
        return PTR_ERR(xnova_class);
    }

    INIT_WORK(&pulse_work, pulse_work_func);

    // Create device
    device_create(xnova_class, NULL, dev_num, NULL, name);
    return 0;
}

static int init_gpio(struct gpio_desc **gpiod, int gpio_num) {
    int ret;

    *gpiod = gpio_to_desc(gpio_num);
    if (IS_ERR(*gpiod)) {
        pr_err("Failed to get GPIO descriptor: %ld\n", PTR_ERR(*gpiod));
        return PTR_ERR(*gpiod);
    }
    ret = gpiod_direction_output(*gpiod, 0);
    if (ret < 0) {
        pr_err("Failed to set GPIO as output: %d\n", gpio_num);
        return ret;
    }
    return 0;
}

static int __init mottura_xnova_init(void)
{
    // Get the GPIO descriptor
    const char *gpio_err = "Failed to initialize GPIO";
    CHECK_ERROR(init_gpio(&xnova_open_gpio, MOTTURA_XNOVA_OPEN_GPIO), gpio_err);
    CHECK_ERROR(init_gpio(&xnova_close_gpio, MOTTURA_XNOVA_CLOSE_GPIO), gpio_err);
    CHECK_ERROR(init_gpio(&xnova_autoclose_gpio, MOTTURA_XNOVA_AUTOCLOSE_GPIO), gpio_err);

    // Create character device
    CHECK_ERROR(create_chrdev("mottura_xnova"), "Failed to create character device");

    // Initialize the timer
    timer_setup(&wakeup_timer, wakeup_time_func, 0);
    wakeup_timer.expires = jiffies + msecs_to_jiffies(PULSE_INTERVAL);
    add_timer(&wakeup_timer);

    pr_info("Mottura XNova module loaded\n");

    return 0;
}

static void __exit mottura_xnova_exit(void)
{
    // Cancel work
    cancel_work_sync(&pulse_work);

    // Timer cleanup
    del_timer_sync(&wakeup_timer);

    // GPIO cleanup
    gpiod_set_value(xnova_open_gpio, 0);
    gpiod_put(xnova_open_gpio);

    gpiod_set_value(xnova_close_gpio, 0);
    gpiod_put(xnova_close_gpio);

    gpiod_set_value(xnova_autoclose_gpio, 0);
    gpiod_put(xnova_autoclose_gpio);

    // chrdev cleanup
    device_destroy(xnova_class, dev_num);
    class_destroy(xnova_class);
    cdev_del(&xnova_cdev);
    unregister_chrdev_region(dev_num, 1);

    pr_info("Mottura XNova module unloaded\n");
}

MODULE_LICENSE("GPL");
module_init(mottura_xnova_init);
module_exit(mottura_xnova_exit);
