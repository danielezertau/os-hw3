#include "message_slot.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int expected_num_args = 4, channel_id, message_len, fd;
    char *message_slot_file_path, *message;
    size_t bytes_written;
    if (argc != expected_num_args) {
        printf("Wrong number of arguments. Expected: %d, actual: %d", expected_num_args, argc);
        exit(EXIT_FAILURE);
    }
    message_slot_file_path = argv[1];
    channel_id = (int) strtol(argv[2], NULL, 10);
    message = argv[3];
    message_len = (int) strlen(message);

    fd = open(message_slot_file_path, O_RDWR);
    if (fd < 0) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    bytes_written = write(fd, message, message_len);
    if (bytes_written == -1 || bytes_written != message_len) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}