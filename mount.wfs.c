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
struct wfs_sb *global_superblock = NULL;

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

int find_new_inode() {
    int newInode = 0;

    struct wfs_log_entry *start_of_log = (struct wfs_log_entry *)((char *)global_superblock + sizeof(struct wfs_sb));
    struct wfs_log_entry *end_of_log = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    struct wfs_log_entry *current_entry = start_of_log;

    while (current_entry < end_of_log) {
        // Check if the current entry is a directory or file with the correct name and inode
        if ((current_entry->inode.inode_number > newInode)) {
            newInode = current_entry->inode.inode_number;
        }
        current_entry = (struct wfs_log_entry *)((char *)current_entry + sizeof(struct wfs_log_entry) + current_entry->inode.size);
    }

    return newInode + 1;
}

struct wfs_log_entry* find_entry_by_inode(int inode) {
    struct wfs_log_entry *start_of_log = (struct wfs_log_entry *)((char *)global_superblock + sizeof(struct wfs_sb));
    struct wfs_log_entry *end_of_log = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    struct wfs_log_entry *current_entry = start_of_log;
    struct wfs_log_entry *found_entry = NULL;

    int found = 0;

    printf("find_entry_by_inode: %d\n", inode);
    while (current_entry < end_of_log) {
        // Check if the current entry is a directory or file with the correct name and inode
        if ((current_entry->inode.inode_number == inode)) {
            
            found_entry = current_entry;
            found = 1;
        }
        // Move to the next log entry
        current_entry = (struct wfs_log_entry *)((char *)current_entry + sizeof(struct wfs_log_entry) + current_entry->inode.size);
    }

    if (!found || found_entry->inode.deleted == 1) { // also check if deleted
        // Token not found in the log, path  does not exist
        printf("Inode %d not found or deleted\n", inode);
        return NULL;
    }

    printf("Found entry by inode %d\n", found_entry->inode.inode_number);
    return found_entry;
}

struct wfs_log_entry* looper(const char *path, mode_t mode) {
    printf("Get looper: %s\n", path);
    // Check if the global superblock has been mapped
    if (!global_superblock) {
        fprintf(stderr, "Disk file not mapped\n");
        return NULL;
    }

    // Start from the root node
    struct wfs_log_entry *start_of_log = (struct wfs_log_entry *)((char *)global_superblock + sizeof(struct wfs_sb));
    struct wfs_log_entry *end_of_log = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    struct wfs_log_entry *current_entry;
    struct wfs_log_entry *found_entry = NULL;

    char *token;
    if (strcmp(path, "/") != 0) {
        char *rest = strdup(path);
        token = strtok(rest, "/");
    } else {
        char *rest = strdup(path);
        token = rest;
    }

    unsigned int current_inode = 0; // Start with root inode    

    printf("%s search start token: %s\n", path, token);

    while (token != NULL) {
        current_entry = start_of_log;
        int found = 0;

        printf("%s searching in looper\n", path);
        while (current_entry < end_of_log) {
            // Check if the current entry is a directory or file with the correct name and inode
            if ((current_entry->inode.inode_number == current_inode)) {
                
                found_entry = current_entry;
                found = 1;
            }
            // Move to the next log entry
            current_entry = (struct wfs_log_entry *)((char *)current_entry + sizeof(struct wfs_log_entry) + current_entry->inode.size);
        }

        if (!found || found_entry->inode.deleted == 1) { // also check if deleted
            // Token not found in the log, path  does not exist
            printf("%s Not found in looper\n", path);
            return NULL;
        }

        // get found_entry
        //////--------------------///////////////////
        // get current_inode
        struct wfs_dentry *current_dentry = (struct wfs_dentry *)found_entry->data;
        int num_entry = found_entry->inode.size / sizeof(struct wfs_dentry);

        // printf("Listing all dentries in found_entry:\n");
        // for (int i = 0; i < num_entry; i++) {
        //     printf("Dentry %d: Name: %s, Inode Number: %lu\n", i, current_dentry->name, current_dentry->inode_number);
        //     current_dentry += 1;
        // }

        // Reset current_dentry to the start position before the main loop
        current_dentry = (struct wfs_dentry *)found_entry->data;
        int old_inode = current_inode;

        printf("%s before with name: %s token: %s\n", path, current_dentry->name, token);
        for (int i = 0; i < num_entry; i++) {
            
            if (strcmp(current_dentry->name, token) == 0) {
                printf("%s made it in here with name: %s token: %s\n", path, current_dentry->name, token);
                current_inode = current_dentry->inode_number;
            }
            current_dentry += 1;
        }

        // Update inode for the next path component
        if (current_inode == old_inode && strcmp(path, "/") != 0) {
            printf("Returning current = old\n");
            return NULL;
        }

        if (strcmp(path, "/") == 0) {
            break;
        }

        token = strtok(NULL, "/"); // Move to next token
    }
    
