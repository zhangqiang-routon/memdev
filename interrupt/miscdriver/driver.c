#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/platform_device.h>
#include<linux/string.h>
#include<linux/miscdevice.h>
#include<mach/regs-gpio.h>
#include<linux/irq.h>
#include<linux/fs.h>
#include<mach/irqs.h>
#include<linux/interrupt.h>
#include<linux/types.h>
#include<linux/uaccess.h>
#include<linux/poll.h>

/*用来保存设备传递过来的资源的结构体*/
struct tq2440_button_irq  
{
    /*不采用指针，而是采用数组,避免指针的内存问题*/
	int  irqs[4];
	char *name[4];
};

/*资源结构体对象*/
static struct tq2440_button_irq tq2440_button_irq;

/*
键值，设置为volatile类型主要是因为键值可以被中断处理，
也就不再是简单的进程控制，导致变换
*/
static volatile int key_values;

/*采用等待队列宏创建等待队列头*/
static DECLARE_WAIT_QUEUE_HEAD(button_waitqueue);

/*用来标示被按下，也是一个中断处理函数可以操作的对象，因此需要添加volatile类型*/
static volatile int bepressed = 0;

/*中断处理函数*/
static irqreturn_t  tq2440_button_interrupt_hander(int irq,void *dev_id)
{
	int i;
	for(i = 0; i < 4; ++ i)
	{
		/*用来检测发生中断的中断号与具体中断号值的对比*/
		if(irq == tq2440_button_irq.irqs[i])
		{
			/*用来保存那个键被按下*/
			key_values = i;
			/*标示有按键按下*/
			bepressed = 1;
			
			/*唤醒等待队列，通常等待队列在poll和read,write中会使用，在中断中唤醒，可以避免死锁*/
			wake_up_interruptible(&button_waitqueue);
		}
	}
	
	/*返回中断函数被处理啦*/
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int tq2440_button_open(struct inode *inode,struct file *filp)
{
	int i = 0,ret = 0;
	/*申请中断，中断处理函数为tq2440_button_interrupt_hander
	   触发方式为双边触发
	   中断名
	   以及传递的数据
	*/
	for( i = 0; i < 4 ; ++ i)
	{
		ret = request_irq(tq2440_button_irq.irqs[i],tq2440_button_interrupt_hander,
			IRQ_TYPE_EDGE_BOTH,tq2440_button_irq.name[i],
			(void*)&tq2440_button_irq.irqs[i]);
		/*错误处理*/
		if(ret)
		{
			break;
		}
	}
	/*如果出错，就会把之前的申请号的中断号释放*/
	if(ret)
	{
		i--;
		for(; i >= 0 ; --i)
		{
			if(tq2440_button_irq.irqs[i] < 0)
				continue;
		}
		
		/*禁止该中断*/
		disable_irq(tq2440_button_irq.irqs[i]);
		/*释放中断*/
		free_irq(tq2440_button_irq.irqs[i],(void *)&tq2440_button_irq.irqs[i]);
		
		/*错误值返回*/
		return -EBUSY;
	}
	return 0;
}

static int tq2440_button_release(struct inode *inode,struct file *filp)
{
	int i = 0;
	
	/*释放中断号，以及相关的数据*/
	
	for(i =0; i< 4; ++ i)
		free_irq(tq2440_button_irq.irqs[i],(void *)&tq2440_button_irq.irqs[i]);

	return 0;
}

/*实现读操作*/
static unsigned long tq2440_button_read(struct file *filp,char __user *buff,size_t count,loff_t offp)
{
	unsigned long err;
	
	/*如果没有被按下，如果设置非阻塞方式读，则直接返回，如果阻塞模式则进入等待*/	
	
	if(!bepressed)
	{
		/*非堵塞方式读*/
		
		if(filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
			/*堵塞方式，等待队列，条件为bepressed是否为1，如果为1,则唤醒，否则睡眠*/
			
			wait_event_interruptible(button_waitqueue,bepressed);
	}
	
	/*唤醒后清除键被按下*/
	
	bepressed = 0;
	
	/*复制数据到用户空间*/
	
	//err = copy_to_user(buff,(void *)key_values,min(sizeof(key_values),count));
	err = copy_to_user(buff,&key_values,min(sizeof(key_values),count));

	/*返回读写的数据长度*/
	
	return err ? -EFAULT : min(sizeof(key_values),count);
}

/**/
static unsigned int tq2440_button_poll(struct file *filp,struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	
	/*将等待队列添加到poll_table_struct 中*/
	
	poll_wait(filp,&button_waitqueue,wait);
	
	/*
	根据条件返回掩码值，其中POLLIN，POLLRDNORM表示有数据可读
                                POLLOUT,POLLWRNORM表示数据可写
	*/
	if(bepressed)
		mask |= POLLIN|POLLRDNORM;

	/*返回掩码值*/
	return mask;
}

/*设备操作函数集合*/
static const struct file_operations filp_fops = 
{
        /*函数集合的拥有者*/
	.owner = THIS_MODULE,
	/*具体的操作函数*/
	.open = tq2440_button_open,
	.release = tq2440_button_release,
	.read = tq2440_button_read,
	.poll = tq2440_button_poll,
};

/*混杂字符设备驱动结构体对象初始化*/
static const struct miscdevice misc=
{
	/*设备驱动名，在/dev目录下的显示名*/
	.name = "tq2440_button",
	/*次设备号，这是混杂字符设备驱动识别的关键*/
	.minor = MISC_DYNAMIC_MINOR, 
	/*混杂字符设备驱动的具体操作函数集合*/
	.fops = &filp_fops,
};

/*
probe函数的具体实现,由于总线设备驱动模型中驱动中的peobe函数实质是
总线完成相关匹配操作以后，驱动的入口函数，因此可以probe函数中实现驱动初始化
*/
static int tq2440_button_probe(struct platform_device *dev)
{
	/*驱动得到设备的资源，完成混杂字符设备的注册*/
	int ret,i;
	/*资源结构体对象*/
	struct resource *plat_resource;
	/*实现间接的操作*/
	struct platform_device *pdev = dev;
	
	printk("The platform driver detect device can be used in platform bus\n");

	/*获得设备的具体资源，因为设备一个存在四个资源，所以i < 4*/
	for(i = 0; i < 4; ++ i)
	{
		/*获取第i个IORESOURCE_IRQ资源*/
		plat_resource = platform_get_resource(pdev,IORESOURCE_IRQ,i);
		/*如果没有获取成功*/
		if(plat_resource == NULL)
		{
			return -ENOENT;
		}
		/*保存得到的资源*/
		tq2440_button_irq.irqs[i] = plat_resource->start;
		/*保存具体的资源名*/
		tq2440_button_irq.name[i] = plat_resource->name;
	}	
	
	/*完成混杂字符设备驱动的注册操作*/
	ret = misc_register(&misc);
	/*错误处理*/
	if(ret)
		return ret;

	return 0;
}

/*
   remove函数的主要作用与probe函数的作用是相对的，probe是实现驱动的初始化，
   remove函数则是完成驱动的退出操作
*/
static int tq2440_button_remove(struct platform_device *dev)
{
	printk("Device was removed from the platform bus\n");

    /*释放注册的混杂字符设备驱动*/
	misc_deregister(&misc);

	return 0;
}

/*平台驱动的结构体对象定义*/
static struct platform_driver tq2440_button_driver=
{
	/*结构体的probe函数该函数是必须手动定义*/
	.probe = tq2440_button_probe,
	/*remove也是必须定义的函数*/
	.remove = tq2440_button_remove,
	/*设备驱动结构体填充*/
	.driver = 
	{
	    /*实现匹配的重要对象，驱动名*/
		.name = "tq2440_button",
		/*驱动拥有者*/
		.owner = THIS_MODULE,
	},
};

/*初始化函数，实现平台总线驱动的注册*/
static int __init tq2440_button_init(void)
{
	int ret ;
	/*注册平台总线驱动*/
	ret = platform_driver_register(&tq2440_button_driver);
	
	/*错误处理*/
	if(ret)
	{
		printk(KERN_NOTICE"The driver cannot register");
		platform_driver_unregister(&tq2440_button_driver);
	}
	return ret;
}

/*退出函数，实现平台总线驱动的注销*/
static void  __exit tq2440_button_exit(void)
{
	/*注销已经申请的平台总线驱动*/
	platform_driver_unregister(&tq2440_button_driver);
}

/*加载和卸载*/
module_init(tq2440_button_init);
module_exit(tq2440_button_exit);

/*LICENSE和作者信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GP-<gp19861112@yahoo.com.cn>");
