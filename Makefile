obj-m += mottura_xnova.o

KDIR ?= "/lib/modules/$(shell uname -r)/build"

all:
        make -C $(KDIR) M=$(PWD) modules

clean:
        make -C $(KDIR) M=$(PWD) clean