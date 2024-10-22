# Write File System (WFS) Implementation

## Overview

This project implements a single-threaded log-structured filesystem using FUSE (Filesystem in Userspace). The implementation allows users to perform basic file operations without requiring kernel modifications in Linux.

## Features Implemented

- **File Operations**
  - Create empty files
  - Write to existing files (preserving content, supporting non-zero offsets)
  - Read from existing files (supporting non-zero offsets)
  - Remove existing files
- **Directory Operations**

  - Create empty directories
  - Read directory contents
  - Get file/directory attributes

- **Filesystem Attributes**
  - Full support for essential file stats (uid, gid, access time, modification time, mode, links, size)
  - Log-structured design for efficient writes
  - Proper error handling for common scenarios (ENOENT, EEXIST, ENOSPC)

## Technical Specifications

- Maximum file name length: 32 characters
- Maximum path length: 128 characters
- Supported filename characters: letters (a-z, A-Z), numbers (0-9), and underscores (\_)
- Log-structured design without wraparound (requires manual compaction)

## Usage Instructions

### Prerequisites

- Linux environment
- FUSE development libraries installed
- GCC compiler

### Building the Project

```bash
make
```

### Creating and Mounting a Filesystem

1. Create a disk image:

```bash
./create_disk.sh    # Creates a 1MB disk file
```

2. Initialize the filesystem:

```bash
./mkfs.wfs disk    # Initializes the disk file as a WFS filesystem
```

3. Create and mount the filesystem:

```bash
mkdir mnt          # Create mount point
./mount.wfs -f -s disk mnt  # Mount filesystem (-f: foreground, -s: single-threaded)
```

### Basic Operations

After mounting, you can perform standard file operations:

```bash
# Create and write to a file
echo "Hello, WFS!" > mnt/test.txt

# Read file contents
cat mnt/test.txt

# Create a directory
mkdir mnt/mydir

# List contents
ls mnt

# Check file/directory stats
stat mnt/test.txt
```

### Unmounting the Filesystem

```bash
./umount.sh mnt
```

## Debugging Tools

### Inspect Disk Contents

To view raw disk contents before mounting:

```bash
xxd -e -g 4 disk | less
```

To view changes after operations:

```bash
./umount.sh mnt
xxd -e -g 4 disk | less
```

## Error Handling

The implementation handles common error scenarios:

- Attempting to access non-existent files/directories (ENOENT)
- Creating files/directories that already exist (EEXIST)
- Writing when disk space is exhausted (ENOSPC)

## Implementation Notes

- Directory and file names must be unique within the same directory
- The filesystem maintains a log structure without automatic wraparound
- All operations are atomic and maintain filesystem consistency
- File attributes are properly maintained and updated for all operations

## Design Decisions

- Used a log-structured approach for efficient write operations
- Implemented a straightforward superblock design for filesystem metadata
- Maintained proper file and directory hierarchies through careful inode management
