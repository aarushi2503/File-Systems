#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <libgen.h> // for mknod
#include <math.h>

char *disk_img;
void *disk;

// USE memcopy, mmap
// DO NOT use write

// Start with writing sime helper functions
// inside main: mmap disc image and start running fuse (fuse should be last thing to do)

// Helper fucntions: all 7 functions take a char* path
// Writing a hlper function inode_from_path is helpful

// Data offset will take a inode and offset in that indode (file and contents in file --> wat pointer that points to offset)
// Becasue data blocks are non-contig --> diff to locate

void *map_disk()
{
    printf("Map disk\n");
    int fd = open(disk_img, O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open disk image\n");
        return NULL;
    }
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("Failed to get size\n");
        return NULL;
    }
    void *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("Failed to map disk\n");
        close(fd);
        return NULL;
    }
    close(fd);
    return addr;
}

int is_inode_allocated(int num)
{
    // printf("Is node alloced\n");
    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    char *inode_bitmap_addr = ((char *)disk + superblock->i_bitmap_ptr);
    // int *inode_bitmap = (int *)inode_bitmap_addr;

    off_t idx = num / 8;
    off_t offset = num % 8;
    char byte = inode_bitmap_addr[idx];

    return (byte & (1 << offset)) != 0;
}

struct wfs_inode *inode_from_path(const char *path)
{
    printf("inode from path\n");
    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    struct wfs_inode *inode = (struct wfs_inode *)((char *)disk + superblock->i_blocks_ptr);

    // Check root inode
    if (strcmp(path, "/") == 0)
    {
        printf("returned root\n");
        return inode;
    }

    char *token;
    char *path_copy = strdup(path);
    printf("Path:%s\n", path);
    // printf("Path copy:%s\n", path_copy);
    int found = 0;
    int fin_found = 0;
    char *base_path = strdup(path);
    char *base_copy = basename(base_path);

    while ((token = strtok_r(path_copy, "/", &path_copy)))
    {
        printf("TOKEN:%s\n", token);
        for (int i = 0; i < N_BLOCKS; i++)
        {
            if (inode->blocks[i] != 0)
            {
                struct wfs_dentry *entry = (struct wfs_dentry *)((char *)disk + (inode->blocks[i]));
                for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
                {
                    // printf("Comparing Entry:%s Token:%s\n", entry[j].name, token);
                    if (strcmp(entry[j].name, token) == 0)
                    {
                        if (strcmp(base_copy, entry[j].name) == 0)
                        {
                            fin_found = 1;
                        }
                        // printf("Name match! Entry:%s Token:%s\n", entry[j].name, token);
                        inode = (struct wfs_inode *)((char *)disk + superblock->i_blocks_ptr + (entry[j].num * BLOCK_SIZE));
                        // printf("Found Inode Number: %d\n", inode->num);
                        // printf("Entry Num: %d\n", entry[j].num);
                        found = 1;
                        break;
                    }
                }
            }
            if (found == 1)
            {
                printf("FOUND!!!\n");
                break;
            }
        }
        if (fin_found == 1)
        {
            // printf("FIN FOUND TRUE\n");
            break;
        }
    }

    if (found == 0 || fin_found == 0 || !is_inode_allocated(inode->num))
    {
        printf("returned NULL\n");
        return NULL;
    }

    printf("INODE FOUND DATA:\n");
    printf("num:%d\n", inode->num);
    printf("mode:%d\n", inode->mode);
    printf("nlinks:%d\n", inode->nlinks);
    printf("size:%ld\n", inode->size);

    return inode;
}

int is_data_block_allocated(off_t num)
{
    printf("is d block alloced\n");
    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    char *data_bitmap_addr = ((char *)disk + superblock->d_bitmap_ptr);
    // int *data_bitmap = (int *)data_bitmap_addr;

    off_t byte_idx = num / 8;
    off_t bit_off = num % 8;
    char byte = data_bitmap_addr[byte_idx];

    return (byte & (1 << bit_off)) != 0;
}

