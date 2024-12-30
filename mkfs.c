#include "wfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>



#define INODE_SIZE sizeof(struct wfs_inode)
#define SBLOCK_SIZE sizeof(struct wfs_sb)
#define BLOCK_BITMAP_SIZE 32
#define INODE_BITMAP_SIZE 32
//#define ROUND_UP(x, y) (((x) + (y) - 1) / (y) * (y))

// Round Up probably not right. 17 inodes should return 4 bytes.
//Don't need to write anything other that superblock, write root inode -> mark 1 in inode bitmap and write to inode block
// 1 bit to represent the status of an inode or data block: if 32 inodes --> inodebitmap = 32 bit overall = 4 bytes

int round_up(int num){
    int quo = num / 32;
    int rem = num % 32;
    if(rem != 0){
        quo += 1;
    }
    int rounded = quo * 32;
    return rounded;
}
int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("Usage: ./mkfs -d disk_img -i num_inodes -b num_blocks\n");
        exit(-1);
    }

    char *disk_img;
    size_t num_inodes, num_blocks;

    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                disk_img = argv[i + 1];
            }
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                num_inodes = atoi(argv[i + 1]);
            }
        }
        else if (strcmp(argv[i], "-b") == 0) {
            if (i + 1 < argc) {
                num_blocks = atoi(argv[i + 1]);
            }
        }
    }

    if (num_inodes <= 0 || num_blocks <= 0) {
        fprintf(stderr, "-i and -b cannot be negative");
        exit(-1);
    }

    // Round up the number of blocks to the nearest multiple of 32
    num_blocks = round_up(num_blocks);
    num_inodes = round_up(num_inodes);

    // mmap disk
    int fd = open(disk_img, O_RDWR);
    if(fd == -1){
        perror("Failed to open disk image\n");
        exit(-1);
    }
    struct stat sb;
    if(fstat(fd, &sb) == -1){
        perror("Failed to get size\n");
        exit(-1);
    }
    void* disk = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED){
        perror("Failed to map disk\n");
        close(fd);
        exit(-1);
    }
    close(fd);

    // Initialize superblock
    struct wfs_sb* superblock = (struct wfs_sb*)disk;
    superblock->num_inodes = num_inodes;
    superblock->num_data_blocks = num_blocks;
    superblock->i_bitmap_ptr = SBLOCK_SIZE;
    superblock->d_bitmap_ptr = superblock->i_bitmap_ptr + round_up(num_inodes)/8;
    superblock->i_blocks_ptr = superblock->d_bitmap_ptr + round_up(num_blocks)/8;
    superblock->d_blocks_ptr = superblock->i_blocks_ptr + (num_inodes * BLOCK_SIZE);

    printf("SUPERBLOCK DATA:\n");
    printf("DISK: %p\n",disk);
    printf("Num inodes: %ld\n",superblock->num_inodes);
    printf("Num datablocks: %ld\n",superblock->num_data_blocks);
    printf("I-bitmap-ptr: %p\n",(void*)superblock->i_bitmap_ptr);
    printf("D-bitmap-ptr: %p\n",(void*)superblock->d_bitmap_ptr);
    printf("I-blocks-ptr: %p\n",(void*)superblock->i_blocks_ptr);
    printf("D-blocks-ptr: %p\n",(void*)superblock->d_blocks_ptr);

    // Initialize inode bitmap
    unsigned char inode_bitmap[round_up(num_inodes)/8];
    memset(inode_bitmap, 0, round_up(num_inodes)/ 8);
    inode_bitmap[0] |= 0x01; 
    memcpy((char*)((char*)disk + superblock->i_bitmap_ptr), inode_bitmap, round_up(num_inodes)/8); // Mark root inode as allocated

    // // Initialize data block bitmap
    unsigned char data_bitmap[round_up(num_inodes)/ 8];
    memset(data_bitmap, 0, round_up(num_inodes)/ 8);
   
    // // Write data block bitmap to disk
    // if (fwrite(data_bitmap, round_up(num_inodes)/ 8, 1, disk) != 1) {
    //     perror("Failed to write data block bitmap to disk");
    //     fclose(disk);
    //     exit(1);
    // }

    // Initialize root inode
    struct wfs_inode* root_inode =(struct wfs_inode*) ((char*)disk + superblock->i_blocks_ptr);
    //memset(&root_inode, 0, INODE_SIZE);
    root_inode->num = 0;
    root_inode->mode = __S_IFDIR;
    root_inode->uid = getuid();
    root_inode->gid = getgid();
    root_inode->size = 0;
    root_inode->nlinks = 2;
    time_t current_time = time(NULL);
    root_inode->atim = current_time;
    root_inode->ctim = current_time;
    root_inode->mtim = current_time;
     memset(root_inode->blocks, 0, N_BLOCKS*sizeof(off_t));

    munmap(disk, sb.st_size);
    return 0;
}