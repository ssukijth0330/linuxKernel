#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "read_write_driver"
#define BUFFER_SIZE 256

static char message[BUFFER_SIZE] = {0};
static int major_number;

static ssize_t dev_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    //The simple_read_to_buffer() is convenion function in kernel, uses for copies data from kernel-space buffer to user-space buffer.
    printk(KERN_ALERT "Called...dev_read\n");
    return simple_read_from_buffer(buffer, len, offset, message, strlen(message));
}

static ssize_t dev_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    //The simple_write_to_buffer() is convenion function in kernel, uses for copies data from user-space buffer to kernel-space buffer.
	printk(KERN_ALERT "Called....dev_write\n");
	return simple_write_to_buffer(message, BUFFER_SIZE - 1, offset, buffer, len);
}

static struct file_operations fops = {
    .read = dev_read,
    .write = dev_write,
};

static int __init read_write_driver_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register character device\n");
        return major_number;
    }
    printk(KERN_INFO "read write driver registered with major number %d\n", major_number);
    return 0;
}

static void __exit read_write_driver_exit(void) {
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "read write driver unregistered\n");
}

module_init(read_write_driver_init);
module_exit(read_write_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sutti");
MODULE_DESCRIPTION("A read write Linux character device driver");