off_t allocate_data_block()
{
    printf("alloc data block\n");
    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    for (off_t i = 0; i < superblock->num_data_blocks; i++)
    {
        if (!is_data_block_allocated(i))
        {
            // allocate data bitmap at i
            struct wfs_sb *superblock = (struct wfs_sb *)disk;
            char *data_bitmap_addr = ((char *)disk + superblock->d_bitmap_ptr);
            // int *data_bitmap = (int *)data_bitmap_addr;
            size_t idx = i / 8;
            size_t offset = i % 8;
            data_bitmap_addr[idx] |= 1 << offset;
            return superblock->d_blocks_ptr + i * BLOCK_SIZE;
        }
    }
    return -1;
}
static int wfs_getattr(const char *path, struct stat *stbuf)
{
    printf("getattr\n");
    struct wfs_inode *inode = inode_from_path(path);
    if (inode == NULL)
    {
        return -ENOENT; // failure
    }
    printf("Getattr inode num: %d\n", inode->num);
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_ino = inode->num;
    printf("Getattr copied num: %ld\n", stbuf->st_ino);
    stbuf->st_mode = inode->mode;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_nlink = inode->nlinks;

    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_ctime = inode->ctim;

    // stbuf->st_blocks = inode->blocks;
    printf("Finished getattr\n");
    return 0; // success
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("\nmknod\n");
    char *path_copy = strdup(path);

    struct wfs_inode *parent = inode_from_path(dirname((char *)path_copy));
    if (parent == NULL)
    {
        return -ENOENT;
    }
    // check if inode already exists
    struct wfs_inode *inode = inode_from_path(path);
    if (inode != NULL)
    {
        return -EEXIST;
    }

    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    int inode_num = -1;
    for (int i = 0; i < superblock->num_inodes; i++)
    {
        if (!is_inode_allocated(i))
        {
            inode_num = i;
            break;
        }
    }
    if (inode_num == -1)
    {
        // no space
        return -ENOSPC;
    }
    printf("inode_num = %d\n", inode_num);

    // allocate new inode
    char *inode_bitmap_addr = ((char *)disk + superblock->i_bitmap_ptr);
    //int *inode_bitmap = (int *)inode_bitmap_addr;
    off_t idx = inode_num / 8;
    off_t bit_offset = inode_num % 8;
    printf("idx: %ld, bit_offset = %ld\n", idx, bit_offset);
    inode_bitmap_addr[idx] |= (0x1 << bit_offset);

    printf("Allocated bitmap inode\n");
    // create new inode
    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)disk + superblock->i_blocks_ptr + inode_num * BLOCK_SIZE); // sizeof(struct wfs_inode));
    new_inode->num = inode_num;
    new_inode->mode = mode | __S_IFREG;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;
    new_inode->nlinks = 1;
    time_t current_time = time(NULL);
    new_inode->atim = current_time;
    new_inode->mtim = current_time;
    new_inode->ctim = current_time;
    memset(new_inode->blocks, 0, N_BLOCKS * sizeof(off_t));
    printf("Added new inode\n");
    // Add directory entry to parent inode
    for (int i = 0; i < N_BLOCKS; i++)
    {
        if (parent->blocks[i] != 0)
        {
            struct wfs_dentry *entries = (struct wfs_dentry *)((char *)disk + parent->blocks[i]);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                if (entries[j].num == 0)
                {
                    char *base_copy = strdup(path);
                    strcpy(entries[j].name, basename(base_copy));
                    entries[j].num = inode_num;
                    parent->mtim = current_time;
                    return 0; // success
                }
            }
        }
        else
        {
            // Allocate new block for directory entries
            printf("----- new data block \n");
            off_t block_num = allocate_data_block();
            if (block_num == -1)
            {
                return -ENOSPC;
            }
            parent->blocks[i] = block_num;
            struct wfs_dentry *entries = (struct wfs_dentry *)((char *)disk + block_num);
            char *base_copy = strdup(path);
            strcpy(entries[0].name, basename(base_copy));
            entries[0].num = inode_num;
            parent->mtim = current_time;
            return 0; // success
        }
    }

    return -ENOSPC; // failure
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    printf("mkdir\n");
    // Get parent inode
    char *path_copy = strdup(path);
    struct wfs_inode *parent = inode_from_path(dirname(path_copy));
    printf("PARENT inode num:%d \n", parent->num);
    if (parent == NULL)
    {
        printf("Parent not found\n");
        // Parent directory does not exist
        return -ENOENT;
    }
    struct wfs_inode *exists = inode_from_path(path);
    if (exists != NULL)
    {
        printf("Dir exists why?!\n");
        // directory already exists
        return -EEXIST;
    }

    struct wfs_sb *superblock = (struct wfs_sb *)disk;

    // Find available inode
    int inode_num = -1;
    for (int i = 0; i < superblock->num_inodes; i++)
    {
        if (!is_inode_allocated(i))
        {
            inode_num = i;
            break;
        }
    }
    if (inode_num == -1)
    {
        // no space
        return -ENOSPC;
    }
    // allocate new inodex
    char *inode_bitmap_addr = ((char *)disk + superblock->i_bitmap_ptr);
    // int *inode_bitmap = (int *)inode_bitmap_addr;
    size_t idx = inode_num / 8;
    size_t bit_offset = inode_num % 8;
    inode_bitmap_addr[idx] |= (1 << bit_offset);

    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)disk + superblock->i_blocks_ptr + inode_num * BLOCK_SIZE); // sizeof(struct wfs_inode));

    // Initialize new inode
    new_inode->num = inode_num;
    new_inode->mode = mode | __S_IFDIR; // Check if need to specify directory
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0;
    new_inode->nlinks = 2;
    time_t current_time = time(NULL);
    new_inode->atim = current_time;
    new_inode->mtim = current_time;
    new_inode->ctim = current_time;
    memset(new_inode->blocks, 0, N_BLOCKS * sizeof(off_t));

    // Update parent directory
    // off_t block_off = allocate_data_block();
    // if(block_off == -1){
    //     return -ENOSPC;
    // }

    // parent->nlinks++;
    // parent->mtim = current_time;

    // // Add . and .. to new directory
    // // struct wfs_dentry* entries = (struct wfs_dentry*)((char*)disk + block_off);
    // // strcpy(entries[0].name, ".");
    // // entries[0].num = inode_num;
    // // strcpy(entries[1].name, "..");
    // // entries[1].num = parent->num;

    // // Update parent's block array
    // for(int i = 0; i < N_BLOCKS; i++){
    //     if(parent->blocks[i] == 0){
    //         parent->blocks[i] = block_off;
    //         break;
    //     }
    // }
    // Add directory entry to parent inode
    for (int i = 0; i < N_BLOCKS; i++)
    {
        if (parent->blocks[i] != 0)
        {
            struct wfs_dentry *entries = (struct wfs_dentry *)((char *)disk + parent->blocks[i]);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                if (entries[j].num == 0)
                {
                    char *base_copy = strdup(path);
                    strcpy(entries[j].name, basename(base_copy));
                    entries[j].num = inode_num;
                    parent->mtim = current_time;
                    return 0; // success
                }
            }
        }
        else
        {
            // Allocate new block for directory entries
            off_t block_num = allocate_data_block();
            if (block_num == -1)
            {
                return -ENOSPC;
            }
            parent->blocks[i] = block_num;
            struct wfs_dentry *entries = (struct wfs_dentry *)((char *)disk + block_num);
            char *base_copy = strdup(path);
            strcpy(entries[0].name, basename(base_copy));
            entries[0].num = inode_num;
            parent->mtim = current_time;
            return 0; // success
        }
    }
    parent->size += sizeof(struct wfs_dentry);
    return 0; // success
}

