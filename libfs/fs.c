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
};

struct __attribute__((__packed__)) root_dir {
    char filename[16];
    uint32_t file_size;
    uint16_t first_data_block_index;
    char padding[10];
};

struct __attribute__((__packed__)) file_descriptor {
    struct root_dir *file;
    int offset;
};

typedef struct superblock super_block;
super_block sb;
struct root_dir * rd;
struct file_descriptor * fd;
uint16_t *fat_table;

int fs_mount(const char *diskname)
{
    // if disk cannot be opened, return -1
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    // read into super block
    block_read(0, (void*)&sb);

    // error checking to verify that the file system has the expected format
    // check that signature of file system is ECS150FS
    if (memcmp("ECS150FS", sb.signature, 8) != 0) {
        return -1;
    }

    // check that the total number of block corresponds to what block_disk_count() returns
    if (sb.virtual_disk_blocks_count != block_disk_count()) {
        return -1;
    }

    // see slide 9 of proj3 pp
    int fb_count_check = block_disk_count() * 2 / BLOCK_SIZE; // fat block
    int rdb_index_check =  block_disk_count() * 2 / BLOCK_SIZE + 1; // root directory block index
    int db_index_check = block_disk_count() * 2 / BLOCK_SIZE + 2; // data block start index

    if (sb.fat_blocks_count != fb_count_check){
        return -1;
    }

    if (sb.root_directory_block_index != rdb_index_check){
        return -1;
    }

    if (sb.data_block_start_index != db_index_check){
        return -1;
    }

    // create root directory and read into it
    rd = (struct root_dir*)malloc(sizeof(struct root_dir) * FS_FILE_MAX_COUNT);
    block_read(sb.root_directory_block_index, rd);
 
    // create fat table and read into it
    fat_table = malloc(sizeof(uint16_t) * sb.fat_blocks_count * BLOCK_SIZE);
    for (int i=1; i <= sb.fat_blocks_count; i++){
        block_read(i, &fat_table[i*(BLOCK_SIZE/2)]);// 2048 entries per fat block
    }
   
    return 0;
}

int fs_umount(void)
{
    if(block_disk_count() == -1){
        return -1;
    }
    // write all meta info and file data to disk
    block_write(sb.root_directory_block_index, rd);
    
    free(fat_table);
    
    if(block_disk_close() == -1){
        return -1;
    }
    return 0;
    
}

// helper functions
int get_fat_free_blocks(){
    int fat_free_blocks = 0;
    for (int i=0; i<sb.data_blocks_count; i++){
        // Entries marked as 0 correspond to free data blocks
        if (fat_table[i] == 0){
            fat_free_blocks++;
        }
    }
    return fat_free_blocks;
}

int get_rdir_free_blocks(){
    int rdir_free_blocks =0;
    for (int i=0; i< FS_FILE_MAX_COUNT; i++){
       // An empty entry is defined by the first character of the entry’s filename being equal to the NULL character.
        if (rd[i].filename[0] == '\0'){
            rdir_free_blocks++;
        }
    }
    return rdir_free_blocks;
}

// end helper functions

int fs_info(void)
{
    if (block_disk_count() == -1){
        return -1;
    }
    
    int fat_free = get_fat_free_blocks();
    int rdir_free = get_rdir_free_blocks();
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", sb.virtual_disk_blocks_count);
    printf("fat_blk_count=%d\n", sb.fat_blocks_count);
    printf("rdir_blk=%d\n", sb.root_directory_block_index);
    printf("data_blk=%d\n", sb.data_block_start_index);
    printf("data_blk_count=%d\n", sb.data_blocks_count);
    printf("fat_free_ratio=%d/%d\n", fat_free, sb.data_blocks_count);
    printf("rdir_free_ratio=%d/%d\n", rdir_free, FS_FILE_MAX_COUNT);
    return 0;
}

int fs_create(const char *filename)
{
    /* TODO: Phase 2 */
    return 0;
}

int fs_delete(const char *filename)
{
    /* TODO: Phase 2 */
    return 0;
}

int fs_ls(void)
{
    /* TODO: Phase 2 */
    return 0;
}

int fs_open(const char *filename)
{
    /* TODO: Phase 3 */
    return 0;
}

int fs_close(int fd)
{
    /* TODO: Phase 3 */
    return 0;
}

int fs_stat(int fd)
{
    /* TODO: Phase 3 */
    return 0;
}

int fs_lseek(int fd, size_t offset)
{
    /* TODO: Phase 3 */
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    /* TODO: Phase 4 */
    return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
    /* TODO: Phase 4 */
    return 0;
}

