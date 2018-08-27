

ifeq ($(DEBUG), y)
	DEBFLAGS = -O -g
else
	DEBFLAGS = -O2
endif

EXTRA_FLAGS += $(DEBFLAGS)
EXTRA_FLAGS += -I..

ifneq ($(KERNELRELEASE),)

obj-m := sbull.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers *.sdtinfo.c *.sdtstub.S
	#make -C $(LINUX_KERNEL_PATH) M=$(PWD) clean 


depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