    return find_entry_by_inode(current_inode); // Return the pointer to the last found entry
}

static int wfs_getattr(const char *path, struct stat *stbuf) {
    printf("GetAttr ******************************\n");
    printf("Get path: %s\n", path);
    memset(stbuf, 0, sizeof(struct stat)); // Initialize the stat structure

    struct wfs_log_entry *entry = looper(path, 0); // Use looper to find the log entry for the path
    if (!entry) {
        printf("Fails here for path: %s\n", path);
        return -ENOENT; // File or directory not found
    }

    // Fill the stat structure with the file/directory attributes
    stbuf->st_mode = entry->inode.mode;
    stbuf->st_nlink = entry->inode.links;
    stbuf->st_uid = entry->inode.uid;
    stbuf->st_gid = entry->inode.gid;
    stbuf->st_size = entry->inode.size;
    stbuf->st_mtime = entry->inode.mtime;

    return 0; // Return success
}

static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    printf("Entering mknod path: %s *********************************\n", path);
    // Check if superblock is properly mapped
    if (!global_superblock) {
        fprintf(stderr, "Superblock not mapped\n");
        return -EIO;
    }

    char parent_path[MAX_PATH_LEN];

    if (strcmp(path, "/") == 0) {
        // Path is the root directory, no parent
        return -EISDIR;
    }

    strcpy(parent_path, path);
    char *last_slash = strrchr(parent_path, '/');

    char new_path[MAX_FILE_NAME_LEN];

    if (last_slash != NULL) {
        // Copy everything after the last slash to new_path
        strncpy(new_path, last_slash + 1, MAX_FILE_NAME_LEN); // Copy up to 4 characters to leave room for the null terminator
        new_path[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    } else {
        // If there's no slash, the whole path is the new path
        strncpy(new_path, path, MAX_FILE_NAME_LEN);
        new_path[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    }

    if (last_slash == parent_path) {
        // If the path is a direct child of root (e.g., "/a")
        strcpy(parent_path, "/");
    } else if (last_slash != NULL) {
        // For deeper paths, find the parent directory
        *last_slash = '\0'; // Terminate the string to get the parent path
    } else {
        // Handle unexpected cases
        return -EINVAL; // Invalid argument
    }

    struct wfs_log_entry *parent_dir_entry = looper(parent_path, S_IFDIR);
    if (!parent_dir_entry) {
        return -ENOENT;  // Parent directory not found
    }

    printf("New path: %s\n", new_path);
    printf("Parent path: %s\n", parent_path);
    // Prepare the new file entry
    struct wfs_dentry newFile;
    strncpy(newFile.name, new_path, MAX_FILE_NAME_LEN);
    newFile.name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    int newInode = find_new_inode();
    newFile.inode_number = newInode; // Assign inode number after new dir entry
    printf("Created new dentry with name %s\n", newFile.name);

    // Create a new log entry for the parent directory
    struct wfs_log_entry *newParentDirEntry = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    *newParentDirEntry = *parent_dir_entry; // Copy the existing parent dir entry
    newParentDirEntry->inode.size = parent_dir_entry->inode.size + sizeof(struct wfs_dentry);
    memcpy(newParentDirEntry->data, parent_dir_entry->data, parent_dir_entry->inode.size);
    memcpy((char *)newParentDirEntry->data , &newFile, sizeof(struct wfs_dentry));

    global_superblock->head += sizeof(struct wfs_log_entry) + newParentDirEntry->inode.size;

    // Create a new log entry for the file
    struct wfs_log_entry *newFileEntry = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    newFileEntry->inode.mode = S_IFREG | mode;
    newFileEntry->inode.inode_number = newInode;
    newFileEntry->inode.links = 1;
    newFileEntry->inode.uid = getuid();
    newFileEntry->inode.gid = getgid();
    newFileEntry->inode.atime = time(NULL);
    newFileEntry->inode.mtime = time(NULL);
    newFileEntry->inode.ctime = time(NULL);
    newFileEntry->inode.size = sizeof(struct wfs_log_entry);

    memcpy(newFileEntry->data, &newFile, sizeof(struct wfs_dentry));

    // Update the superblock head to point to the next free space
    global_superblock->head += sizeof(struct wfs_log_entry) + newFileEntry->inode.size;

    // Synchronize changes
    if (msync(global_superblock, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        return -EIO;
    }

    printf("Reached end of mknod for path: %s\n", path);
    return 0;
}


static int wfs_mkdir(const char *path, mode_t mode) {
    printf("Entering mkdir path: %s >>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    // Check if superblock is properly mapped
    if (!global_superblock) {
        fprintf(stderr, "Superblock not mapped\n");
        return -EIO;
    }

    // Use looper to find the parent directory of the path
    char parent_dir[MAX_PATH_LEN];

    if (strcmp(path, "/") == 0) {
        // Path is the root directory, no parent
        return -EISDIR;
    }

    strcpy(parent_dir, path);
    char *last_slash = strrchr(parent_dir, '/');

    char new_dir[MAX_FILE_NAME_LEN];

    if (last_slash != NULL) {
        // Copy everything after the last slash to new_path
        strncpy(new_dir, last_slash + 1, MAX_FILE_NAME_LEN); // Copy up to 4 characters to leave room for the null terminator
        new_dir[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    } else {
        // If there's no slash, the whole path is the new path
        strncpy(new_dir, path, MAX_FILE_NAME_LEN);
        new_dir[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    }

    if (last_slash == parent_dir) {
        // If the path is a direct child of root (e.g., "/a")
        strcpy(parent_dir, "/");
    } else if (last_slash != NULL) {
        // For deeper paths, find the parent directory
        *last_slash = '\0'; // Terminate the string to get the parent path
    } else {
        // Handle unexpected cases
        return -EINVAL; // Invalid argument
    }

    struct wfs_log_entry *parent_dir_entry = looper(parent_dir, S_IFDIR);
    if (!parent_dir_entry) {
        return -ENOENT;  // Parent directory not found
    }

    printf("New path: %s\n", new_dir);
    printf("Parent path: %s\n", parent_dir);
    // Prepare the new file entry
    struct wfs_dentry newDir;
    strncpy(newDir.name, new_dir, MAX_FILE_NAME_LEN);
    newDir.name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null-termination
    printf("newDir name: %s\n", newDir.name);
    int newInode = find_new_inode();
    newDir.inode_number = newInode; // Assign inode number after new dir entry
    printf("Created new dentry with name %s and inode: %ld\n", newDir.name, newDir.inode_number);

    struct wfs_dentry *current_dentry = (struct wfs_dentry *)parent_dir_entry->data;
    int num_entry = parent_dir_entry->inode.size / sizeof(struct wfs_dentry);
    printf("Listing all dentries in parent_dir_entry\n");
    for (int i = 0; i < num_entry; i++) {
        printf("Dentry %d: Name: %s, Inode Number: %lu\n", i, current_dentry->name, current_dentry->inode_number);
        current_dentry += 1;
    }


    // Create a new log entry for the parent directory
    struct wfs_log_entry *newParentDirEntry = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    *newParentDirEntry = *parent_dir_entry;
    newParentDirEntry->inode.size = parent_dir_entry->inode.size + sizeof(struct wfs_dentry);
    memcpy(newParentDirEntry->data, parent_dir_entry->data, parent_dir_entry->inode.size);
    memcpy((char *)newParentDirEntry->data , &newDir, sizeof(struct wfs_dentry));

    global_superblock->head += sizeof(struct wfs_log_entry) + newParentDirEntry->inode.size;

    current_dentry = (struct wfs_dentry *)newParentDirEntry->data;
    num_entry = newParentDirEntry->inode.size / sizeof(struct wfs_dentry);
    printf("Listing all dentries in new parent_dir_entry\n");
    for (int i = 0; i < num_entry; i++) {
        printf("Dentry %d: Name: %s, Inode Number: %lu\n", i, current_dentry->name, current_dentry->inode_number);
        current_dentry += 1;
    }


    // Create the new log entry for the directory
    struct wfs_log_entry *newDirEntry = (struct wfs_log_entry *)((char *)global_superblock + global_superblock->head);
    newDirEntry->inode.mode = S_IFDIR | mode;  // Ensure the mode indicates a directory
    newDirEntry->inode.inode_number = newInode;
    newDirEntry->inode.links = 2; // Directories typically have 2 links (".", "..")
    newDirEntry->inode.uid = getuid();
    newDirEntry->inode.gid = getgid();
    newDirEntry->inode.atime = time(NULL);
    newDirEntry->inode.mtime = time(NULL);
    newDirEntry->inode.ctime = time(NULL);
    newDirEntry->inode.size = sizeof(struct wfs_log_entry); // No additional data for the directory

    memcpy(newDirEntry->data, &newDir, sizeof(struct wfs_dentry));

    // Update the superblock head to point to the next free space
    global_superblock->head += sizeof(struct wfs_log_entry) + newDirEntry->inode.size;

    // Synchronize changes
    if (msync(global_superblock, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        return -EIO;
    }

    printf("Reached end of mkdir for path: %s\n", path);
    return 0;
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Use looper to find the log entry for the file
    struct wfs_log_entry *file_entry = looper(path, 0);
    printf("Read path: %s", path);
    if (!file_entry) {
        return -ENOENT; // File not found
    }

    // Check if the offset is valid
    if (offset >= file_entry->inode.size) {
        return -EINVAL; // Invalid argument
    }

    // Calculate the size to read
    size_t available_size = file_entry->inode.size - offset;
    size_t read_size = (size < available_size) ? size : available_size;

    // Read the data
    memcpy(buf, (char *)file_entry->data + offset, read_size);

    // Return the number of bytes read
    return read_size;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Use looper to find the log entry for the file
    struct wfs_log_entry *file_entry = looper(path, 0);
    printf("Write path: %s", path);
    if (!file_entry) {
        return -ENOENT; // File not found
    }

    // Check if the offset is valid
    if (offset > file_entry->inode.size) {
        return -EINVAL; // Invalid argument
    }

    // Calculate the size to write
    size_t available_size = file_entry->inode.size - offset;
    size_t write_size = (size < available_size) ? size : available_size;

    // Write the data at the specified offset
    memcpy((char *)file_entry->data + offset, buf, write_size);

    // Update the file size if necessary
    if (offset + write_size > file_entry->inode.size) {
        file_entry->inode.size = offset + write_size;
    }

    // Update the modification time
    file_entry->inode.mtime = time(NULL);

    // Synchronize changes
    if (msync(global_superblock, DISK_SIZE, MS_SYNC) == -1) {
        perror("Error syncing changes");
        return -EIO;
    }

    // Return the number of bytes written
    return write_size;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    return 0;
}

static int wfs_unlink(const char *path) {
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <disk_image> <mount_point> [FUSE options]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Open the disk image file
    disk_fd = open(argv[argc - 2], O_RDWR);
    if (disk_fd == -1) {
        perror("Error opening disk file");
        return EXIT_FAILURE;
    }

    // Map the disk image into memory
    global_superblock = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);
    if (global_superblock == MAP_FAILED) {
        perror("Error mapping disk file");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    // Adjust argv for FUSE
    argv[argc - 2] = argv[argc - 1];
    argv[argc - 1] = NULL;
    argc--;

    int fuse_stat = fuse_main(argc, argv, &ops, NULL);

    // Unmap the disk file and close it
    munmap(global_superblock, DISK_SIZE);
    close(disk_fd);

    return fuse_stat;
}