static int wfs_unlink(const char *path)
{
    printf("unlink\n");

    struct wfs_inode *inode = inode_from_path(path);
    if (inode == NULL)
    {
        // File doesn't exist
        return -ENOENT;
    }

    // Only files can be unlinked, check if inode is directory
    if (S_ISDIR(inode->mode))
    {
        return 1;
    }

    struct wfs_sb *superblock = (struct wfs_sb *)disk;
    // Decrement links
    inode->nlinks--;

    // check if last hard link
    if (inode->nlinks == 0)
    {
        // Free data blocks and update bitmap
        for (int i = 0; i < N_BLOCKS; i++)
        {
            if (inode->blocks[i] != 0)
            {
                off_t block_offset = inode->blocks[i];
                char *data_bitmap_addr = (char *)disk + superblock->d_bitmap_ptr;
                // int *data_bitmap = (int *)data_bitmap_addr;
                size_t block_idx = (block_offset - superblock->d_blocks_ptr) / BLOCK_SIZE;
                data_bitmap_addr[block_idx / 8] &= ~(1 << (block_idx % 8));
                inode->blocks[i] = 0;
            }
        }

        // Mark inode as unallocated in inode bitmap
        char *inode_bitmap_addr = (char *)disk + superblock->i_bitmap_ptr;
        // int *inode_bitmap = (int *)inode_bitmap_addr;
        inode_bitmap_addr[inode->num / 8] &= ~(1 << (inode->num % 8));

        // Update parent
        char *path_copy = strdup(path);
        struct wfs_inode *parent = inode_from_path(dirname(path_copy));
        if (parent != NULL)
        {
            parent->mtim = time(NULL);
        }
    }
    return 0; // success
}

