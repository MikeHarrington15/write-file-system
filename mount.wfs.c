#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "wfs.h"

// Forward declarations of functions that will handle filesystem operations
static int wfs_getattr(const char *path, struct stat *stbuf);
static int wfs_mknod(const char *path, mode_t mode, dev_t rdev);
static int wfs_mkdir(const char *path, mode_t mode);
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int wfs_unlink(const char *path);

// Global file descriptor for the disk file
int disk_fd = -1;

// Initialize fuse_operations structure
static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};

static int wfs_getattr(const char *path, struct stat *stbuf) {
    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode) {
    // Only create the directory "a" at the root
    if (strcmp(path, "mnt/a") != 0) {
        printf("TESTSTSETSETSETSE\n");
        return -EINVAL;  // Invalid path
    }

    // Read the superblock
    struct wfs_sb superblock;
    if (lseek(disk_fd, 0, SEEK_SET) == -1 || 
        read(disk_fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error reading superblock");
        return -EIO;
    }

    if (superblock.magic != WFS_MAGIC) {
        fprintf(stderr, "Invalid filesystem magic number.\n");
        return -EIO;
    }

    // Create a new inode for the directory "a"
    struct wfs_inode new_inode;
    new_inode.inode_number = superblock.head; // Assign the head as the inode number
    new_inode.deleted = 0;
    new_inode.mode = S_IFDIR | (mode & 0777); // Set as directory and apply mode mask
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.size = 0;  // Initially empty
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.links = 2; // Standard for directories (".", "..")

    // Write the inode to the disk
    struct wfs_log_entry new_entry;
    new_entry.inode = new_inode;
    if (lseek(disk_fd, superblock.head, SEEK_SET) == -1 ||
        write(disk_fd, &new_entry, sizeof(new_entry)) != sizeof(new_entry)) {
        perror("Error writing new directory inode");
        return -EIO;
    }

    // Update the superblock to point to the next free space
    superblock.head += sizeof(new_entry);  // Update to the next free space
    if (lseek(disk_fd, 0, SEEK_SET) == -1 ||
        write(disk_fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error updating superblock");
        return -EIO;
    }

    return 0;
}



static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    return 0;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    return 0;
}

static int wfs_unlink(const char *path) {
    return 0;
}

int main(int argc, char *argv[]) {
    const char *disk_path = argv[2];
    disk_fd = open(disk_path, O_RDWR);
    if (disk_fd == -1) {
        perror("Error opening disk file");
        return EXIT_FAILURE;
    }

    printf("Disk file opened. File descriptor: %d\n", disk_fd);

    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    --argc;
    
    int fuse_stat = fuse_main(argc, argv, &ops, NULL);

    close(disk_fd);
    printf("Disk file closed.\n");

    return fuse_stat;
}