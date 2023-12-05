#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wfs.h"

// Forward declarations of functions that will handle filesystem operations
static int wfs_getattr(const char *path, struct stat *stbuf);
static int wfs_mknod(const char *path, mode_t mode, dev_t rdev);
static int wfs_mkdir(const char *path, mode_t mode);
static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int wfs_unlink(const char *path);

static struct fuse_operations wfs_oper = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};


// Implement filesystem operations here
// For example:
static int wfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        // Root directory attributes
        stbuf->st_mode = S_IFDIR | 0755; // Directory with read/write/execute permissions for owner and read/execute for others
        stbuf->st_nlink = 2;             // Typically, for directories nlink = 2
    } else {
        // Here you would normally look up the file in your filesystem's structures
        // For this example, let's assume all other paths are regular files
        // In an actual implementation, you would return -ENOENT if the file doesn't exist
        stbuf->st_mode = S_IFREG | 0644; // Regular file with read/write permissions for owner and read for others
        stbuf->st_nlink = 1;             // Regular files typically have nlink = 1
        stbuf->st_size = 1024;           // Example file size; in a real scenario, this should be the file's actual size
    }

    // Set common attributes
    stbuf->st_uid = getuid(); // File owner
    stbuf->st_gid = getgid(); // File group
    stbuf->st_atime = time(NULL); // Last access time
    stbuf->st_mtime = time(NULL); // Last modification time
    stbuf->st_ctime = time(NULL); // Last status change time

    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    // Check if the file already exists in your filesystem
    // If it does, return -EEXIST

    // Create a new inode for the file
    struct wfs_inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));

    // Assign appropriate values to new_inode fields
    new_inode.mode = mode; // File type and mode
    new_inode.uid = getuid(); // Owner's user ID
    new_inode.gid = getgid(); // Owner's group ID
    new_inode.size = 0; // Initial size of the file is 0
    new_inode.atime = time(NULL); // Access time
    new_inode.mtime = time(NULL); // Modification time
    new_inode.ctime = time(NULL); // Status change time
    new_inode.links = 1; // Number of hard links

    // Implement logic to add the new inode to your filesystem
    // This would typically involve writing the new inode to the log

    // Handle potential errors, such as if there's no space left on the device
    // In such a case, return -ENOSPC

    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode) {
    // Check if the directory already exists in your filesystem
    // If it does, return -EEXIST

    // Create a new inode for the directory
    struct wfs_inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(new_dir_inode));

    // Assign appropriate values to new_dir_inode fields
    new_dir_inode.mode = S_IFDIR | (mode & 0777); // Directory type and permissions
    new_dir_inode.uid = getuid(); // Owner's user ID
    new_dir_inode.gid = getgid(); // Owner's group ID
    new_dir_inode.size = 0; // Initial size, might be set to a constant representing an empty directory
    new_dir_inode.atime = time(NULL); // Access time
    new_dir_inode.mtime = time(NULL); // Modification time
    new_dir_inode.ctime = time(NULL); // Status change time
    new_dir_inode.links = 2; // Directories typically start with 2 links ('.' and its entry in the parent directory)

    // Implement logic to add the new directory inode to your filesystem
    // This involves updating your log structure to include the new directory

    // Handle potential errors, such as if there's no space left on the device
    // In such a case, return -ENOSPC

    return 0;
}


static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Locate the file in your filesystem using 'path'
    // If the file does not exist, return -ENOENT

    // Check if offset is not beyond the end of the file
    // If it is, adjust 'size' to read only up to the end of the file

    // Read the data from the file starting at 'offset'
    // You'll need to access your filesystem's storage to get the file content
    // Copy the data into 'buf'

    // Determine the actual number of bytes read
    size_t bytes_read = 0; // Actual number of bytes read

    return bytes_read; // Return the number of bytes actually read
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Locate the file in your filesystem using 'path'
    // If the file does not exist, return -ENOENT

    // Check if the write operation will extend the size of the file
    // If so, you need to handle increasing the file's size

    // Write the data to the file starting at 'offset'
    // This involves updating your filesystem's storage with the contents of 'buf'
    // Ensure that the write operation does not exceed 'size'
    // Handle the case where the write operation spans multiple log entries

    // Determine the actual number of bytes written
    size_t bytes_written = 0; // Actual number of bytes written

    return bytes_written; // Return the number of bytes actually written
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    // Locate the directory in your filesystem using 'path'
    // If the directory does not exist, return -ENOENT

    // If the directory is found, iterate over its contents
    // For each entry in the directory:
    //   Use the filler function to add the entry to the buffer
    //   Example: filler(buf, entry_name, NULL, 0);

    // The filler function returns 1 if the buffer is full, and 0 otherwise
    // If the buffer is full, you should stop adding entries and return 0

    // Here's an example of adding a static entry, replace this with your actual directory content logic
    filler(buf, ".", NULL, 0);  // Current directory
    filler(buf, "..", NULL, 0); // Parent directory

    // Iterate through and add actual files/subdirectories from your filesystem's directory structure
    // Example:
    // filler(buf, "some_file", NULL, 0);
    // filler(buf, "some_directory", NULL, 0);

    return 0;
}

static int wfs_unlink(const char *path) {
    // Locate the file in your filesystem using 'path'
    // If the file does not exist, return -ENOENT

    // Implement logic to remove the file from the filesystem
    // This typically involves:
    // - Removing the file's entry from its parent directory
    // - Marking the file's space as free in the filesystem (if applicable)
    // - Updating relevant metadata

    // In a log-structured system, you might also need to create a new log entry
    // indicating that the file has been deleted

    return 0;
}

int main(int argc, char *argv[]) {
    // Ensure that '-s' (single-threaded mode) is always passed to fuse_main
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    --argc;
    return fuse_main(argc, argv, &wfs_oper, NULL);
}