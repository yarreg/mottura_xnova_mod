Mottura XNova kernel module
===========================

This is a kernel module for the Mottura XNova lock.
Bacause Mottura XNova goes to sleep after 3 sec, this module will keep it awake. By sending a keep alive signal every 2 sec.
When lock is in sleep mode, it will not provide status information by output pins. Using this module we can read status of the lock at any time.
Also this module will provide a way to open/close/autoclose the lock by sending a signal to the lock.

Available commands:
```
echo "open" > /dev/mottura_xnova

echo "close" > /dev/mottura_xnova

echo "autoclose" > /dev/mottura_xnova

```

THis module do for WirenBoard 6.5. It may work on other devices, but not tested.
GPIO configured for A1 A2 A3 pins. You can change it in the source code.


**How to build and install**
Prepare the kernel source code:
```
apt update
apt install build-essential libncurses5-dev fakeroot lzop bc git bison flex libssl-dev rsync
apt install gcc-arm-linux-gnueabihf

cd ~
git clone https://github.com/wirenboard/linux
cd linux
git submodule update --init --recursive

export KDIR=~/linux
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KBUILD_OUTPUT=.build-wb6

make prepare
```

Build the module:
```
cd ~/mottura_xnova
make
```