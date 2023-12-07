#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include "wfs.h"

#define DISK_SIZE 1048576  // Example size, e.g., 1MB
#define MAX_PATH_LEN 128

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
int newInode = 0;

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

struct wfs_log_entry* looper(const char *path, mode_t mode) {
    // Map the disk file into memory
    struct wfs_sb *superblock = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);
    if (superblock == MAP_FAILED) {
        perror("Error mapping disk file");
        return NULL;  // Return the initialized empty entry on error
    }

   // Start from the root node
    struct wfs_log_entry *start_of_log = (struct wfs_log_entry *)((char *)superblock + sizeof(struct wfs_sb));
    struct wfs_log_entry *end_of_log = (struct wfs_log_entry *)((char *)superblock + superblock->head);
    struct wfs_log_entry *current_entry;
    struct wfs_log_entry *found_entry;
    memset(&found_entry, 0, sizeof(struct wfs_log_entry));

    char *rest = strdup(path);
    char *token = strtok(rest, "/");

    while (token != NULL) {
        current_entry = start_of_log;
        int found = 0;

        while (current_entry < end_of_log) {
            if ((S_ISDIR(current_entry->inode.mode) || S_ISREG(current_entry->inode.mode)) &&
                strcmp(((struct wfs_dentry *)current_entry->data)->name, token) == 0) {
                *found_entry = *current_entry;
                found = 1;
            }
            // Move to the next log entry
            current_entry = (struct wfs_log_entry *)((char *)current_entry + sizeof(struct wfs_log_entry) + current_entry->inode.size);
        }

        if (!found) {
            // Token not found in the log, path does not exist
            free(rest);
            munmap(superblock, DISK_SIZE);
            return NULL;
        }

        token = strtok(NULL, "/"); // Move to next token
    }

    free(rest);
    munmap(superblock, DISK_SIZE);
    return found_entry; // Return the last found entry
}

