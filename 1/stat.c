#include <linux/seq_file.h>
#include <linux/fs.h>
#include "scull.h"

static void *scull_seq_start(struct seq_file* sfile, loff_t *pos)
{
    if (*pos >= scull_nr_devs)
        return NULL;
    return scull_devices + *pos;
}

static void *scull_seq_next(struct seq_file* sfile, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= scull_nr_devs)
        return NULL;
    return scull_devices + *pos;
}


static void scull_seq_stop(struct seq_file* sfile, void *v)
{
}

static int scull_seq_show(struct seq_file *sfile, void *v)
{
    struct scull_dev *dev = (struct scull_dev *) v;
    struct scull_qset *d;
    int i;
    //if (diwn_interruptible(&dev-sem))
    //    return -ERESTARTSYS;
    seq_printf(sfile, "\nDevice %i: qset %d, q %i, sz %li\n",
              (int) (dev - scull_devices), dev->qset, dev->quantum, dev->size);
    for (d = dev->data; d; d = d->next)
    {
        seq_printf(sfile, " item at %p, qset at %p\n", d, d->data);
        if (d->data && !d->next)
        {
            for (i = 0; i < dev->qset; i++)
            {
                if (d->data[i])
                    seq_printf(sfile, " %4i: %8p\n", i, d->data[i]);
            }
        }
    }
    //up(&dev->sem);
    return 0;
}

static struct seq_operations scull_seq_ops =
{
    .start = scull_seq_start,
    .next = scull_seq_next,
    .stop = scull_seq_stop,
    .show = scull_seq_show,
};

static int scull_proc_open(struct inode* inode, struct file *file)
{
    return seq_open(file, &scull_seq_ops);
}

struct file_operations scull_proc_ops =
{
    .owner = THIS_MODULE,
    .open = scull_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};
