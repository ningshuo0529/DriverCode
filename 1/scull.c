#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include "scull.h"

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev * scull_devices;

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    //.llseek = scull_llseek,
    //.read = scull_read,
    //.write = scull_write,
    //.ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
};

int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;
    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        scull_trim(dev);
    }
    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    return 0;
}

int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next, *dptr;
    int qset = dev->qset;
    int i;
    for (dptr = dev->data; dptr; dptr = next)
    {
        if (dptr->data)
        {
            for (i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int __init scull_init_module(void)
{
    int result, i;

    dev_t dev = 0;

    if (scull_major)
    {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, "scull");
    }
    else
    {
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
        scull_major = MAJOR(dev);
    }
    if (result < 0)
    {
        printk(KERN_WARNING "scull: can't get major %d \n", scull_major);
        return result;
    }
    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
    if (!scull_devices)
    {
        printk(KERN_WARNING "scull: can't get devices mem");
        return ENOMEM;
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));
    for (i = 0; i < scull_nr_devs; i++)
    {
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        scull_setup_cdev(scull_devices + i, i);
    }
    return 0;
}

static void __exit scull_cleanup_module(void)
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

    if (scull_devices)
    {
        for (i = 0; i < scull_nr_devs; i++)
        {
            scull_trim(scull_devices + i);
            cdev_del(&scull_devices[i].cdev);
        }
    }
    unregister_chrdev_region(devno, scull_nr_devs);
}
module_init(scull_init_module);
module_exit(scull_cleanup_module);
