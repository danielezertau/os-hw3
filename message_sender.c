#include "message_slot.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int expected_num_args = 4;
    if (argc != expected_num_args) {
        printf("Wrong number of arguments. Expected: %d, actual: %d", expected_num_args, argc);
        exit(EXIT_FAILURE);
    }
    char* message_slot_file_path = argv[1];
    int channel_id = (int) strtol(argv[2], NULL, 10);
    char* message = argv[3];
    int message_len = (int) strlen(message);

    int fd = open(message_slot_file_path, O_RDWR);
    if (fd < 0) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (write(fd, message, message_len) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}