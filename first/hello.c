
#ifndef __KERNEL__
    #define __KERNEL__
#endif

#ifndef MODULE
    #define MODULE
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int hello_init(void)
{
	printk("Hello, shuningz\n");
	return 0;
}

static void hello_exit(void)
{
	printk("Bye!\n");

}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunnyzhang<shuningzhang@126.com>");

