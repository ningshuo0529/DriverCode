#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/cdev.h>

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4    /* scull0 ~ scull3 */
#endif

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET  1000
#endif

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

struct scull_qset
{
    void **data;
    struct scull_qset *next;
};

struct scull_dev {
    struct scull_qset *data;
    int quantum;
    int qset;
    unsigned long size;
    unsigned long access_key;
    struct semaphore sem;
    struct cdev cdev;
};

ssize_t scull_read(struct file *filep,char __user *buf, size_t count,loff_t *offp);
ssize_t scull_write(struct file *filep, const char __user *buff, size_t count,loff_t *offp);
int scull_trim(struct scull_dev *dev);
int scull_release(struct inode *inode, struct file *filp);
int scull_open(struct inode *inode, struct file *filp);

#endif
