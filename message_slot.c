// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/stddef.h> /* for NULL */
#include <linux/slab.h> /* for GFP_KERNEL */
#include "message_slot.h"

MODULE_LICENSE("GPL");


// Linked List implementation
struct LinkedList {
    char message[MAX_MESSAGE_LEN];
    int message_size;
    int channel_id;
    struct LinkedList *next;
};

// Message slots array
static struct LinkedList* message_slots[MAX_MESSAGE_SLOTS];

int get_minor_number(struct file* file) {
    return iminor(file->f_inode);
}

void free_linked_list(struct LinkedList* head) {
    struct LinkedList *curr, *tmp;
    if (head == NULL) {
        return;
    }
    curr = head;
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
    ssize_t i;
    int minor_number, channel_id, prev_message_size;
    struct LinkedList *head, *curr;

    // Invalid buffer
    if (access_ok(buffer, length) == 0) {
        return -EINVAL;
    }

    // Channel ID not set
    if (file->private_data == NULL) {
        return -EINVAL;
    }
    channel_id = (unsigned long) file->private_data;

    // File descriptor has no channels
    minor_number = get_minor_number(file);
    head = message_slots[minor_number];
    if (head == NULL) {
        printk(KERN_ERR, "Linked list head is NULL\n");
        return -EWOULDBLOCK;
    }

    printk(KERN_INFO, "Invoking device_read\n");

    // Search for the message channel
    curr = head;
    while(curr ->next != NULL && curr->channel_id != channel_id) {
        curr = curr->next;
    }

    // Channel ID not found
    if (curr->channel_id != channel_id) {
        printk(KERN_ERR, "Channel ID %d not found\n", channel_id);
        return -EWOULDBLOCK;
    }

    prev_message_size = curr->message_size;

    // Buffer too small for message
    if (length < prev_message_size) {
        printk(KERN_ERR, "Buffer too small for the previous message. "
                         "Message size: %d, buffer size: %d\n", channel_id, length);
        return -ENOSPC;
    }

    // Write message to user buffer
    for( i = 0; i < prev_message_size; ++i ) {
        if (put_user((curr->message)[i], &buffer[i]) != 0) {
            printk(KERN_ERR, "Failed to write message to the user buffer\n");
            return -EFAULT;
        }
    }
    printk("Successfully read %zu bytes\n", i);
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

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file,
                            const char __user* buffer,
                            size_t length,
                            loff_t* offset) {
    ssize_t i;
    int minor_number, channel_id;
    struct LinkedList *head, *curr;

    // Invalid buffer
    if (access_ok(buffer, length) == 0) {
        return -EINVAL;
    }

    // Channel not set
    if (file->private_data == NULL) {
        return -EINVAL;
    }

    channel_id = (unsigned long) file->private_data;

    // Message too big or no message
    if (length == 0 || length > 128) {
        return -EMSGSIZE;
    }

    // Get file minor number
    minor_number = get_minor_number(file);

    printk(KERN_INFO, "Invoking device_write\n");

    head = message_slots[minor_number];
    curr = head;

    if (head == NULL) {
        // Initialize linked list
        printk(KERN_INFO, "Initializing new linked list for minor %d", minor_number);
        head = create_node(channel_id);
        if (head == NULL) {
            printk(KERN_ERR, "Failed to create new linked list for minor %d", minor_number);
            return -ENOMEM;
        }
        message_slots[minor_number] = head;
        curr = head;
    } else {
        // Traverse linked list
        while(curr->next != NULL && curr->channel_id != channel_id) {
            curr = curr->next;
        }
        if (curr->channel_id != channel_id) {
            // We are at the List's tail, create new node
            printk(KERN_INFO, "Creating new node for channel %ld", channel_id);
            curr -> next = create_node(channel_id);
            if (curr -> next == NULL) {
                printk("Failed to create linked list node for channel %ld", channel_id);
                return -ENOMEM;
            }
            // Move to the new node
            curr = curr -> next;
        }
    }

    // Write the message to the kernel buffer
    for( i = 0; i < length ; ++i ) {
        if (get_user((curr->message)[i], &buffer[i]) != 0) {
            printk(KERN_ERR, "Failed to write message to the kernel buffer\n");
            return -EFAULT;
        }
    }
    curr->message_size = i;
    printk("Successfully wrote %zu bytes\n", i);
    // return the number of input characters used
    return i;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int ioctl_command_id,
                          unsigned long ioctl_param )
{
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL;
    }

    printk("Setting channel ID to %ld\n", ioctl_param);
    file->private_data = (void *) ioctl_param;
    printk("Successfully set private data to %ld\n", ioctl_param);

    return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
        .owner	        = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init message_slot_init(void) {
    int rc = -1;
    // Register driver capabilities. Obtain major num
    rc = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);

    // Negative values signify an error
    if( rc < 0 ) {
        printk(KERN_ALERT "%s registration failed for %d\n",
               DEVICE_NAME, MAJOR_NUM);
        return rc;
    }

    return SUCCESS;
}

//---------------------------------------------------------------
static void __exit message_slot_cleanup(void) {
    int i;
    // Free memory
    for (i = 0; i < MAX_MESSAGE_SLOTS; ++i) {
        free_linked_list(message_slots[i]);
    }
    // Unregister the device
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

//---------------------------------------------------------------
module_init(message_slot_init);
module_exit(message_slot_cleanup);
