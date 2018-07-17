ifneq ($(KERNELRELEASE),)
obj-m:=mem_dev.o mem_drv.o
else
KERNELDIR:=/usr/src/linux-headers-4.4.0-101-generic
PWD:=$(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.o *.mod.c *.mod.o *.ko
endif
