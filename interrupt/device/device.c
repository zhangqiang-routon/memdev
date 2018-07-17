#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/string.h>
#include<linux/platform_device.h>
/*硬件相关的头文件*/
#include<mach/regs-gpio.h>
#include<mach/hardware.h>
#include<linux/gpio.h>
/*这个是硬件密切相关的*/
#include<mach/irqs.h>

/*硬件资源量*/
static struct resource tq2440_button_resource[]=
{
	/*EINT0*/
	[0]=
	{
		.flags = IORESOURCE_IRQ,
		.start = IRQ_EINT0,
		.end = IRQ_EINT0,
		.name = "S3C24XX_EINT0",
	},
	/*EINT1*/
	[1]=
	{
		.flags = IORESOURCE_IRQ,
		.start = IRQ_EINT1,
		.end = IRQ_EINT1,
		.name = "S3C24xx_EINT1",
	},
	/*EINT2*/
	[2]=
	{
		.flags = IORESOURCE_IRQ,
		.start = IRQ_EINT2,
		.end = IRQ_EINT2,
		.name = "S3C24xx_EINT2",
	},
	/*EINT4*/
	[3]=
	{
		.flags = IORESOURCE_IRQ,
		.start = IRQ_EINT4,
		.end = IRQ_EINT4,
		.name = "S3C24xx_EINT4",
	},
};

static struct platform_device tq2440_button_device=
{
	/*设备名*/
	.name = "tq2440_button",
	.id = -1,
	/*资源数*/
	
	.num_resources = ARRAY_SIZE(tq2440_button_resource),
	/*资源指针*/
	
	.resource = tq2440_button_resource,
};

static int __init tq2440_button_init(void)
{
	int ret ;
	/*设备注册*/
	ret = platform_device_register(&tq2440_button_device);
}

static void __exit tq2440_button_exit(void)
{
	/*设备的注销*/
	platform_device_unregister(&tq2440_button_device);
}

/*加载与卸载*/
module_init(tq2440_button_init);
module_exit(tq2440_button_exit);

/*LICENSE和作者信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GP-<gp19861112@yahoo.com.cn>");

