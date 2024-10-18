#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/io.h>
/*---------------------------------------*/
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>

MODULE_DESCRIPTION("custom cdev");
MODULE_AUTHOR("Dhiraj");
MODULE_LICENSE("GPL");

#define MY_MAJOR 42
#define MY_MINOR 0
// 4 devices
#define NUM_MINORS 4
#define BUFFER_SIZE 4096
#define INIT_MESSAGE "Hello custom_cdev\n"

#define IOCTL_MESSAGE "Hello IOCTL"

#define MY_IOCTL_PRINT _IOC(_IOC_NONE, 'k' , 1 , 0)

/*GPIO Related*/
#define BCM2837_GPIO_ADDRESS 0x3F200000
#define BCM2711_GPIO_ADDRESS 0xfe200000
static unsigned int *gpio_registers = NULL;


static void gpio_pin_on(unsigned int pin)
{
	unsigned int fsel_index = pin/10;
	unsigned int fsel_bitpos = pin%10;
	unsigned int* gpio_fsel = gpio_registers + fsel_index;
	unsigned int* gpio_on_register = (unsigned int*)((char*)gpio_registers + 0x1c);

	*gpio_fsel &= ~(7 << (fsel_bitpos*3));
	*gpio_fsel |= (1 << (fsel_bitpos*3));
	*gpio_on_register |= (1 << pin);

	return;
}

static void gpio_pin_off(unsigned int pin)
{
	unsigned int *gpio_off_register = (unsigned int*)((char*)gpio_registers + 0x28);
	*gpio_off_register |= (1<<pin);
	return;
}


/*Structure for cdev*/
struct custom_cdev_data{
	struct cdev cdev;
	char buffer[BUFFER_SIZE];
	size_t size;
	atomic_t singleAccess;
};

struct custom_cdev_data dev[NUM_MINORS];

/*File Ops*/
static int custom_cdev_open(struct inode *inode, struct file *file)
{
	struct custom_cdev_data *data = container_of(inode->i_cdev, struct custom_cdev_data, cdev);

	/*Access data from custom_cdev_data*/
	file->private_data = data;

	if(atomic_cmpxchg(&data->singleAccess, 0, 1) != 0)
	{
		return -EBUSY;
	}

	pr_info("custom cdev - open\n");
	schedule_timeout(10 * HZ);
	return 0;
}

static int custom_cdev_release(struct inode *inode, struct file *file)
{
	struct custom_cdev_data *data = (struct custom_cdev_data *) file->private_data;

	atomic_set(&data->singleAccess, 0);

	pr_info("custom cdev - release\n");
	return 0;
}

static ssize_t custom_cdev_read(struct file *file, char *user_buffer, size_t size, loff_t *offset)
{
	struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;
	pr_info("Read Operation\n");

	pr_info("Read Bytes = %lu, Offset = %llu\n", size, *offset);
	
	pr_info("Reading user_buffer before read op\n");
	for(int i = 0; i < size; i++)
	{
		pr_info("%c",user_buffer[i]);
	}
	pr_info("\n");
				

	ssize_t to_read = min((size_t)data->size - (size_t)*offset, (size_t)size);
	
	if(to_read <= 0)
	{
		return 0;
	}
	else
	{
		pr_info("To read = %lu\n", to_read);
	}


	if(copy_to_user(user_buffer, data->buffer + *offset, to_read))
	{
		return -EFAULT;
	}

	*offset += to_read;


	pr_info("Reading user_buffer after read op\n");
	for(int i = 0; i < to_read; i++)
	{
		pr_info("%c",user_buffer[i]);
	}
	pr_info("\n");
	return to_read;
}

static ssize_t custom_cdev_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset)
{
	unsigned int pin = UINT_MAX;
	unsigned int value = UINT_MAX;
	struct custom_cdev_data *data = (struct custom_cdev_data *)file->private_data;
	pr_info("Write Operation\n");

	/*Set Buffer to zero before writing anything*/
	memset(data->buffer, 0 , BUFFER_SIZE);
	data->size = BUFFER_SIZE;

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

	if (sscanf(data->buffer, "%d,%d", &pin, &value) != 2)
	{
		printk("Inproper data format submitted\n");
		return size;
	}

	if (pin > 21 || pin < 0)
	{
		printk("Invalid pin number submitted\n");
		return size;
	}

	if (value != 0 && value != 1)
	{
		printk("Invalid on/off value\n");
		return size;
	}

	printk("You said pin %d, value %d\n", pin, value);
	
	if (value == 1)
	{
		gpio_pin_on(pin);
	} else if (value == 0)
	{
		gpio_pin_off(pin);
	}

	*offset += to_write;
	return to_write;

}

static long custom_cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
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
	// NUM_MINORS starting from 0
	err = register_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR), NUM_MINORS, "custom_gpio");

	if(err != 0)
	{	
		pr_info("DB- Registeration failed");
		return err;
	}

	/*Virtual Address Mapping*/
	gpio_registers = (int*)ioremap(BCM2837_GPIO_ADDRESS, PAGE_SIZE);
	if (gpio_registers == NULL)
	{
		printk("Failed to map GPIO memory to driver\n");
		return -1;
	}
	
	for(int i = 0; i < NUM_MINORS; i++)
	{
	  //strncpy(&dev[i].buffer[0], INIT_MESSAGE, strlen(INIT_MESSAGE));
	  memset(&dev[i].buffer[0], 0, BUFFER_SIZE);
      /*Buffer size*/
	  dev[i].size = BUFFER_SIZE;
	  /*Single Access*/
	  atomic_set(&dev[i].singleAccess, 0);
	  cdev_init(&dev[i].cdev, &custom_cdev_ops);
	  err = cdev_add(&dev[i].cdev, MKDEV(MY_MAJOR, MY_MINOR) , i);
	  
	  if(err)
	  {
		pr_err("Failed to add cdev\n");
		unregister_chrdev_region(MKDEV(MY_MAJOR, MY_MINOR) , i);
	  }
	}
	
	pr_info("Initializing custom cdev - completed\n");

	return 0;
}

static void custom_cdev_exit(void)
{
	pr_info("Exiting custom cdev");
	
	/*Virtual Address Unmap*/
	iounmap(gpio_registers);
	for(int i=0; i < NUM_MINORS; i++)
	{
	  cdev_del(&dev[i].cdev);
	}

    unregister_chrdev_region(MKDEV(MY_MAJOR,MY_MINOR), NUM_MINORS);
}


module_init(custom_cdev_init);
module_exit(custom_cdev_exit);