static int wfs_rmdir(const char *path)
{
    printf("rmdir\n");
    struct wfs_inode *inode = inode_from_path(path);
    if (inode == NULL)
    {
        return -ENOENT;
    }

    // Remove directory from parent
    char *path_copy = strdup(path);
    struct wfs_inode *parent = inode_from_path(dirname(path_copy));
    if (parent == NULL)
    {
        return -ENOENT;
    }
    char *base_copy = strdup(path);
    for (int i = 0; i < N_BLOCKS; i++)
    {
        if (parent->blocks[i] != 0)
        {
            struct wfs_dentry *entries = (struct wfs_dentry *)((char *)disk + parent->blocks[i]);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                if (entries[j].num != 0 && strcmp(entries[j].name, basename(strdup(base_copy))) == 0)
                {
                    entries[j].num = 0;
                    entries[j].name[0] = '\0'; // Null character
                    parent->mtim = time(NULL);
                    return 0; // success
                }
            }
        }
    }
    parent->size -= sizeof(struct wfs_dentry);
    return -ENOENT; // failure
}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("\nread\n");
    struct wfs_inode *inode = inode_from_path(path);
    if (inode == NULL)
    {
        // printf("INODE NOT FOUND\n");
        return -ENOENT;
    }

    // Check if offset is within size
    if (offset >= inode->size)
    {
        // printf("OFFSET GREATER THAN FILE SIZE\n");
        return 0;
    }

    size_t rem_bytes = inode->size - offset;
    size_t to_read_bytes = size;
    if (size >= rem_bytes)
    {
        to_read_bytes = rem_bytes;
    }

    // printf("to_read_bytes: %zu\n", to_read_bytes);
    // printf("rem_bytes: %zu\n", rem_bytes);

    int start = (int)(offset / BLOCK_SIZE);
    off_t block_off = offset % BLOCK_SIZE;

    // printf("start: %d\n", start);
    // printf("block_off: %ld\n", block_off);

    // Read data into buffer
    size_t read = 0;
    size_t total_read = 0;
    while (to_read_bytes > 0 && start < N_BLOCKS - 1)
    {
        off_t dblock_num = inode->blocks[start];
        if (dblock_num == 0)
        {
            // No more data
            // printf("NO MORE DATA BLOCKS\n");
            break;
        }

        off_t read_off = dblock_num + block_off;
        size_t read_size = BLOCK_SIZE - block_off;
        if (to_read_bytes < (BLOCK_SIZE - block_off))
        {
            read_size = to_read_bytes;
        }

        // printf("Reading from direct block: %ld\n", dblock_num);
        // printf("read_off: %ld, read_size: %zu\n", read_off, read_size);
        memcpy(buf + total_read, (char *)disk + read_off, read_size);

        // Update counters
        read = read_size;
        total_read += read;
        to_read_bytes -= read;
        start++;
        block_off = 0;

        // printf("read_size: %zu\n", read_size);
        // printf("total_read: %zu\n", total_read);
        // printf("start: %d\n", start);
        // printf("to_read_bytes: %zu\n", to_read_bytes);
    }

    if (to_read_bytes > 0)
    {
        start = N_BLOCKS - 1;
        if (inode->blocks[start] == 0)
        {
            // printf("NO MORE INDIRECT BLOCKS\n");
            return total_read; // No more to read
        }

        // Read from indirect block
        off_t indirect_block = inode->blocks[start];
        size_t indirect_entries = BLOCK_SIZE / sizeof(off_t);
        off_t *indirect_array = (off_t *)((char *)disk + indirect_block);
        block_off = offset % BLOCK_SIZE;
        for (size_t i = 0; i < indirect_entries && to_read_bytes > 0; i++)
        {
            //    if(i>0){
            //     block_off = 0;
            //     }
            off_t read_off = indirect_array[i];
            size_t read_size = to_read_bytes;
            if (to_read_bytes > (BLOCK_SIZE))
            {
                read_size = BLOCK_SIZE;
            }

            // printf("Reading from indirect block: %ld\n", indirect_array[i]);
            // printf("read_off: %ld, read_size: %zu\n", read_off, read_size);

            memcpy(buf + total_read, (char *)disk + block_off + read_off, read_size);
            // printf("read content: %x\n", ((char*)(buf + total_read))[0]);
            to_read_bytes -= read_size;
            total_read += read_size;

            // printf("read_size: %zu\n", read_size);
            // printf("total_read: %zu\n", total_read);
            // printf("to_read_bytes: %zu\n", to_read_bytes);
            // block_off = 0;
        }
    }
    return total_read; // success
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("\nwrite off %lu size %lu \n", offset, size);
    struct wfs_inode *inode = inode_from_path(path);
    if (inode == NULL || S_ISDIR(inode->mode))
    {
        return -ENOENT;
    }

    off_t end = offset + size;
    // Update inode size
    if (end > inode->size)
    {
        inode->size = end;
    }
    // printf("WRITE: Calculated end\n");
    size_t to_write = size;
    if (size + offset > inode->size)
    {
        // printf("Shouldn;t get here\n");
        to_write = inode->size - offset;
    }
    // printf("WRITE: towrite:%ld\n", to_write);
    int start_block = floor(offset / BLOCK_SIZE);
    off_t block_off = offset % BLOCK_SIZE;

    // Write data to file
    size_t written = 0;
    while (to_write > 0 && start_block < N_BLOCKS - 1)
    {
        // printf("start_block (begin): %d\n", start_block);
        off_t block = inode->blocks[start_block];
        if (block == 0)
        {
            // Allocate new block
            block = allocate_data_block();
            // printf("Allocated block\n");
            if (block == -1)
            {
                return -ENOSPC;
            }
            inode->blocks[start_block] = block;
        }
        // printf("WRITE: Calculating write offset\n");
        off_t write_off = block + block_off;
        size_t write_size = to_write;
        if (to_write > (BLOCK_SIZE - block_off))
        {
            write_size = BLOCK_SIZE - block_off;
        }

        // printf("write offset: %ld\n", write_off);
        // printf("write size: %zu\n", write_size);

        memcpy(((char *)disk + write_off), buf + written, write_size);
        // Update counters
        written += write_size;
        to_write -= write_size;
        start_block++;

        // printf("WRITE: towrite update:%ld\n", to_write);
        // printf("WRITE: written updated:%ld\n", written);

        block_off = 0;
    }
    // if to_write > 0 and start block == NBLOCKS, allocate indirect block
    // indirect block is an array of offsets
    // size of array = BLOCK_SIZE/ sizeof(off_t)
    if (to_write > 0)
    {
        // printf("indirect\n");
        start_block = N_BLOCKS - 1; // Set start_block to point to the indirect block
        // Check if the indirect block is already allocated
        off_t indirect_block = inode->blocks[start_block];
        if (indirect_block == 0)
        {
            // Allocate new indirect block
            indirect_block = allocate_data_block();
            if (indirect_block == -1)
            {
                return -ENOSPC;
            }
            // printf("ALLOC (indirect)\n");
            inode->blocks[start_block] = indirect_block;
        }

        // Calculate the number of entries in the indirect block
        int indirect_entries = BLOCK_SIZE / sizeof(off_t);
        off_t *indirect_array = (off_t *)((char *)disk + indirect_block);

        block_off = offset % BLOCK_SIZE;
        // Loop through the indirect block entries
        // printf("BEFORE LOOP (indirect)\n");
        for (int i = 0; i < indirect_entries && to_write > 0; i++)
        {
            // printf("INSIDE LOOP (indirect): %d\n", i);
            // if(i>0){
            //     block_off = 0;
            // }
            // Check if the indirect block entry is already allocated
            if (indirect_array[i] == 0)
            {
                // Allocate new data block
                off_t data_block = allocate_data_block();
                if (data_block == -1)
                {
                    return -ENOSPC;
                }
                indirect_array[i] = data_block;
                // printf("ALLOC ARRAY (indirect)\n");
            }
            else
            {
                off_t idx = indirect_array[i];
                struct wfs_sb *super = (struct wfs_sb *)disk;
                idx -= super->d_blocks_ptr;
                idx /= BLOCK_SIZE;
                if (is_data_block_allocated(idx))
                {
                    // printf("INDEX CONT (indirect), %d\n", i);
                    continue;
                }
            }

            // Write data to the data block pointed by the indirect block entry
            off_t write_off = indirect_array[i];
            // printf("----- i: %d\n", i);
            size_t write_size = to_write;
            if (to_write > (BLOCK_SIZE))
            {
                write_size = BLOCK_SIZE;
            }

            memcpy(((char *)disk + block_off + write_off), buf + written, write_size);
            // printf("write content: %x\n", ((char*)(buf + written))[0]);
            // Update counters
            written += write_size;
            to_write -= write_size;
            // block_off = 0;
        }
    }
    if (to_write > 0)
    {
        return -ENOSPC;
    }

    inode->mtim = time(NULL);
    printf("WRITE: written final val:%ld\n", written);

    return written; // success
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    printf("readdir\n");
    struct wfs_inode *inode = inode_from_path(path);
    // struct wfs_sb* superblock = (struct wfs_sb*)disk;
    // void* data_block_addr = (char*)disk + superblock->d_blocks_ptr;
    if (inode == NULL)
    {
        return -ENOENT;
    }
    printf("Inode number:%d\n", inode->num);
    // filler with . and ..
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    if (S_ISDIR(inode->mode))
    {
        // is a directory, iterate through all entries
        for (int i = 0; i < N_BLOCKS; i++)
        {
            if (inode->blocks[i] == 0)
            {
                // unused
                continue;
            }
            printf("offset: %ld\n", inode->blocks[i]);
            struct wfs_dentry *dir = (struct wfs_dentry *)((char *)disk + inode->blocks[i]);
            for (int j = 0; j < BLOCK_SIZE / sizeof(struct wfs_dentry); j++)
            {
                // iterate over entries and add to buffer
                if (dir[j].num != 0)
                {
                    printf("entered if statement\n");
                    filler(buf, dir[j].name, NULL, 0);
                    printf("Name:%s\n", dir[j].name);
                }
            }
        }
        return 0; // success
    }
    else
    {
        // not a directory, return error
        return -ENOENT;
    }
}

static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir,
    .read = wfs_read,
    .write = wfs_write,
    .readdir = wfs_readdir,
};

int main(int argc, char *argv[])
{
    // check at least 4 arguments are passed
    if (argc < 4)
    {
        perror("Usage: ./wfs disk_path [FUSE options] mount_point\n");
        exit(1);
    }
    disk_img = argv[1];
    disk = map_disk();
    int fuse_argc = argc - 1; // remove ./wfs and disk_img
    char *fuse_argv[fuse_argc];
    fuse_argv[0] = "./wfs";
    int j = 1; // fuse idx
    for (int i = 2; i < argc; i++)
    {
        fuse_argv[j] = argv[i];
        j++;
    }
    // Initialize FUSE with specified operations
    // Filter argc and argv here then pass it to fuse_main
    return fuse_main(fuse_argc, fuse_argv, &ops, NULL);
    // OS will unmap disk / do it manually (save val from fuse main then unmap)
}
