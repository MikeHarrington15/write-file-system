#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "wfs.h"

#define DISK_SIZE 1048576  // Example size, e.g., 1MB

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *disk_path = argv[1];
    int fd = open(disk_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening or creating disk file");
        return EXIT_FAILURE;
    }

    // Ensure the disk is of the correct size (e.g., 1MB)
    if (ftruncate(fd, DISK_SIZE) == -1) {
        perror("Error setting disk size");
        close(fd);
        return EXIT_FAILURE;
    }

    // Initialize the superblock
    struct wfs_sb superblock;
    superblock.magic = WFS_MAGIC;
    superblock.head = sizeof(struct wfs_sb);

    // Write the superblock to the disk image
    if (write(fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error writing superblock to disk");
        close(fd);
        return EXIT_FAILURE;
    }

    // Initialize the root directory log entry
    struct wfs_log_entry emptyDirectory;
    struct wfs_inode emptyDirectoryInode;
    
    emptyDirectoryInode.mode = S_IFDIR | 0755;
    emptyDirectoryInode.inode_number = 0;
    emptyDirectoryInode.links = 2;  // Typically 2 for directories (".", "..")
    emptyDirectoryInode.uid = getuid();
    emptyDirectoryInode.gid = getgid();
    emptyDirectoryInode.atime = time(NULL);
    emptyDirectoryInode.mtime = time(NULL);
    emptyDirectoryInode.ctime = time(NULL);
    emptyDirectoryInode.size = 0;  // No entries yet

    emptyDirectory.inode = emptyDirectoryInode;

    // Write the root directory log entry to the disk
    if (write(fd, &emptyDirectory, sizeof(struct wfs_inode)) != sizeof(struct wfs_inode)) {
        perror("Error writing root directory log entry");
        close(fd);
        return EXIT_FAILURE;
    }

    // Update the superblock to point to the next free space
    superblock.head += sizeof(struct wfs_inode);  // Update to the next free space
    lseek(fd, 0, SEEK_SET);  // Go back to the beginning of the file
    write(fd, &superblock, sizeof(superblock));  // Write the updated superblock

    close(fd);
    return EXIT_SUCCESS;
}
