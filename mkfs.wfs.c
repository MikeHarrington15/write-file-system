#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "wfs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *disk_path = argv[1];
    int fd = open(disk_path, O_RDWR);
    if (fd == -1) {
        perror("Error opening disk file");
        return EXIT_FAILURE;
    }

    // Initialize the superblock
    struct wfs_sb superblock;
    superblock.magic = WFS_MAGIC;
    superblock.head = sizeof(struct wfs_sb);  // The next free space starts right after the superblock

    // Write the superblock to the disk image
    if (write(fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error writing superblock to disk");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    return EXIT_SUCCESS;
}
