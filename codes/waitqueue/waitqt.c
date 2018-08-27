
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

//#include <linux/slub_def.h>

static int condition = 0;
static struct task_struct *wait_task;
static struct task_struct *wakeup_task;

DECLARE_WAIT_QUEUE_HEAD(wq);

static int wait_task_thread(void *data)
{

	printk(KERN_NOTICE "wait task come in\n");
	wait_event_interruptible(wq, condition);

	condition = 0;

	printk(KERN_NOTICE "wait task passed\n");

	do {
	
		msleep(1000);
		printk(KERN_NOTICE "wait task running\n");
	}while(!kthread_should_stop());
	return 0;
}

static int wakeup_task_thread(void *data)
{
	do {
	
		msleep(5000);
		condition  = 1;
		wake_up_interruptible(&wq);
		printk(KERN_NOTICE "wakeup task running\n");
	}while(!kthread_should_stop());
	return 0;
}


static int test_init(void)
{

	wait_task = kthread_run(wait_task_thread, NULL, "thread%s", "wait");
	if ( IS_ERR(wait_task) ) {
		printk(KERN_NOTICE "Create thread error!\n");
	}
	
	wakeup_task = kthread_run(wakeup_task_thread, NULL, "thread%s", "wakeup");
	if ( IS_ERR(wakeup_task) ) {
		printk(KERN_NOTICE "Create wakeup thread error!\n");
	}

	printk(KERN_NOTICE "Module init!\n");
	return 0;
}

static void test_exit(void)
{
	int ret = 0;

	if ( !IS_ERR(wait_task) ) {
		ret = kthread_stop(wait_task);
		printk(KERN_NOTICE "Stop wait thread %d!", ret);
	}
	if ( !IS_ERR(wakeup_task) ) {
		ret = kthread_stop(wakeup_task);
		printk(KERN_NOTICE "Stop wakeup thread %d!", ret);
	}
	

	printk("Bye!\n");

}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunnyzhang<shuningzhang@126.com>");

