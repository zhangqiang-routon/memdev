#include<linux/types.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/platform_device.h>
#include<mach/irqs.h>
#include<linux/irq.h>
#include<mach/regs-gpio.h>
#include<linux/device.h>
#include<linux/string.h>
#include<linux/cdev.h>
#include<linux/fs.h>
#include<linux/spinlock.h>
#include<linux/wait.h>
#include<linux/interrupt.h>
#include<linux/uaccess.h>
#include<linux/poll.h>


#define NUM_RESOURCE	4

/*主设备号*/
int dev_major = -1;

/*中断结构体定义*/
struct irqs
{
	int  pirqs[NUM_RESOURCE];
	char *names[NUM_RESOURCE];
}irqs;
/*完成具体设备的结构体设计*/
struct tq2440_button
{
	/*添加具体的字符设备结构*/
	struct cdev cdev;
	/*用于自动创建设备*/
	struct class *myclass;
	/*引用次数统计表*/
	unsigned int count;
	/*添加并行机制*/
	spinlock_t lock;
	/*添加等待队列*/
	wait_queue_head_t read_wait_queue;
	/*数据*/
	int bepressed;
	/*案件值*/
	int key_values;
};

static struct tq2440_button tq2440_button;

static irqreturn_t tq2440_button_interrupt_handler(int irq,void *dev_id)
{
	/*得到传递过来的参数*/
	struct tq2440_button * dev = dev_id;
	int i;
	/*根据得到的irq值确定具体的按键值*/
	for (i = 0; i < NUM_RESOURCE; ++ i)
	{
		/*判断条件*/
		if(irq == irqs.pirqs[i])
		{
			/*对关键数据添加并行机制*/
			spin_lock(&(dev->lock));
			/*确定被按下的值*/
			dev->key_values = i ;
			/*表示有数据可以读*/
			dev->bepressed = 1;
			spin_unlock(&(dev->lock));
			/*唤醒等待的队列*/
			wake_up_interruptible(&(dev->read_wait_queue));	
		}
	}
	/*返回值*/
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int tq2440_button_open(struct inode *inode,struct file *filp)
{
	int i = 0,ret = 0;
	
	/*中断申请*/
	/*这句话主要是实现间接控制，但是还是可以直接控制*/
	filp->private_data = &tq2440_button;
	
	/*修改被打开的次数值*/
	spin_lock(&(tq2440_button.lock));	
	tq2440_button.count ++ ;
	spin_unlock(&(tq2440_button.lock));

	/*如果是第一次打开则需要申请中断*/
	if(1==tq2440_button.count)
	{
		for(i = 0;i < NUM_RESOURCE; ++ i)
		{
			/*request_irq操作*/
			ret = request_irq(irqs.pirqs[i],
				tq2440_button_interrupt_handler,
				IRQ_TYPE_EDGE_BOTH,irqs.names[i],(void *)&tq2440_button);
		
			if(ret)
			{
				break;
			}
		}
		if(ret)/*错误处理机制*/
		{
			i --;
			for(; i >=0; --i)
			{
				/*禁止中断*/
				
				disable_irq(irqs.pirqs[i]);
			
				free_irq(irqs.pirqs[i],(void *)&tq2440_button);
			}
			return -EBUSY;
		}
	}	
	return 0;
}

/*closed函数*/
static int tq2440_button_close(struct inode *inode,struct file *filp)
{
	/*确保是最后一次使用设备*/
	int i = 0;
	if(tq2440_button.count == 1)
	{
		for(i = 0; i < NUM_RESOURCE; ++ i)
		{
			free_irq(irqs.pirqs[i],(void *)&tq2440_button);
		}
	}
	/*更新设备文件引用次数*/
	spin_lock(&(tq2440_button.lock));
	tq2440_button.count = 0;
	spin_unlock(&(tq2440_button.lock));

	return 0;
}

static unsigned long tq2440_button_read(struct file *filp,char __user *buff,
					size_t count,loff_t offp)
{
	/*设备操作*/
	struct tq2440_button *dev = filp->private_data;
	
	unsigned long err;
		if(!dev->bepressed)/*确保没有采用非堵塞方式读标志*/
		{
		if(filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else
			/*添加等待队列，条件是bepressed*/
			wait_event_interruptible(dev->read_wait_queue,dev->bepressed);
	}	

	/*复制数据到用户空间*/
	err = copy_to_user(buff, &(dev->key_values),min(sizeof(dev->key_values),count));
	
	/*修改标志表示没有数据可读了*/
	spin_lock(&(dev->lock));
	dev->bepressed = 0;
	spin_unlock(&(dev->lock));
	
	/*返回数据量*/
	
	return err ? -EFAULT:min(sizeof(dev->key_values),count);
}


static unsigned int tq2440_button_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	struct tq2440_button *dev = filp->private_data;

	unsigned int mask = 0;
	
	/*将结构体中的等待队列添加到wait_table*/	
	poll_wait(filp,&(dev->read_wait_queue),wait);

	/*
	返回掩码
	POLLIN|POLLRDNORM表示有数据可读
	*/
	if(dev->bepressed)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

/*设备的具体操作函数*/
static const struct file_operations tq2440_fops=
{
	.owner = THIS_MODULE,
	.open = tq2440_button_open,
	.release = tq2440_button_close,
	.read = tq2440_button_read,
	.poll = tq2440_button_poll,
};


/*remove函数实现字符设备的注销操作*/

static int tq2440_button_probe(struct platform_device *dev)
{
	printk("The driver found a device can be handler on platform bus\n");
	
	/*用来存储定义好的资源，即中断号*/
	
	struct resource * irq_resource;
	struct platform_device *pdev = dev;
	int i = 0,ret = 0;

	/*接下来完成具体字符驱动结构体的初始化*/
	/*1、设备号申请*/
	
	dev_t devno;

	if(dev_major > 0)/*静态申请设备号*/
	{
		devno = MKDEV(dev_major,0);
		ret = register_chrdev_region(devno,1,"tq2440_button");
	}
	else/*动态申请设备号*/
	{
		ret = alloc_chrdev_region(&devno,0,1,"tq2440_button");
		dev_major = MAJOR(devno);
	}
	if(ret < 0)
	{
		return  ret;
	}

	/*完成设备类的创建，主要实现设备文件的自动创建*/
	tq2440_button.myclass = class_create(THIS_MODULE,"tq2440_button_class");
 
	/*2、完成字符设备的加载*/
	cdev_init(&(tq2440_button.cdev),&tq2440_fops);
	tq2440_button.cdev.owner = THIS_MODULE;
	ret = cdev_add(&(tq2440_button.cdev),devno,1);	
	if(ret)
	{
		printk("Add device error\n");
		return ret;
	}
	
		/*初始化自旋锁*/
	spin_lock_init(&(tq2440_button.lock));
	
	/*修改引用次数值*/
	
	spin_lock(&(tq2440_button.lock));
	/*被打开次数统计*/
	tq2440_button.count = 0;	
	/*键值*/
	tq2440_button.key_values = -1;
	spin_unlock(&(tq2440_button.lock));
	
	/*初始化等待队列*/
	
	init_waitqueue_head(&(tq2440_button.read_wait_queue));
	
	
	/*设备的创建，实现设备文件自动创建*/
	
	device_create(tq2440_button.myclass,NULL,devno,NULL,"tq2440_button");

	/*3.获得资源*/
	for(; i < NUM_RESOURCE; ++ i)
	{
		/*获得设备的资源*/
		irq_resource = platform_get_resource(pdev,IORESOURCE_IRQ,i);
		
		if(NULL == irq_resource)
		{
			return -ENOENT;
		}
		irqs.pirqs[i] = irq_resource->start;

		/*实现名字的复制操作*/
		//strcpy(tq2440_irqs.name[i],irq_resource->name);

		/*这一句是将指针的地址指向一个具体的地址*/
		irqs.names[i] = irq_resource->name;
	}
	/*将设备的指针指向中断号*/
	
	return 0;
}

/*probe函数实现字符设备的初始化操作*/
static int tq2440_button_remove(struct platform_device *dev)
{
	printk("The driver found a device be removed from the platform bus\n");
	
	/*注销设备*/
	device_destroy(tq2440_button.myclass,MKDEV(dev_major,0));
	/*字符设备注销*/
	cdev_del(&(tq2440_button.cdev));
	/*注销创建的设备类*/
	class_destroy(&(tq2440_button.myclass));
	/*释放设备号*/
	unregister_chrdev_region(MKDEV(dev_major,0),1);
	return 0;
}

/*完成平台总线结构体的设计*/
static const struct platform_driver tq2440_button_driver = 
{
	/*完成具体设备的初始化操作*/
	.probe = tq2440_button_probe,
	/*完成具体设备的退出操作*/
	.remove = tq2440_button_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "tq2440_button",
	},
};

/*总线设备初始化过程*/
static int __init platform_driver_init(void)
{
	int ret;
	
	/*总线驱动注册*/
	ret = platform_driver_register(&tq2440_button_driver);
	
	/*错误处理*/
	if(ret)
	{
		platform_driver_unregister(&tq2440_button_driver);
		return ret;
	}

	return 0;
}

static void __exit platform_driver_exit(void)
{
	/*总线驱动释放*/
	platform_driver_unregister(&tq2440_button_driver);
}

/*加载和卸载*/
module_init(platform_driver_init);
module_exit(platform_driver_exit);

/*LICENSE和作者信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GP-<gp19861112@yahoo.com.cn>");
