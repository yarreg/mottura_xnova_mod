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

This module do for WirenBoard 8. It may work on other devices, but not tested.
GPIO configured for A1 A2 A3 pins. You can change it in the source code.


**Build and install**

### Cross-compilation (recommended)

Build on an x86 machine with cross-compilation for WirenBoard 8 (ARM64):

```
# Install cross-compiler
apt install gcc-aarch64-linux-gnu

git clone https://github.com/yarreg/mottura_xnova_mod
cd mottura_xnova_mod

# KERNEL_VERSION must match the kernel version on the target device
make KERNEL_VERSION=6.8.0-wb153

# Copy module and run depmod on the target device
make deploy HOST=192.168.0.102
```

The Makefile automatically downloads the required kernel headers from `deb.wirenboard.com`.
Override variables as needed:
- `KERNEL_VERSION` — kernel version (default `6.8.0-wb153`)
- `HOST` — target device IP (default `192.168.0.102`)

### Native build (on WirenBoard itself)

```
apt install build-essential linux-headers-$(uname -r)

git clone https://github.com/yarreg/mottura_xnova_mod
cd mottura_xnova_mod
make ARCH=arm64 CROSS_COMPILE=
cp mottura_xnova.ko /lib/modules/$(uname -r)/extra/
depmod -a
modprobe mottura_xnova

echo "mottura_xnova" > /etc/modules-load.d/mottura_xnova.conf
```