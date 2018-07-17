ifneq ($(KERNELRELEASE), )

obj-m	:= driver.o

else
KDIR	:=/opt/EmbedSky/linux-2.6.30.4

PWD	:=$(shell pwd)

default:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers

endif
