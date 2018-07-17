#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kobject.h>

struct my_kobj {
	int val;
	struct kobject kobj;
};

struct my_attribute {
	struct attribute attr;
	ssize_t (*show)(struct my_kobj *obj, 
			struct my_attribute *attr, char *buf);
	ssize_t (*store)(struct my_kobj *obj, 
			struct my_attribute *attr, const char *buf, size_t count);
};

struct my_kobj *obj;

static ssize_t my_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	struct my_attribute *my_attr;
	ssize_t ret = -EIO;

	my_attr = container_of(attr, struct my_attribute, attr);
	if (my_attr->show)
		ret = my_attr->show(container_of(kobj, struct my_kobj, kobj), 
				my_attr, buf);
	return ret;
}

static ssize_t my_attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count)
{
	struct my_attribute *my_attr;
	ssize_t ret = -EIO;

	my_attr = container_of(attr, struct my_attribute, attr);
	if (my_attr->store)
		ret = my_attr->store(container_of(kobj, struct my_kobj, kobj), 
				my_attr, buf, count);
	return ret;
}

const struct sysfs_ops my_sysfs_ops = {
	.show	= my_attr_show,
	.store	= my_attr_store,
};

void obj_release(struct kobject *kobj)
{
	struct my_kobj *obj = container_of(kobj, struct my_kobj, kobj);
	printk(KERN_INFO "obj_release %s\n", kobject_name(&obj->kobj));
	kfree(obj);
}

static struct kobj_type my_ktype = {
	.release	= obj_release,
	.sysfs_ops	= &my_sysfs_ops,
};

ssize_t name_show(struct my_kobj *obj, struct my_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%s\n", kobject_name(&obj->kobj));
}

ssize_t val_show(struct my_kobj *obj, struct my_attribute *attr, char *buffer)
{
	return sprintf(buffer, "%d\n", obj->val);
}

ssize_t val_store(struct my_kobj *obj, struct my_attribute *attr, 
		const char *buffer, size_t size)
{
	sscanf(buffer, "%d", &obj->val);

	return size;
}

struct my_attribute name_attribute = 
	__ATTR(name, 0444, name_show, NULL);
struct my_attribute val_attribute = 
	__ATTR(val, 0666, val_show, val_store);

struct attribute *my_attrs[] = {
	&name_attribute.attr, 
	&val_attribute.attr,
	NULL,
};

struct attribute_group my_group = {
	.name	= "mygroup",
	.attrs	= my_attrs,
};

static int __init mykobj_init(void)
{
	int retval;

	printk(KERN_INFO "mykobj_init\n");

	obj = kmalloc(sizeof(struct my_kobj), GFP_KERNEL);
	if (!obj) {
		return -ENOMEM;
	}
	obj->val = 1;

	memset(&obj->kobj, 0, sizeof(struct kobject));

	kobject_init_and_add(&obj->kobj, &my_ktype, NULL, "mykobj");

	retval = sysfs_create_files(&obj->kobj, 
			(const struct attribute **)my_attrs);
	if (retval) {
		kobject_put(&obj->kobj);
		return retval;
	}

	retval = sysfs_create_group(&obj->kobj, &my_group);
	if (retval) {
		kobject_put(&obj->kobj);
		return retval;
	}

	return 0;
}

static void __exit mykobj_exit(void)
{
	printk(KERN_INFO "mykobj_exit\n");

	kobject_del(&obj->kobj);
	kobject_put(&obj->kobj);

	return;
}

module_init(mykobj_init);
module_exit(mykobj_exit);

MODULE_LICENSE("GPL");

