#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h> //for mdelay
#include <linux/device.h> //for class_create



#define DEVICE_NAME "pcspkr_driver"
#define CLASS_NAME "pcspkr_class"

static int major_number;
static struct class* pcspkr_class = NULL;

static void beep(int frequency, int duration) {
    // Send command to the PC speaker to generate a beep
    outb(0xB6, 0x43); // Set up the timer
    outb(1193180 / frequency & 0xFF, 0x42); // Set frequency
    outb(1193180 / frequency >> 8, 0x42);
    outb(inb(0x61) | 3, 0x61); // Enable speaker
    mdelay(duration); // Wait for duration
    outb(inb(0x61) & ~3, 0x61); // Disable speaker
}

static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "PC Speaker Device: Opened\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "PC Speaker Device: Closed\n");
    return 0;
}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    int frequency = 440; // Default frequency (A4 note)
    int duration = 1000; // Default duration (1 second)

    printk(KERN_INFO "PC Speaker Device: write\n");

    // Copy data from user space to kernel space.
    if (copy_from_user(&frequency, buf, sizeof(int))) {
        return -EFAULT;
    }

    beep(frequency, duration);
    return count; // Return number of bytes written
}

//Device driver handler
//  THIS_MODULE is defind in module.h
//    
//  #define THIS_MODULE ((struc modulr *)0)
//
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .write = device_write,
};

static int __init pcspkr_init(void) {
    // Register
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "PC Speaker Device: Failed to register a major number\n");
        return major_number;
    }

    // Create class
    pcspkr_class = class_create(CLASS_NAME);
    if (IS_ERR(pcspkr_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "PC Speaker Device: Failed to create device class\n");
        return PTR_ERR(pcspkr_class);
    }

    // device create
    if (IS_ERR(device_create(pcspkr_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME))) {
        class_destroy(pcspkr_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "PC Speaker Device: Failed to create device\n");
        return PTR_ERR(pcspkr_class);
    }

    printk(KERN_INFO "PC Speaker Device: Driver loaded: major number: [%d]\n", major_number);
    return 0;
}

static void __exit pcspkr_exit(void) {
    device_destroy(pcspkr_class, MKDEV(major_number, 0));
    class_destroy(pcspkr_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "PC Speaker Device: Driver unloaded\n");
}

module_init(pcspkr_init);
module_exit(pcspkr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux character device driver to generate sound using the PC speaker.");
