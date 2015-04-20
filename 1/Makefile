obj-m := scull.o
scull-objs := stat.o scull_register.o
KERNELDIR :=/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	make -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.o *~core .depend .*.cmd *.ko *.mod.c .tmp_versions Module* modules*
