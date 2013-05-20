/**
  * Sachin Desai (C)
  * mmap Driver Example 
  * May 15th, 2013
  * sachin dot desai at gmail
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define LEN         (16*1024)
#define DEVICE_NAME	"kmap"

static int major;
static struct class *kmap_class;
static struct device *kmap_dev;
unsigned long virt_addr;
static char *kmalloc_ptr = NULL;
static char *kmalloc_area = NULL;

static long
kmap_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  printk("kmap: unlocked ioctl\n"); 

  return 0; 
}


static int
mmap_kmalloc (struct file *filp, struct vm_area_struct *vma)
{
  int ret;
  unsigned long length;

  length = vma->vm_end - vma->vm_start;

  /** Restrict to size of device memory */
  if (length > LEN)
    return -EIO;

  /**
   * remap_pfn_range function arguments:
   * vma: vm_area_struct passed to the mmap method
   * vma->vm_start: start of mapping user address space
   * Page frame number of first page that you can get by:
   *   virt_to_phys((void *)kmalloc_area) >> PAGE_SHIFT
   * size: length of mapping in bytes which is simply vm_end - vm_start
   * vma->>vm_page_prot: protection bits received from the application
   */
  vma->vm_flags |= VM_RESERVED;
  ret = remap_pfn_range (vma, 
      vma->vm_start,
      virt_to_phys ((void *) ((unsigned long) kmalloc_area)) >> PAGE_SHIFT, 
      vma->vm_end - vma->vm_start,
      vma->vm_page_prot);

  if (ret)
    return -EAGAIN;

  return 0;
}

static const struct file_operations kmap_fops = { 
  mmap: mmap_kmalloc,
  unlocked_ioctl: kmap_unlocked_ioctl,
  owner: THIS_MODULE
};

void init_memory(const char* str, char* begin, unsigned long len)
{
  int i = 0; 
  char* ptr = NULL; 
  int str_len = strlen(str); 
  int iterations = len / str_len; 
  int leftover = len - (iterations * str_len); 

  printk(KERN_INFO "str = %s, str_len = %d\n", str, str_len); 
  printk(KERN_INFO "begin = %p, len = %lu\n", begin, len); 
  printk(KERN_INFO "iterations = %d, leftover = %d\n", iterations, leftover); 

  for (i = 0, ptr = begin; i < iterations; i++, ptr += str_len) {
    memcpy(ptr, str, str_len); 
  }

  printk(KERN_INFO "ptr = %p\n", ptr); 

  memcpy(ptr, str, leftover); 
}


static int 
__init kmap_init (void)
{
  void *ptr_err = NULL; 

  /**
   * register w/ dynamic major number
   */
  major = register_chrdev (0, DEVICE_NAME, &kmap_fops);
  printk(KERN_INFO "kmap: Loading driver with major num: %d\n", major); 
  if (major < 0) {
    printk (KERN_ERR "kmap: Registration failed with %d\n", major);
    return major;
  }

  /**
   * udev class
   */
  kmap_class = class_create (THIS_MODULE, DEVICE_NAME);
  if (IS_ERR(ptr_err = kmap_class)) {
    printk (KERN_ERR "kmap: Error creating kmap class\n");
    unregister_chrdev(major, DEVICE_NAME); 
    return PTR_ERR(ptr_err); 
  }

  /** 
   * register with sysfs
   */
  kmap_dev = device_create(kmap_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 
  if (IS_ERR(ptr_err = kmap_dev)) {
    printk (KERN_ERR "kmap: Error creating kmap device\n");
    class_destroy(kmap_class);
    return PTR_ERR(ptr_err); 
  }

  /**
	 * kmalloc() returns memory in bytes instead of PAGE_SIZE
	 * mmap memory should be PAGE_SIZE and aligned on a PAGE boundary.
	 */
  kmalloc_ptr = kmalloc(LEN + (2*PAGE_SIZE), GFP_KERNEL);
  if (!kmalloc_ptr) {
    printk (KERN_ERR "kmap: kmalloc failed\n");
    return -ENOMEM;
  }
  printk(KERN_ERR "kmap: kmalloc_ptr at 0x%p \n", kmalloc_ptr);

  /**
	 * This is the same as: 
	 * (int *)((((unsigned long)kmalloc_ptr) + ((1<<12) - 1)) & 0xFFFF0000);
	 * where: PAGE_SIZE is defined as 1UL <<PAGE_SHIFT. 
	 * That is 4k on x86. 0xFFFF0000 is a PAGE_MASK to mask out the upper 
	 * bits in the page. This will align it at 4k page boundary that means 
	 * kmalloc start address is now page aligned.
	 */
  kmalloc_area = (char *) (((unsigned long) kmalloc_ptr + (PAGE_SIZE - 1)) & PAGE_MASK);
  printk ("kmap: kmalloc_area at 0x%p \t physical at 0x%lx)\n", 
    kmalloc_area,
    (long unsigned int) virt_to_phys ((void *)(kmalloc_area)));

  /* reserve kmalloc memory as pages to make them remapable */
  for (virt_addr = (unsigned long) kmalloc_area;
       virt_addr < (unsigned long) kmalloc_area + LEN; 
       virt_addr += PAGE_SIZE)
  {
      SetPageReserved (virt_to_page (virt_addr));
  } 

  /**
	 *  Write code to init memory with ascii 0123456789. Where ascii 
	 *  equivalent of 0 is 48  and 9 is 58. This is read from mmap() by 
	 *  user level application
	 */

  init_memory("0123456789", kmalloc_area, LEN);  

  return 0;
}

static void 
__exit kmap_exit (void) 
{
  for (virt_addr = (unsigned long) kmalloc_area;
       virt_addr < (unsigned long) kmalloc_area + LEN; 
       virt_addr += PAGE_SIZE)
  {

    ClearPageReserved(virt_to_page (virt_addr));
  } 

  if (kmalloc_ptr)
    kfree(kmalloc_ptr);

  printk (KERN_INFO "kmap: cleaning up kmap module\n");
  device_destroy(kmap_class, MKDEV(major,0)); 
  class_destroy(kmap_class);

  return unregister_chrdev(major, DEVICE_NAME); 
} 


module_init(kmap_init);
module_exit(kmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sachin Desai <sachin.desai@ieee.org>");
MODULE_DESCRIPTION("kmap");
