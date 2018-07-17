/*me: memdev.c********************************************************************
*Desc: 字符设备驱动程序的框架结构，该字符设备并不是一个真实的物理设备，
* 而是使用内存来模拟一个字符设备,用户空间可以直接读写设备文件，同时设备类下面有记录读写次数的属性文件
* *Parameter: 
* *Return:
* *Author: zhang
* *Date: 2017-11-25
* *Modify: 2017-11-25
* ********************************************************************************/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>

#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/rtc.h>


#include <linux/interrupt.h>

#include <linux/workqueue.h>

#include <linux/delay.h>

#include <linux/spinlock.h>


#include <linux/slab.h>



#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0)
#include <asm/switch_to.h>
#else
#include <asm/system.h>
#endif

#include <asm/atomic.h>

#include "memdev.h"


//#include <asm/system.h>
#include <asm/uaccess.h>


static int mem_major = MEMDEV_MAJOR;			//直接用宏不就可以吗？为什么要再定义一个变量？定义这个变量是可用可以在安装模块时带有参数指定主设备号

module_param(mem_major,int, S_IRUGO);			//用于向模块传入参数，mem_major为参数名，int为其类型，最后一个参数表示权限

struct mem_dev *mem_devp;				/*设备结构体指针*/

struct cdev cdev;					//一个 字符设备 结构体

struct class* memdev_class;
struct device* memdev_dev;
struct workqueue_struct *wq;
//结构体内数据初始化，其实也没什么用
void init_data(struct mem_dev *mp)
{
	mp->value=-1;
}
//显示当前时间
void prinktime(void)
{
	struct timex txc;
	struct rtc_time tm;
	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec,&tm);
	printk("current time is %d-%d-%d %d:%d:%d\n",tm.tm_year+1900,tm.tm_mon,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
}





//工作队列函数：将结构体mem_dev中的值加１
void work_func(struct work_struct * work)
{
	printk("work_func begin to running\n");
	prinktime();
	struct mem_dev* md = container_of(work,struct mem_dev,my_work);
	
	spin_lock(&md->my_lock);
	printk("work_func: now the value of the read&write count is %d\n",md->count);
	atomic_inc(&md->count);
	printk("work_func: after call the workqueue The value of read&wirte count is %d\n",md->count);
	spin_unlock(&md->my_lock);
}

/*int work_queue_init_func(void)
{
	struct my_data *md = NULL;
	struct my_data *md2 = NULL;
	md = init_data(md);
	md2 = init_data(md2);
	md->value = 10;
	md2->value = 20;
	
	INIT_WORK(&md->my_work,work_func);
	schedule_work(&md->my_work);

	wq=create_workqueue("testworkqueue");
	INIT_WORK(&md2->my_work,work_func);
	queue_work(wq,&md2->my_work);
	
	return 0;
}*/
//初始化工作队列
void init_my_workqueue(struct mem_dev* mp)
{
	INIT_WORK(&mp->my_work,work_func);
	wq=create_singlethread_workqueue("testworkqueue");

	
}

//开始调用工作队列，一行代码而已～～
void work_queue_begin(void)
{
	if(queue_work(wq,&mem_devp->my_work) == 0)
	{
		printk("工作队列调用失败!\n");
	}
	else
	{
		printk("工作队列调用成功!\n");
	}

}

//定时器的处理函数
void timer_func(unsigned long arg)
{

	printk("timer function is running \n");
	prinktime();

	work_queue_begin();
	//递归调用，实现重复打印效果
//	mem_devp->mytimer.expires = jiffies+100;
//	mem_devp->mytimer.function = timer_func;
//	add_timer(&mem_devp->mytimer);

}
//定时器初始化函数
bool init_mytimer(struct mem_dev *mp)
{
	printk("timer init function is running \n");
	mp->mytimer.expires = jiffies+3*HZ;
	mp->mytimer.data = 0;
        mp->mytimer.function = timer_func;
	
	init_timer(&mp->mytimer);
	return true;
}
//定时器退出
void exit_timer(void)
{
	del_timer(&mem_devp->mytimer);
	printk("timer is over\n");
}

//tasklet 处理函数
void tasklet_func(unsigned long data)
{
	printk("tasklet is running\n");
	schedule_delayed_work(&mem_devp->my_delayed_work,1*HZ);

}

