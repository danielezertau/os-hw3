#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int expected_num_args = 3, channel_id, fd;
    char* message_slot_file_path;
    char buff[MAX_MESSAGE_LEN];

    if (argc != expected_num_args) {
        printf("Wrong number of arguments. Expected: %d, actual: %d", expected_num_args, argc);
        exit(EXIT_FAILURE);
    }

    message_slot_file_path = argv[1];
    channel_id = (int) strtol(argv[2], NULL, 10);

    fd = open(message_slot_file_path, O_RDWR);
    if (fd < 0) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (read(fd, buff, MAX_MESSAGE_LEN) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (write(STDOUT_FILENO, buff, MAX_MESSAGE_LEN) == -1) {
        perror(strerror(errno));
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}