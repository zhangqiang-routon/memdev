ifneq ($(KERNELRELEASE), )

obj-m := device.o

else

KDIR	:= /opt/EmbedSky/linux-2.6.30.4

PWD 	:= $(shell pwd)

default:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -fr *.ko *.o *.mod.o *.mod.c *.symvers *.cmd .*.*.cmd .tmp* *.order
endif
