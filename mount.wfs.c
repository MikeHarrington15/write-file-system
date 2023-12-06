#define FUSE_USE_VERSION 30

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
    //read the superblock
    struct wfs_sb superblock;
    if (read(disk_fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error reading superblock");
        close(disk_fd);
        exit(EXIT_FAILURE);
    }

    //go past superblock
    if (lseek(disk_fd, superblock.head, SEEK_SET) == -1) {
        perror("Error seeking to the position after superblock");
        close(disk_fd);
        return EXIT_FAILURE;
    }
    
    //read the root node
    struct wfs_log_entry rootNode;
    if (read(disk_fd, &rootNode, sizeof(rootNode)) != sizeof(rootNode)) {
        perror("Error reading root node");
        close(disk_fd);
        exit(EXIT_FAILURE);
    }

    

    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    //read the superblock
    struct wfs_sb superblock;
    if (read(disk_fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        perror("Error reading superblock");
        close(disk_fd);
        exit(EXIT_FAILURE);
    }

    //go past superblock
    if (lseek(disk_fd, superblock.head, SEEK_SET) == -1) {
        perror("Error seeking to the position after superblock");
        close(disk_fd);
        return EXIT_FAILURE;
    }
    
    //read the root node
    struct wfs_log_entry rootNode;
    if (read(disk_fd, &rootNode, sizeof(rootNode)) != sizeof(rootNode)) {
        perror("Error reading root node");
        close(disk_fd);
        exit(EXIT_FAILURE);
    }

    struct wfs_log_entry newEntry;

    struct wfs_log_entry newFile;
    struct wfs_inode newFileInode;
    newFileInode.mode = S_IFREG | 0755;
    newFileInode.inode_number = 0;
    newFileInode.links = 1;  // Typically 2 for directories (".", "..")
    newFileInode.uid = getuid();
    newFileInode.gid = getgid();
    newFileInode.atime = time(NULL);
    newFileInode.mtime = time(NULL);
    newFileInode.ctime = time(NULL);
    newFileInode.size = 0;  // No entries yet
    newFile.inode = newFileInode;

    

    superblock.head += sizeof(newFile);  // Update to the next free space
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode) {
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