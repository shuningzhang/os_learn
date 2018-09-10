
#ifndef __KERNEL__
    #define __KERNEL__
#endif

#ifndef MODULE
    #define MODULE
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/list.h>


struct list_head head;

struct test_data {
	int age;
	struct list_head n_list;
};

void walk_items(void)
{
	struct list_head *p, *n;
	struct test_data *tmp_data;

	list_for_each_safe(p, n, &head) {
		tmp_data = list_entry(p, struct test_data, n_list);
		printk(KERN_NOTICE "Walk Item: %d, Addr: %p\n", 
				tmp_data->age,
				&tmp_data->n_list);
		printk(KERN_NOTICE "Next: %p, Prev: %p \n", 
				tmp_data->n_list.next,
				tmp_data->n_list.prev);
	}
	
}

void release_items(void)
{
	struct list_head *p, *n;
	struct test_data *tmp_data;

	list_for_each_safe(p, n, &head) {
		tmp_data = list_entry(p, struct test_data, n_list);
		list_del(&tmp_data->n_list);
		printk(KERN_NOTICE "Free Item: %d\n", tmp_data->age);
		kfree(tmp_data);
	}
}

void break_item(int pos)
{
	struct list_head *p, *n;
	int i = 0;

	list_for_each_safe(p, n, &head) {
		if ( i == pos ) {
			INIT_LIST_HEAD(p);
		}
		i ++;
	}
}

static int test_init(void)
{
	int count = 5;
	int i = 0;
	struct test_data *tmp_data;
	
	INIT_LIST_HEAD(&head);

	/*初始化链表*/	
	for (i=0; i<count; i++) {

		tmp_data = kmalloc(sizeof(struct test_data), GFP_KERNEL);
		if (tmp_data == NULL) {
			goto alloc_error;
		}
		tmp_data->age = i;
		INIT_LIST_HEAD(&tmp_data->n_list);
		list_add_tail(&tmp_data->n_list, &head);
	}

	/*元素破坏测试*/
	break_item(2);	

	//
	
	tmp_data = kmalloc(sizeof(struct test_data), GFP_KERNEL);
	if (tmp_data == NULL) {
		goto alloc_error;
	}
	tmp_data->age = 8;
	INIT_LIST_HEAD(&tmp_data->n_list);
	list_add_tail(&tmp_data->n_list, &head);

	
	/*遍历元素*/
	walk_items();


alloc_error:
	/*如果中间出现分配内存失败，则释放之前分配的元素*/	

	return 0;
}

static void test_exit(void)
{
	release_items();
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunnyzhang<shuningzhang@126.com>");

