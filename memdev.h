
/************************
 * *memdev.h
 * ************************/
#ifndef _MEMDEV_H_
#define _MEMDEV_H	//这用定义只是问了检查头文件是否被包含？如#ifndef _MEDEV_H_ #include<memdev.h> #endif

#ifndef MEMDEV_MAJOR
#define MEMDEV_MAJOR 260/*预设的mem的主设备号*/
#endif

#ifndef MEMDEV_NR_DEVS
#define MEMDEV_NR_DEVS 2/*设备数*/
#endif

#ifndef MEMDEV_SIZE
#define MEMDEV_SIZE 4096	//设备大小？
#endif

/*mem设备描述结构体*/
struct mem_dev 
{ 
	  char *data;			//数据
	  unsigned long size;		//长度
	  int value;			//一个值，用于测试
	  atomic_t count;		//读写引用计数


	  spinlock_t my_lock;		//自旋锁
	  struct work_struct my_work;	//工作队列
	  struct timer_list mytimer;	//定时器
	  struct delayed_work my_delayed_work;	//延后工作队列？
	  struct tasklet_struct mytasklet;	//下半部机制


};

#endif /* _MEMDEV_H_ */
