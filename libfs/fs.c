#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define BLOCK_SIZE 4096
#define FAT_EOC 0xFFFF

struct __attribute__((__packed__)) superblock {
    char signature[8];
    uint16_t virtual_disk_blocks_count;
    uint16_t root_directory_block_index;
    uint16_t data_block_start_index;
    uint16_t data_blocks_count;
    uint8_t fat_blocks_count;
    char padding[BLOCK_SIZE];
}

struct __attribute__((__packed__)) root_dir {
    char filename[16];
    uint32_t file_size;
    uint16_t first_data_block_index;
    char padding[10];
}

struct __attribute__((__packed__)) file_descriptor {
    struct root_dir *file;
    int offset;
}

int fs_mount(const char *diskname)
{
    // if disk cannot be opened, return -1
    if (block_disk_open(diskname) == -1) {
        return -1;
    }
    
    // create and read into super block
    super_block = malloc(sizeof stuct super_block));
    block_read(0, super_block);
    
    
    // error checking to verify that the file system has the expected format
    
    // check that signature of file system is ECS150FS
    if (strncmp("ECS150FS", super_block->signature, 8) != 0) {
        return -1;
    }
    
    // check that the total number of block corresponds to what block_disk_count() returns
    if (data_blocks_count != block_disk_count() {
        return -1;
    }
        
    // create root directory and check if root directory block index is correct
    
    
    return 0;
}

int fs_umount(void)
{
    /* TODO: Phase 1 */
}

int fs_info(void)
{
    /* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
    /* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
    /* TODO: Phase 2 */
}

int fs_ls(void)
{
    /* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
    /* TODO: Phase 3 */
}

int fs_close(int fd)
{
    /* TODO: Phase 3 */
}

int fs_stat(int fd)
{
    /* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
    /* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
    /* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
    /* TODO: Phase 4 */
}

