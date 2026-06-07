#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define BUF_SIZE 4096

typedef struct {
    char *buf;
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    struct mutex lock;
    wait_queue_head_t read_wait;
    wait_queue_head_t write_wait;
} ring_buffer;

static dev_t dev_num;
static struct cdev ring_cdev;
static struct class *dev_class;
static ring_buffer *ring_dev;

static int ring_open(struct inode *inode, struct file *filp)
{
    ring_dev->head = ring_dev->tail = ring_dev->count = 0;
    filp->private_data = ring_dev;
    pr_info("[DRV] device open, ringbuf reset\n");
    return 0;
}

static int ring_release(struct inode *inode, struct file *filp)
{
    pr_info("[DRV] device close\n");
    return 0;
}

static ssize_t ring_write(struct file *filp, const char __user *usr_buf, size_t len, loff_t *off)
{
    ring_buffer *dev = filp->private_data;
    int write_len, i, ret;

    mutex_lock(&dev->lock);
    wait_event_interruptible(dev->write_wait, dev->count < BUF_SIZE);
    write_len = (len > (BUF_SIZE - dev->count)) ? (BUF_SIZE - dev->count) : len;

    for(i = 0; i < write_len; i++){
        char tmp_ch;
        ret = copy_from_user(&tmp_ch, usr_buf + i, 1);
        dev->buf[dev->head] = tmp_ch;
        dev->head = (dev->head + 1) % BUF_SIZE;
        dev->count++;
    }

    mutex_unlock(&dev->lock);
    wake_up_interruptible(&dev->read_wait);
    return write_len;
}

static ssize_t ring_read(struct file *filp, char __user *usr_buf, size_t len, loff_t *off)
{
    ring_buffer *dev = filp->private_data;
    int read_len, i, ret;

    mutex_lock(&dev->lock);
    wait_event_interruptible(dev->read_wait, dev->count > 0);
    read_len = (len > dev->count) ? dev->count : len;

    for(i = 0; i < read_len; i++){
        char tmp_ch = dev->buf[dev->tail];
        ret = copy_to_user(usr_buf + i, &tmp_ch, 1);
        dev->tail = (dev->tail + 1) % BUF_SIZE;
        dev->count--;
    }

    mutex_unlock(&dev->lock);
    wake_up_interruptible(&dev->write_wait);
    return read_len;
}

static struct file_operations ring_fops = {
    .owner = THIS_MODULE,
    .open = ring_open,
    .release = ring_release,
    .write = ring_write,
    .read = ring_read,
};

static int __init ring_drv_init(void)
{
    int err;

    err = alloc_chrdev_region(&dev_num, 0, 1, "ringbuf_dev");
    if(err < 0){
        pr_err("[DRV] alloc dev number failed\n");
        return err;
    }

    cdev_init(&ring_cdev, &ring_fops);
    ring_cdev.owner = THIS_MODULE;
    err = cdev_add(&ring_cdev, dev_num, 1);
    if(err < 0){
        unregister_chrdev_region(dev_num,1);
        return err;
    }

    dev_class = class_create("ringbuf_class");
    if(IS_ERR(dev_class)){
        cdev_del(&ring_cdev);
        unregister_chrdev_region(dev_num,1);
        return PTR_ERR(dev_class);
    }

    device_create(dev_class, NULL, dev_num, NULL, "ringbuf0");

    ring_dev = kzalloc(sizeof(ring_buffer), GFP_KERNEL);
    ring_dev->buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    mutex_init(&ring_dev->lock);
    init_waitqueue_head(&ring_dev->read_wait);
    init_waitqueue_head(&ring_dev->write_wait);

    pr_info("[DRV] driver loaded ok, dev=/dev/ringbuf0\n");
    return 0;
}

static void __exit ring_drv_exit(void)
{
    kfree(ring_dev->buf);
    kfree(ring_dev);
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&ring_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("[DRV] driver unloaded ok\n");
}

module_init(ring_drv_init);
module_exit(ring_drv_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exp4-2 ringbuffer char driver 4KB");
