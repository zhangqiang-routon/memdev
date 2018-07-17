EXTRA_CFLAGS := -g
obj-m += ldm3.o

default:
	make -C /lib/modules/$(shell uname -r)/build/ SUBDIRS=$(shell pwd) modules 
