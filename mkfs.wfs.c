#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "wfs.h"

#define DISK_SIZE 1048576  // Example size, e.g., 1MB

int main(int argc, char *argv[]) {
    printf("Program started.\n");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *disk_path = argv[1];
    printf("Disk path provided: %s\n", argv[1]);

    int fd = open(disk_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening or creating disk file");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, DISK_SIZE) == -1) {
        perror("Error setting disk size");
        close(fd);
        return EXIT_FAILURE;
    }

    void *disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("Error mapping disk file");
        close(fd);
        return EXIT_FAILURE;
    }

    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    superblock->magic = WFS_MAGIC;
    superblock->head = sizeof(struct wfs_sb);

    struct wfs_log_entry *emptyDirectory = (struct wfs_log_entry *)((char *)disk + sizeof(struct wfs_sb));
    emptyDirectory->inode.mode = S_IFDIR | 0755;
    emptyDirectory->inode.inode_number = 0;
    emptyDirectory->inode.links = 2;
    emptyDirectory->inode.uid = getuid();
    emptyDirectory->inode.gid = getgid();
    emptyDirectory->inode.atime = time(NULL);
    emptyDirectory->inode.mtime = time(NULL);
    emptyDirectory->inode.ctime = time(NULL);
    emptyDirectory->inode.size = 0;

    superblock->head += sizeof(struct wfs_inode);

    // Synchronize the memory-mapped region with the file
    if (msync(disk, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        munmap(disk, DISK_SIZE);
        close(fd);
        return EXIT_FAILURE;
    }

    // Unmap the file
    if (munmap(disk, DISK_SIZE) == -1) {
        perror("Error unmapping disk file");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    printf("Disk file initialized and closed. Program finished.\n");
    return EXIT_SUCCESS;
}
