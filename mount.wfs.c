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
// static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
// static int wfs_unlink(const char *path);

// Initialize fuse_operations structure
static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    // .read	    = wfs_read,
    // .write      = wfs_write,
    // .readdir	= wfs_readdir,
    // .unlink    	= wfs_unlink,
};

int disk_fd = -1;  // You should open the disk image file and assign to disk_fd in your main function

static int wfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        // Open disk image file
        int fd = open("path_to_disk_image", O_RDONLY);
        if (fd == -1) {
            perror("Error opening disk image");
            return -ENOENT;
        }

        // Seek to the position right after the superblock
        lseek(fd, sizeof(struct wfs_sb), SEEK_SET);

        // Read inode
        struct wfs_inode inode_buffer;
        if (read(fd, &inode_buffer, sizeof(struct wfs_inode)) != sizeof(struct wfs_inode)) {
            close(fd);
            return -ENOENT; // Error reading inode
        }

        if (inode_buffer.inode_number != 0 || inode_buffer.deleted) {
            close(fd);
            return -ENOENT; // Root inode not found or deleted
        }

        // Set attributes
        stbuf->st_mode = inode_buffer.mode;
        stbuf->st_nlink = inode_buffer.links;
        stbuf->st_uid = inode_buffer.uid;
        stbuf->st_gid = inode_buffer.gid;
        stbuf->st_size = inode_buffer.size;

        close(fd);
        return 0;
    }

    // For now, assume no other files/directories exist
    return -ENOENT;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    // Open the disk file
    int fd = open("./disk", O_RDWR);
    if (fd == -1) {
        perror("Error opening disk image");
        return -EIO;
    }

    // Seek past the superblock to the beginning of the log entries
    off_t current_offset = sizeof(struct wfs_sb);
    lseek(fd, current_offset, SEEK_SET);

    struct wfs_inode inode_buffer;
    // Scan for the next free space or the end of the log
    while (read(fd, &inode_buffer, sizeof(struct wfs_inode)) == sizeof(struct wfs_inode)) {
        if (inode_buffer.deleted) {
            // Found a free space
            break;
        }
        // Advance by the size of the current entry
        current_offset += sizeof(struct wfs_inode) + inode_buffer.size;
        lseek(fd, current_offset, SEEK_SET);
    }

    // Now 'current_offset' points to where the new entry should be written

    // Create a new file inode
    struct wfs_inode new_inode = {
        .inode_number = current_offset, // Use the current offset as the inode number
        .mode = mode,
        .uid = getuid(),
        .gid = getgid(),
        .size = 0, // Size will be zero initially
        .atime = time(NULL),
        .mtime = time(NULL),
        .ctime = time(NULL),
        .links = 1,
        .deleted = 0  // Mark as not deleted
    };

    // Write the new inode to the disk at the found location
    if (pwrite(fd, &new_inode, sizeof(new_inode), current_offset) != sizeof(new_inode)) {
        close(fd);
        return -EIO;
    }

    close(fd);
    return 0;
}


static int wfs_mkdir(const char *path, mode_t mode) {
    return 0;
}

// static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
//     return 0;
// }

// static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
//     return 0;
// }

// static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
//     return 0;
// }

// static int wfs_unlink(const char *path) {
//     return 0;
// }

int main(int argc, char *argv[]) {
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    --argc;
    return fuse_main(argc, argv, &ops, NULL);
}