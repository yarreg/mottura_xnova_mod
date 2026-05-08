obj-m += mottura_xnova.o

KERNEL_VERSION ?= 6.8.0-wb153
KERNEL_DEB = linux-headers-wb8_$(KERNEL_VERSION)_arm64.deb
KERNEL_DEB_URL = https://deb.wirenboard.com/wb8/bullseye/pool/main/l/linux-wb/$(KERNEL_DEB)
KERNEL_HEADERS_DIR = kernel-headers
KDIR = $(KERNEL_HEADERS_DIR)/usr/src/linux-headers-$(KERNEL_VERSION)

ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

HOST ?= 192.168.0.102

all: $(KDIR)/.config
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

$(KDIR)/.config:
	wget -c "$(KERNEL_DEB_URL)" -O /tmp/$(KERNEL_DEB)
	mkdir -p $(KERNEL_HEADERS_DIR)
	dpkg -x /tmp/$(KERNEL_DEB) $(KERNEL_HEADERS_DIR)
	rm -f /tmp/$(KERNEL_DEB)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

deploy:
	ssh root@$(HOST) "mkdir -p /lib/modules/$(KERNEL_VERSION)/extra"
	scp mottura_xnova.ko root@$(HOST):/lib/modules/$(KERNEL_VERSION)/extra/
	ssh root@$(HOST) "depmod -a $(KERNEL_VERSION)"

format:
	clang-format -i *.c
