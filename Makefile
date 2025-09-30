obj-m += mottura_xnova.o

KDIR ?= "/lib/modules/$(shell uname -r)/build"

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

deploy:
	scp mottura_xnova.ko root@$(HOST):/lib/modules/$(shell uname -r)/

format:
	clang-format -i *.c -style=file:$(KDIR)/.clang-format
