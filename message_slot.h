#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

// The major device number.
#define MAJOR_NUM 235

// The device name
#define DEVICE_NAME "message_slot"

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

// Number of message slots
#define MAX_MESSAGE_SLOTS 256

// Buffer message_size
#define MAX_MESSAGE_LEN 128

// Exit codes
#define SUCCESS 0
#define FAILURE (-1)
#endif
