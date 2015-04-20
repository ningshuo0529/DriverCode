#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "scull.h"

extern struct file_operations scull_proc_ops;
int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;
struct scull_dev * scull_devices;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
MODULE_LICENSE("Dual BSD/GPL");

struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    //.llseek = scull_llseek,
    .read = scull_read,
    .write = scull_write,
    //.ioctl = scull_ioctl,
    .open = scull_open,
    .release = scull_release,
};

static struct scull_qset* scull_follow(struct scull_dev* dev, int item)
{
    struct scull_qset *qset = dev->data;
    if (!qset)
    {
        qset = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        dev->data = qset;
        if (!qset)
            return NULL;
        memset(qset, 0, sizeof(struct scull_qset));
    }
    while (item--)
    {
        if (!qset->next)
        {
            qset->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (!qset->next)
                return NULL;
            memset(qset->next, 0, sizeof(struct scull_qset));
        }
        qset = qset->next;
    }
    return qset;
}

ssize_t scull_write(struct file *filep, const char __user *buf, size_t count,loff_t *f_pos)
{
    struct scull_dev *dev = filep->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset= dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;

    ssize_t retval = -ENOMEM;

    //if (down_interruptible(&dev->sem))
    //    return -ERESTARTSYS;
    item = (long) *f_pos / itemsize;
    rest = (long) *f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;
    dptr = scull_follow(dev, item);
    if (dptr == NULL)
        goto out;
    if (!dptr->data)
    {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, sizeof(char*) * qset);
    }
    if (!dptr->data[s_pos])
    {
        dptr->data[s_pos] = kmalloc(sizeof(char) * quantum, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto out;
    }
    if (count > quantum - q_pos)
        count = quantum - q_pos;
    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count))
    {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;
    if (dev->size < *f_pos)
        dev->size = *f_pos;
    //printk(KERN_NOTICE "scull write %zd bytes", retval);
out:
    //up(&dev->sem);
    return retval;
}

ssize_t scull_read(struct file *filep, char __user *buf, size_t count,loff_t *f_pos)
{
    struct scull_dev *dev = filep->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset= dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;

    ssize_t retval = 0;

    //if (down_interruptible(&dev->sem))
    //    return -ERESTARTSYS;
    if (*f_pos >= dev->size)
        goto out;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    item = (long) *f_pos / itemsize;
    rest = (long) *f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;
    dptr = scull_follow(dev, item);
    if (dptr == NULL || !dptr->data || !dptr->data[s_pos])
        goto out;
    if (count > quantum - q_pos)
        count = quantum - q_pos;
    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
    {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;
    //printk(KERN_NOTICE "scull write %zd bytes", retval);
out:
    //up(&dev->sem);
    return retval;
}
int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;
    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;
    //printk(KERN_NOTICE "scull opened");

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        //printk(KERN_NOTICE "damn %u %u %u\n", filp->f_flags, O_WRONLY, O_ACCMODE);
        scull_trim(dev);
    }
    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    //printk(KERN_NOTICE "scull released");
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

    printk("init\n");
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
    proc_create("scullseq", 0, NULL, &scull_proc_ops);
    printk(KERN_ALERT "init done\n");
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
    remove_proc_entry("scullseq", NULL);
    unregister_chrdev_region(devno, scull_nr_devs);
}
module_init(scull_init_module);
module_exit(scull_cleanup_module);
