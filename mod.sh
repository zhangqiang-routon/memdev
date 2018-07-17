#/bin/bash
if [ $1 == "i" ]; then
echo "begin insmod the mem_dev and mem_drv mod"
insmod mem_dev.ko
insmod mem_drv.ko
elif [ $1 == "r" ]; then
echo "begin to remove the mem_dev and mem_drv mod"
rmmod mem_dev.ko
rmmod mem_drv.ko
fi