//延后工作队列工作函数,因实现逻辑和工作队列的处理函数一致，故没再使用
void delay_work_func(struct work_struct *work)
{
	printk("WRITE delay work is running\n");
	struct mem_dev* md = container_of(work,struct mem_dev,my_work);
	spin_lock(&md->my_lock);
	atomic_inc(&md->count);
	spin_unlock(&md->my_lock);
	printk("after write the value of read&wtrie count  is %d",md->count);
}
//延后队列初始化
void init_my_delayed_work(struct mem_dev *mp)
{
	printk("delayed　work init\n ");
	INIT_DELAYED_WORK(&mp->my_delayed_work,work_func);
}
//延后队列退出
void exit_my_delayed_work(void)
{
	cancel_delayed_work_sync(&mem_devp->my_delayed_work);
	printk("delayed work has gone\n");

}
//读写计数的读取函数
ssize_t count_attr_show(struct device *dev,struct device_attribute *attr,char* buf)
{
	int val = 0;
	val = sprintf(buf,"%d\n",mem_devp->count);
	return val;
}
//读写计数属性文件
struct device_attribute dev_attr_test ={
	.attr={
		.name = "count",
		.mode = 0444,
	      },
	.show = count_attr_show,
};



/*文件打开函数*/
int mem_open(struct inode*inode, struct file *filp)
{
	    struct mem_dev *dev;			
	     
	       /*获取次设备号*/
	    int num = MINOR(inode->i_rdev);
	    if (num>= MEMDEV_NR_DEVS)		//若设备个数大于2个，返回错误？
		return -ENODEV;
	
	    dev = &mem_devp[num];
			    
		/*将设备描述结构指针赋值给文件私有数据指针*/
	   filp->private_data= dev;
			        
		return 0;
}

/*文件释放函数*/
int mem_release(struct inode*inode, struct file *filp)
{
	  return 0;
}

/*读函数*/

static ssize_t mem_read(struct file *filp,char __user *buf,size_t size, loff_t*ppos)
{
	  unsigned long p= *ppos;
	  unsigned int count = size;
	  int ret = 0;
	  struct mem_dev *dev= filp->private_data;//获得设备结构体指针

		  //判断读位置是否有效
	  
	  spin_lock(&dev->my_lock);
	  if (p >= MEMDEV_SIZE)					//若读取位置超限则结束
		return 0;
	  if (count> MEMDEV_SIZE - p)
		count = MEMDEV_SIZE- p;			//若要 读取的个数 大于剩余个数对 读取个数 进行调整
		      //读数据到用户空间
	  if (copy_to_user(buf,(void*)(dev->data+ p),count))	//将打开文件从ppos位置复制size个数据位到buf
	  {
		ret = - EFAULT;
	  }
	  else
	  {
		*ppos +=count;						//？？？？
		ret = count;
						      
		printk(KERN_INFO "read %d bytes(s) from %d\n", count, p);
	  }

	 spin_unlock(&dev->my_lock);
	 printk("call the timer here\n");
	 prinktime();
	 add_timer(&mem_devp->mytimer);
	 

		return ret;
}
/*
static ssize_t mem_read(struct file *file,char __user *buff,size_t size,loff_t *ppos)
{
	unsigned long p =0;
	unsigned int count = sizeof(int);
	int ret =0;
	struct mem_dev *dev = file->private_data;

	add_timer(&mem_devp->mytimer);
	
	spin_lock(&dev->my_lock);

	printk("mem_read the value of memdev struct is %d \n",dev->value);

	if(copy_to_user(buff,(void*)(dev->value+p),count))
	{
		printk("向用户ｂｕｆｆ复制失败!\n");
		ret = -EFAULT;
	}
	
	spin_unlock(&dev->my_lock);
	return ret;

}*/


/*写函数，和读函数类似*/
static ssize_t mem_write(struct file *filp,const char __user*buf, size_t size, loff_t *ppos)
{
	  unsigned long p= *ppos;
	  unsigned int count = size;
	  int ret = 0;
	  struct mem_dev *dev= filp->private_data;/*获得设备结构体指针*/
		  
		  /*分析和获取有效的写长度*/
	  spin_lock(&dev->my_lock);
	  if (p >= MEMDEV_SIZE)
		return 0;
	  if (count> MEMDEV_SIZE - p)
		count = MEMDEV_SIZE- p;
		        
		      /*从用户空间写入数据*/
          if (copy_from_user(dev->data+ p, buf,count))
		ret = - EFAULT;
	  else
	  {
          	*ppos +=count;
		ret = count;
						      
		printk(KERN_INFO "written %d bytes(s) from %d \n", count, p);
	  }
	
	  spin_unlock(&dev->my_lock);
	  printk("bengin call the tasklet \n");
	  prinktime();
	  tasklet_schedule(&mem_devp->mytasklet);
  	  return ret;
}

// seek文件定位函数 
static loff_t mem_llseek(struct file *filp, loff_t offset,int whence)
{ 
	    loff_t newpos;

	        switch(whence){
			        case 0:/* SEEK_SET */
				    newpos = offset;
				break;

				case 1:/* SEEK_CUR */
					newpos = filp->f_pos+ offset;
				break;

				case 2:/* SEEK_END */
					newpos = MEMDEV_SIZE -1 + offset;
				break;

				default:/* can't happen */
				return -EINVAL;
				}

		    if ((newpos<0)|| (newpos>MEMDEV_SIZE))
			         return -EINVAL;
		         
		        filp->f_pos= newpos;
			    return newpos;

}

