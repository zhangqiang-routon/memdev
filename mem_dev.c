#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
//#include <asm/mach/irq.h>
//#include <asm/arch/regs-gpio.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
/* 参考arch/arm/plat-s3c24xx/devs.c */
/*1. 根据芯片手册来获取资源*/
static struct resource mem_resource[] = {
	 [0] = {
		   //.start = 0x56000050,
		    // .end   = 0x56000057,
		     //  .flags = IORESOURCE_MEM,
		        },
	  [1] = {
		    //.start = 5,
		     // .end   = 5,
		      //  .flags = IORESOURCE_IRQ,
			 },
};
void mem_release(struct device *dev)
{ 
}
/*1.构建平台设备结构体，将平台资源加入进来*/
struct platform_device mem_device = {
	 .name    = "memdev", /* 使用名为"myled"的平台驱动 */
	  .id    = -1,
	   .dev = {
		     .release = mem_release,
		      },
	    .num_resources   = ARRAY_SIZE(mem_resource),
	     .resource   = mem_resource,
};
/*2。把我们的设备资源挂在到虚拟总线的设备连表中去*/
int mem_dev_init(void)
{
	 platform_device_register(&mem_device);    
	  return 0;
}
void mem_dev_exit(void)
{
	 platform_device_unregister(&mem_device);
}
module_init(mem_dev_init);
module_exit(mem_dev_exit);
MODULE_LICENSE("GPL"); 