static int wfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat)); // Initialize the stat structure

    struct wfs_log_entry *entry = looper(path, 0); // Use looper to find the log entry for the path
    if (!entry) {
        return -ENOENT; // File or directory not found
    }

    // Fill the stat structure with the file/directory attributes
    stbuf->st_mode = entry->inode.mode;
    stbuf->st_nlink = entry->inode.links;
    stbuf->st_uid = entry->inode.uid;
    stbuf->st_gid = entry->inode.gid;
    stbuf->st_size = entry->inode.size;
    stbuf->st_atime = entry->inode.atime;
    stbuf->st_mtime = entry->inode.mtime;
    stbuf->st_ctime = entry->inode.ctime;
    stbuf->st_ino = entry->inode.inode_number;

    return 0; // Return success
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    // Map the disk file into memory
    struct wfs_sb *superblock = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);
    if (superblock == MAP_FAILED) {
        perror("Error mapping disk file");
        return -EIO;
    }

    // Use looper to find the parent directory of the path
    char parent_path[MAX_PATH_LEN];
    strcpy(parent_path, path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == NULL) {
        munmap(superblock, DISK_SIZE);
        return -ENOENT;  // Invalid path
    }
    *last_slash = '\0'; // Terminate the string to get the parent path
    struct wfs_log_entry *parent_dir_entry = looper(parent_path, S_IFDIR);
    if (!parent_dir_entry) {
        munmap(superblock, DISK_SIZE);
        return -ENOENT;  // Parent directory not found
    }

    // Prepare the new file entry
    struct wfs_dentry newFile;
    strncpy(newFile.name, last_slash + 1, MAX_FILE_NAME_LEN);
    newFile.name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    newFile.inode_number = superblock->head + sizeof(struct wfs_log_entry); // Assign inode number after new dir entry

    // Create a new log entry for the parent directory
    struct wfs_log_entry *newParentDirEntry = (struct wfs_log_entry *)((char *)superblock + superblock->head);
    *newParentDirEntry = *parent_dir_entry; // Copy the existing parent dir entry
    newParentDirEntry->inode.size += sizeof(struct wfs_dentry); // Increase the size to include new dentry
    memcpy((char *)newParentDirEntry->data + parent_dir_entry->inode.size, &newFile, sizeof(struct wfs_dentry));

    superblock->head += sizeof(struct wfs_log_entry) + newParentDirEntry->inode.size;

    // Create a new log entry for the file
    struct wfs_log_entry *newFileEntry = (struct wfs_log_entry *)((char *)superblock + superblock->head);
    newFileEntry->inode.mode = mode;
    newFileEntry->inode.inode_number = newFile.inode_number;
    newFileEntry->inode.links = 1;
    newFileEntry->inode.uid = getuid();
    newFileEntry->inode.gid = getgid();
    newFileEntry->inode.atime = time(NULL);
    newFileEntry->inode.mtime = time(NULL);
    newFileEntry->inode.ctime = time(NULL);
    newFileEntry->inode.size = sizeof(struct wfs_log_entry) + sizeof(struct wfs_dentry);

    memcpy(newFileEntry->data, &newFile, sizeof(struct wfs_dentry));

    // Update the superblock head to point to the next free space
    superblock->head += sizeof(struct wfs_log_entry) + newFileEntry->inode.size;

    // Synchronize changes
    if (msync(superblock, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        munmap(superblock, DISK_SIZE);
        return -EIO;
    }

    // Unmap the disk file
    if (munmap(superblock, DISK_SIZE) == -1) {
        perror("Error unmapping disk file");
        return -EIO;
    }

    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode) {
    // Map the disk file into memory
    struct wfs_sb *superblock = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);
    if (superblock == MAP_FAILED) {
        perror("Error mapping disk file");
        return -EIO;
    }

    // Use looper to find the parent directory of the path
    char parent_path[MAX_PATH_LEN];
    strcpy(parent_path, path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == NULL) {
        munmap(superblock, DISK_SIZE);
        return -ENOENT;  // Invalid path
    }
    *last_slash = '\0'; // Terminate the string to get the parent path
    struct wfs_log_entry *parent_dir_entry = looper(parent_path, S_IFDIR);
    if (!parent_dir_entry) {
        munmap(superblock, DISK_SIZE);
        return -ENOENT;  // Parent directory not found
    }

    // Prepare the new directory entry
    struct wfs_dentry newDir;
    strncpy(newDir.name, last_slash + 1, MAX_FILE_NAME_LEN);
    newDir.name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    newDir.inode_number = superblock->head + sizeof(struct wfs_log_entry); // Assign inode number after new dir entry

    // Create a new log entry for the parent directory
    struct wfs_log_entry *newParentDirEntry = (struct wfs_log_entry *)((char *)superblock + superblock->head);
    *newParentDirEntry = *parent_dir_entry;
    newParentDirEntry->inode.size += sizeof(struct wfs_dentry);
    memcpy((char *)newParentDirEntry->data + parent_dir_entry->inode.size, &newDir, sizeof(struct wfs_dentry));

    superblock->head += sizeof(struct wfs_log_entry) + newParentDirEntry->inode.size;

    // Create the new log entry for the directory
    struct wfs_log_entry *newDirEntry = (struct wfs_log_entry *)((char *)superblock + superblock->head);
    newDirEntry->inode.mode = mode | S_IFDIR;  // Ensure the mode indicates a directory
    newDirEntry->inode.inode_number = newDir.inode_number;
    newDirEntry->inode.links = 2; // Directories typically have 2 links (".", "..")
    newDirEntry->inode.uid = getuid();
    newDirEntry->inode.gid = getgid();
    newDirEntry->inode.atime = time(NULL);
    newDirEntry->inode.mtime = time(NULL);
    newDirEntry->inode.ctime = time(NULL);
    newDirEntry->inode.size = sizeof(struct wfs_log_entry); // No additional data for the directory

    superblock->head += sizeof(struct wfs_log_entry);

    // Synchronize changes
    if (msync(superblock, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        munmap(superblock, DISK_SIZE);
        return -EIO;
    }

    // Unmap the disk file
    if (munmap(superblock, DISK_SIZE) == -1) {
        perror("Error unmapping disk file");
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