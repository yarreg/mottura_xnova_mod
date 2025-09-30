#!/bin/bash

KERNEL_DEB="https://deb.wirenboard.com/wb8/bullseye/pool/main/l/linux-wb/linux-headers-wb8_6.8.0-wb140_arm64.deb"
wget -c $KERNEL_DEB -O /tmp/kernel.deb
dpkg -x /tmp/kernel.deb linux
rm -f /tmp/kernel.deb