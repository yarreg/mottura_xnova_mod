Mottura XNova kernel module
===========================

This kernel module is designed for the Mottura XNova lock. The XNova lock enters sleep mode after 3 seconds of inactivity. To prevent this, our module sends a keep-alive signal every 2 seconds, ensuring the lock remains awake.
When the lock is in sleep mode, it does not provide status information via its output pins. However, with this module, we can read the lock's status at any time.
Additionally, this module provides functionality to open, close, or set the lock to autoclose by sending a specific signal to the lock.
It's important to note that running similar programs in user-space could lead to the application being preempted or even killed by the Out-Of-Memory (OOM) killer. Running this as a kernel module ensures more reliable operation.

Available commands:
```
echo "open" > /dev/mottura_xnova

echo "close" > /dev/mottura_xnova

echo "autoclose" > /dev/mottura_xnova

```

THis module do for WirenBoard 8. It may work on other devices, but not tested.
GPIO configured for A1 A2 A3 pins. You can change it in the source code.


**How to build and install on WirenBoard 8**

Prepare the kernel source code:
```
apt update
apt install build-essential libncurses5-dev fakeroot lzop bc git bison flex libssl-dev rsync
apt install linux-headers


git clone https://github.com/yarreg/mottura_xnova_mod
cd mottura_xnova_mod
make
cp mottura_xnova.ko /lib/modules/$(uname -r)/
depmod -a
modprobe mottura_xnova

echo "mottura_xnova" > /etc/modules-load.d/mottura_xnova.conf
```