/*文件操作结构体*/
static const struct file_operations mem_fops =
{
	  .owner = THIS_MODULE,			//所有者，标示模块是否在使用
	  .llseek = mem_llseek,			//指定文件定位函数为mem_llseek
	  .read = mem_read,			//指定读取函数为mem_red
	  .write = mem_write,			//指定写入函数为mem_write
	  .open = mem_open,			//指定打开函数为mem_open
	  .release = mem_release,		//指定释放函数为mem_release
};

/*设备驱动模块加载函数*/
static int memdev_init(void)
{
	  int result;
	  int i;

	  dev_t devno = MKDEV(mem_major, 0);

	  /* 静态申请设备号*/
	   if (mem_major)
		 result = register_chrdev_region(devno, 2,"memdev");
	   else /* 动态分配设备号 */
		{
			result = alloc_chrdev_region(&devno, 0, 2,"memdev");
			mem_major = MAJOR(devno);
		} 
		    
	   if (result< 0)
		return result;			//若申请设备号失败则返回结果值（为一负值）

	/*初始化cdev结构*/
	   cdev_init(&cdev,&mem_fops);		
	   cdev.owner = THIS_MODULE;
	   cdev.ops =&mem_fops;
			    
	/* 注册字符设备 */
	   cdev_add(&cdev, MKDEV(mem_major, 0), MEMDEV_NR_DEVS);

	   memdev_class = class_create(THIS_MODULE,"mymemdev");
	   memdev_dev = device_create(memdev_class,NULL,devno,NULL,"memdev%d",0);
		
	   device_create_file(memdev_dev,&dev_attr_test);


	/* 为设备描述结构分配内存*/
	   mem_devp = kmalloc(MEMDEV_NR_DEVS* sizeof(struct mem_dev), GFP_KERNEL);	//申请内存，用于存放自定义的mem_dev结构体
	   if (!mem_devp)/*申请失败*/
	 	{
			result = - ENOMEM;
			goto fail_malloc;
		}
	   memset(mem_devp, 0,sizeof(struct mem_dev));					//将申请得到的内存区域初始化（全部置零）
				    
	/*为设备分配内存*/
	   for (i=0; i< MEMDEV_NR_DEVS; i++)
		{
			mem_devp[i].size= MEMDEV_SIZE;				//长度为4096
			mem_devp[i].data= kmalloc(MEMDEV_SIZE, GFP_KERNEL);	//申请内核内存，大小为4096,；返回申请到内存区的首地址
			memset(mem_devp[i].data, 0, MEMDEV_SIZE);		//置零申请到的内存区	
		
			init_my_workqueue(&mem_devp[i]);
			atomic_set(&mem_devp[i].count, 0);
		        printk("init the read&write cout to 0\n");
			tasklet_init(&mem_devp[i].mytasklet,tasklet_func,0);
  		        printk("tasklet_init has runned!\n");
			init_data(&mem_devp[i]);
			init_my_delayed_work(&mem_devp[i]);
			spin_lock_init(&mem_devp[i].my_lock);
			init_mytimer(&mem_devp[i]);
		}
				  
	return 0;

	fail_malloc: 
			unregister_chrdev_region(devno, 1);			//若申请失败则释放设备号
					  
	return result;
}

/*模块卸载函数*/
static void memdev_exit(void)
{
	if(wq != NULL)
	{
	destroy_workqueue(wq);
	}
	wq=NULL;

	exit_my_delayed_work();
	tasklet_kill(&mem_devp->mytasklet);
	exit_timer();
	
	cdev_del(&cdev);/*注销设备*/
	kfree(mem_devp);/*释放设备结构体内存*/
	unregister_chrdev_region(MKDEV(mem_major, 0), 2);/*释放设备号*/
	device_unregister(memdev_dev);
	class_destroy(memdev_class);
	
	printk(KERN_ALERT "mem_dev is gone\n");
	
}
//同名设备添加进总线后调用该函数
int mem_probe(struct platform_device *dev)
{
	printk("mem_probe is running\n");
	memdev_init();
	return 0;
}
//移除
int mem_remove(struct platform_device *dev)
{
	printk("mem_remove is running\n");
	memdev_exit();
	return 0;
}

//指定平台设备驱动
struct platform_driver mem_driver={
	.probe = mem_probe,
	.remove = mem_remove,
	.driver={
		.name = "memdev" ,
		.owner = THIS_MODULE,
	},
	
};
//模块初始化
int mem_drv_init(void)
{
	printk("mem_drv_init is running\n");
	platform_driver_register(&mem_driver);
	return 0;
}

//模块退出
int mem_drv_exit(void)
{
	printk("mem_drv_exit is running\n");
	platform_driver_unregister(&mem_driver);
	return 0;
}


MODULE_AUTHOR("zhang");
MODULE_LICENSE("GPL");

module_init(mem_drv_init);
module_exit(mem_drv_exit);
