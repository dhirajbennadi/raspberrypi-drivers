#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>

MODULE_DESCRIPTION("custom cdev");
MODULE_AUTHOR("Dhiraj Bennadi");
MODULE_LICENSE("GPL");

#define MY_MAJOR 42
#define MY_MINOR 0
#define BUFFER_SIZE 4096
#define INIT_MESSAGE "Hello custom_cdev\n"

#define IOCTL_MESSAGE "Hello IOCTL"

#define MY_IOCTL_PRINT _IOC(_IOC_NONE, 'k' , 1 , 0)

/*Structure for cdev*/
struct custom_cdev_data{
        struct cdev cdev;
        char buffer[BUFFER_SIZE];
        size_t size;
};

struct custom_cdev_data dev;

/*File Ops*/
static int custom_cdev_open(struct inode *inode, struct file *file)
{
        struct custom_cdev_data *data = container_of(inode->i_cdev, struct custom_cdev_data, cdev);

        /*Access data from custom_cdev_data*/
        file->private_data = data;

        pr_info("custom cdev - open\n");
        schedule_timeout(10 * HZ);
        return 0;
}

static int custom_cdev_release(struct inode *inode, struct file *file)
{
        pr_info("custom cdev - release\n");
        return 0;
}

static ssize_t custom_cdev_read(struct file *file, char *user_buffer, size_t size, loff_t *offset)
{
        struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;
        pr_info("custom cdev read op\n");

        ssize_t to_read = min((size_t)data->size - (size_t)*offset, (size_t)size);

        if(to_read <= 0)
        {
                return 0;
        }

        if(copy_to_user(user_buffer, data->buffer + *offset, to_read))
        {
                return -EFAULT;
        }

        *offset += to_read;

        return to_read;
}

static ssize_t custom_cdev_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
        struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;
        pr_info("custom cdev write op\n");

        ssize_t to_write = min((size_t)data->size - (size_t)*offset, (size_t)size);

        if(to_write <= 0)
        {
                pr_info("No space on device\n");
        }

        if(copy_from_user(data->buffer + *offset, user_buffer, to_write))
        {
                pr_info("Copying failed\n");
                return -EFAULT;
        }

        *offset += to_write;

        return to_write;

}

static long custom_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        //struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;
        int ret = 0;

        switch(cmd)
        {
                case MY_IOCTL_PRINT:
                        pr_info("%s\n",IOCTL_MESSAGE);
                break;

                default:
                        ret = -EINVAL;
        }

        return ret;
}

static loff_t custom_cdev_llseek(struct file *file, loff_t offset, int whence)
{
        loff_t new_offset = 0;
        struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;

        switch(whence)
        {
                case SEEK_SET:
                        new_offset = offset;
                break;

                case SEEK_CUR:
                        new_offset = file->f_pos + offset;
                break;

                case SEEK_END:
                        new_offset = data->size + offset;
                break;

                default:
                        return -EINVAL;
        }

        if(new_offset < 0 || new_offset > data->size)
        {
                return -EINVAL;
        }

        file->f_pos = new_offset;

        return new_offset;
}

static const struct file_operations custom_cdev_ops = {
        .owner = THIS_MODULE,
        .open = custom_cdev_open,
        .release = custom_cdev_release,
        .read = custom_cdev_read,
        .write = custom_cdev_write,
        .unlocked_ioctl = custom_cdev_ioctl,
        .llseek = custom_cdev_llseek,
};

static int custom_cdev_init(void)
{
        int err;

        pr_info("Initializing custom cdev\n");
        err = register_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), 1, "cusotm_cdev");

        if(err != 0)
        {
                pr_info("custom cdev registeration failed");
                return err;
        }

        strncpy(&dev.buffer[0], INIT_MESSAGE, strlen(INIT_MESSAGE));
        /*Buffer size*/
        dev.size = BUFFER_SIZE;

        cdev_init(&dev.cdev, &custom_cdev_ops);
        err = cdev_add(&dev.cdev, MKDEV(MY_MAJOR, MY_MINOR) , 1);

        if(err)
        {
                pr_err("Failed to add cdev\n");
                unregister_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR) , 1);
        }

        pr_info("Initializing custom cdev - completed\n");
        return 0;
}

static void custom_cdev_exit(void)
{
    pr_info("Exiting custom cdev");
    cdev_del(&dev.cdev);

    unregister_chrdev_region(MKDEV(MY_MAJOR,MY_MINOR), 1);
}


module_init(custom_cdev_init);
module_exit(custom_cdev_exit);