/*
 * PSO - Memory Mapping Lab(#11)
 *
 * Exercise #1: memory mapping using kmalloc'd kernel areas
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/highmem.h>
#include <linux/rmap.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>


MODULE_DESCRIPTION("simple mmap driver");
MODULE_AUTHOR("PSO");
MODULE_LICENSE("Dual BSD/GPL");

#define MY_MAJOR        42
/* how many pages do we actually kmalloc */
#define NPAGES          16

#define PROC_ENTRY_NAME "my-proc-entry"

/* character device basic structure */
static struct cdev mmap_cdev;

/* pointer to kmalloc'd area */
static void *kmalloc_ptr;

/* pointer to the kmalloc'd area, rounded up to a page boundary */
static char *kmalloc_area;

static int my_open(struct inode *inode, struct file *filp)
{
        return 0;
}

static int my_release(struct inode *inode, struct file *filp)
{
        return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer,
                size_t size, loff_t *offset)
{
        /* TODO 2: check size doesn't exceed our mapped area size */
        size_t total_mapped_size = NPAGES * PAGE_SIZE;

        if(*offset >= total_mapped_size)
        {
                return 0;
        }

        if(size + *offset > total_mapped_size)
        {
                size = total_mapped_size - *offset;
        }

        /* TODO 2: copy from mapped area to user buffer */
        if(copy_to_user(user_buffer, kmalloc_area + *offset, size))
        {
                printk(KERN_ERR "Failed to copy data to user space\n");
                return -EFAULT;
        }

        *offset += size;

        return size;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer,
                size_t size, loff_t *offset)
{
        /* TODO 2: check size doesn't exceed our mapped area size */
        size_t total_mapped_size = NPAGES * PAGE_SIZE;

        if(*offset >= total_mapped_size)
        {
                return 0;
        }

        if(size + *offset > total_mapped_size)
        {
                size = total_mapped_size - *offset;
        }

        /* TODO 2: copy from user buffer to mapped area */
        if(copy_from_user(kmalloc_area + *offset, user_buffer, size))
        {
                printk(KERN_ERR "Failed to copy data from user space\n");
                return -EFAULT;
        }

        *offset += size;


        return size;
}

static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
        int ret;
        unsigned long pfn;
        long length = vma->vm_end - vma->vm_start;

        /* do not map more than we can */
        if (length > NPAGES * PAGE_SIZE)
                return -EIO;

        /* TODO 1: map the whole physically contiguous area in one piece */
        phys_addr_t phys_addr = virt_to_phys(kmalloc_area);

        pfn = phys_addr >> PAGE_SHIFT;

        // Map the physical memory to user space
        if (remap_pfn_range(vma, vma->vm_start, pfn, length, vma->vm_page_prot)) {
            printk("Failed to remap memory to user space\n");
            return -EAGAIN;
        }

        printk("Memory successfully mapped to user space\n");

        ret = 0;
        return ret;
}

static const struct file_operations mmap_fops = {
        .owner = THIS_MODULE,
        .open = my_open,
        .release = my_release,
        .mmap = my_mmap,
        .read = my_read,
        .write = my_write
};

static int my_seq_show(struct seq_file *seq, void *v)
{
        struct mm_struct *mm;
        //struct vm_area_struct *vma_iterator;
        //unsigned long total = 0;

        /* TODO 3: Get current process' mm_struct */
        mm = get_task_mm(current);
        if(!mm)
        {
                printk("No memory management structure found\n");
                return 0;
        }

        // for_each_vma(vma_iterator, mm) 
        // {
        //         unsigned long size = vma_iterator->vm_end - vma_iterator->vm_start;
        //         total += size;
        //         //printk("%lx %lx\n", vma_iterator->vm_start, vma_iterator->vm_end);
        // }

        // /* TODO 3: Iterate through all memory mappings */
        // for(vma_iterator = mm->mmap; vma_iterator != NULL; vma_iterator = vma_iterator->vm_next)
        // {
        //         unsigned long size = vma_iterator->vm_end - vma_iterator->vm_start;
        //         total += size;
        //         printk("%lx %lx\n", vma_iterator->vm_start, vma_iterator->vm_end);
        // }

        /* TODO 3: Release mm_struct */
        mmput(mm);

        /* TODO 3: write the total count to file  */
        //seq_printf(seq, "%lu", total);
        return 0;
}

static int my_seq_open(struct inode *inode, struct file *file)
{
        /* TODO 3: Register the display function */
        return single_open(file, my_seq_show, NULL);
}

static const struct proc_ops my_proc_ops = {
        .proc_open    = my_seq_open,
        .proc_read    = seq_read,
        .proc_lseek   = seq_lseek,
        .proc_release = single_release,
};

static int __init my_init(void)
{
        int ret = 0;
        int i;
        /* TODO 3: create a new entry in procfs */
        proc_create(PROC_ENTRY_NAME, 0, NULL, &my_proc_ops);

        ret = register_chrdev_region(MKDEV(MY_MAJOR, 0), 1, "mymap");
        if (ret < 0) {
                printk("could not register region\n");
                goto out_no_chrdev;
        }

        /* TODO 1: allocate NPAGES+2 pages using kmalloc */
        kmalloc_ptr = kmalloc((NPAGES+2) * PAGE_SIZE, GFP_KERNEL);

        if(!kmalloc_ptr)
        {
            printk("Memory could not be allocated\n");
            return -1;
        }

        /* TODO 1: round kmalloc_ptr to nearest page start address */
        kmalloc_area = (void *)ALIGN((uintptr_t)kmalloc_ptr, PAGE_SIZE);

        /* TODO 1: mark pages as reserved */
        for(i = 0; i < (NPAGES+2); i++)
        {
            struct page *page = virt_to_page((uintptr_t)(kmalloc_area + (i*PAGE_SIZE)));
            SetPageReserved(page);
            printk("Setting Page = %d at Memory %lu\n", i, (unsigned long)((uintptr_t)kmalloc_area + (i*PAGE_SIZE)));
        }

        /* TODO 1: write data in each page */
        char data_to_write[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        for(i = 0; i < (NPAGES+2); i++)
        {
            memcpy(kmalloc_area + (i*PAGE_SIZE), data_to_write, sizeof(data_to_write));
        }

        /* Init device. */
        cdev_init(&mmap_cdev, &mmap_fops);
        ret = cdev_add(&mmap_cdev, MKDEV(MY_MAJOR, 0), 1);
        if (ret < 0) {
                pr_err("could not add device\n");
                goto out_kfree;
        }

        goto out;

out_kfree:
        kfree(kmalloc_ptr);
        unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1); 
// out_unreg:
//         unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
out_no_chrdev:
        remove_proc_entry(PROC_ENTRY_NAME, NULL);
out:
        return ret;
}

static void __exit my_exit(void)
{
        int i;

        cdev_del(&mmap_cdev);

        /* TODO 1: clear reservation on pages and free mem. */
        for (i = 0; i < (NPAGES+2); i++)
        {
            struct page *page = virt_to_page((void *)((uintptr_t)kmalloc_area + (i * PAGE_SIZE)));
            ClearPageReserved(page);  // Clear the reserved bit
        }

        kfree(kmalloc_area);

        unregister_chrdev_region(MKDEV(MY_MAJOR, 0), 1);
        /* TODO 3: remove proc entry */
        remove_proc_entry(PROC_ENTRY_NAME, NULL);
}

module_init(my_init);
module_exit(my_exit);