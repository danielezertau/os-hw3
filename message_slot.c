// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/errno.h>
#include <linux/stddef.h> /* for NULL */
#include <linux/slab.h> /* for GFP_KERNEL */

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

// Message slots array
static struct LinkedList* message_slots[MAX_MESSAGE_SLOTS];

// Linked List implementation
struct LinkedList {
    char message[MAX_MESSAGE_LEN];
    int message_size;
    int channel_id;
    struct LinkedList *next;
};

void free_linked_list(struct LinkedList* head) {
    if (head == NULL) {
        return;
    }
    struct LinkedList* curr = head;
    struct LinkedList* tmp;
    while (curr->next != NULL) {
        tmp = curr;
        curr = curr -> next;
        kfree(tmp);
    }
}


//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file* file,
                            char __user* buffer,
                            size_t length,
                            loff_t* offset) {
    // Channel ID not set
    int channel_id = (int) file->private_data == 0;
    if (channel_id) {
        return -EINVAL;
    }

    // Channel is empty
    int minor_number = get_minor_number(file);
    struct LinkedList* head = message_slots[minor_number];
    if (head == NULL) {
        return -EWOULDBLOCK;
    }

    printk("Invoking device_read(%p,%ld)\n", file, length);
    // Search for the message channel
    struct LinkedList* curr = head;
    while(head ->next != NULL && head->channel_id != channel_id) {
        curr = curr->next;
    }
    // No channel for ID
    if (curr == NULL || curr->channel_id != channel_id) {
        return -EWOULDBLOCK;
    }

    // Buffer too small for message
    if (length < curr->message_size) {
        return -ENOSPC;
    }

    // Write message to user buffer
    for( i = 0; i < length && i < MAX_MESSAGE_LEN; ++i ) {
        if (put_user((curr->message)[i], &buffer[i]) != 0) {
            return -EFAULT;
        }
    }

    return i;
}

struct LinkedList* create_node(int channel_id) {
    struct LinkedList* node;
    node = kmalloc(sizeof(struct LinkedList), GFP_KERNEL);
    if (node != NULL) {
        node->channel_id = channel_id;
    }
    return node;
}

int get_minor_number(struct file* file) {
    return iminor(file->f_inode);
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file,
                            const char __user* buffer,
                            size_t length,
                            loff_t* offset) {
    // Channel not set
    int channel_id = (int) file->private_data;
    if (channel_id == 0) {
        return -EINVAL;
    }
    // Message too big or no message
    if (length == 0 || length > 128) {
        return -EMSGSIZE;
    }

    // Write message to the kernel buffer
    int minor_number = get_minor_number(file);

    ssize_t i;
    printk("Invoking device_write(%p,%ld)\n", file, length);

    struct LinkedList* head = message_slots[minor_number];
    struct LinkedList* curr = head;

    if (head == NULL) {
        // Initialize linked list
        head = create_node(channel_id);
        if (head == NULL) {
            return -EMVSERR;
        }
        message_slots[minor_number] = head;
        curr = head;
    } else {
        // Traverse linked list
        while(curr->next != NULL) {
            curr = curr->next;
        }
        // Create new node
        curr = create_node(channel_id);
        if (curr == NULL) {
            return -EMVSERR;
        }
    }

    // Write message
    for( i = 0; i < length && i < MAX_MESSAGE_LEN; ++i ) {
        if (get_user((curr->message)[i], &buffer[i]) != 0) {
            return -EFAULT;
        }
    }
    curr->message_size = i;
    // return the number of input characters used
    return i;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int ioctl_command_id,
                          unsigned int ioctl_param )
{
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL;
    }

    printk("Setting channel ID to %ld\n", ioctl_param);
    file -> private_data = (void *) ioctl_param;

    return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init module_init(void)
{
    int rc = -1;
    // Register driver capabilities. Obtain major num
    rc = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);

    // Negative values signify an error
    if( rc < 0 ) {
        printk(KERN_ALERT "%s registration failed for %d\n",
               DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }

    return SUCCESS;
}

//---------------------------------------------------------------
static void __exit module_cleanup(void)
{
    // Free memory
    for (int i = 0; i < MAX_MESSAGE_SLOTS; ++i) {
        free_linked_list(message_slots[i]);
    }
    // Unregister the device
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

//---------------------------------------------------------------
module_init(module_init);
module_exit(module_cleanup);
