#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "wfs.h"

#define DISK_SIZE 1048576  // Example size, e.g., 1MB

int main(int argc, char *argv[]) {
    printf("Program started.\n"); // Indicates the program has started.

    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Disk path provided: %s\n", argv[1]); // Shows the disk path provided.

    const char *disk_path = argv[1];
    printf("Attempting to open or create the disk file...\n");
    int fd = open(disk_path, O_RDWR | O_CREAT);
    if (fd == -1) {
        perror("Error opening or creating disk file");
        return EXIT_FAILURE;
    }
    printf("Disk file opened or created successfully. fd: %d\n", fd);

    printf("Setting disk size to %d bytes...\n", DISK_SIZE);
    if (ftruncate(fd, DISK_SIZE) == -1) {
        perror("Error setting disk size");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Disk size set successfully.\n");

    printf("Initializing superblock...\n");
    struct wfs_sb superblock;
    superblock.magic = WFS_MAGIC;
    superblock.head = sizeof(struct wfs_sb);

    printf("Writing superblock to disk...\n");
    if (write(fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error writing superblock to disk");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Superblock written successfully.\n");

    printf("Initializing root directory log entry...\n");
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

    printf("Writing root directory log entry to disk...\n");
    if (write(fd, &emptyDirectory, sizeof(struct wfs_inode)) != sizeof(struct wfs_inode)) {
        perror("Error writing root directory log entry");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Root directory log entry written successfully.\n");

    printf("Updating superblock to point to the next free space...\n");
    superblock.head += sizeof(struct wfs_inode);  // Update to the next free space
    lseek(fd, 0, SEEK_SET);  // Go back to the beginning of the file
    if (write(fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error updating superblock");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("Superblock updated successfully.\n");

    close(fd);
    printf("Disk file closed. Program finished.\n");
    return EXIT_SUCCESS;
}
