# Block-Based Filesystem using FUSE

### Overview
This project implements a block-based filesystem in userspace using the FUSE library. The filesystem supports essential operations like file creation, reading, writing, directory management, and deletion. It was completed as part of a partner project.

### Features
- **File and Directory Management:** Create, remove, read, and write files and directories. Retrieve file and directory attributes.
- **Filesystem Structure:**
  - Superblock for metadata.
  - Inode and data block bitmaps for allocation tracking.
  - Support for direct and single indirect blocks for larger files.
- **Compatibility:** Interacts with standard file operations like `mkdir`, `ls`, `stat`, `echo`, `cat`, and `rm`.
- **Error Handling:** Handles cases like nonexistent files, duplicate names, and insufficient disk space using standard error codes (`-ENOENT`, `-EEXIST`, `-ENOSPC`).

### Project Files
1. `mkfs.c`: Initializes a disk image with a specified number of inodes and data blocks.
2. `wfs.c`: Implements the FUSE filesystem, providing callback functions for operations like `getattr`, `mkdir`, `read`, `write`, and more.

### Setup and Usage
1. Compile the code: `make`
2. Create a Disk Image: `./create_disk.sh`
3. Initialize the Filesystem: `./mkfs -d disk.img -i 32 -b 200`
4. Mount the Filesystem:
   ```
   mkdir mnt
   ./wfs disk.img -f -s mnt
   ```
5. Interact with the Filesystem:
   ```
   mkdir mnt/a
   echo "hello" > mnt/file.txt
   cat mnt/file.txt
   rm mnt/file.txt
   ```
6. Unmount the Filesystem: `./umount.sh mnt`

### Acknowledgments
This project was completed as part of a course assignment in collaboration with my partner. We adhered strictly to the provided project guidelines and specifications.
