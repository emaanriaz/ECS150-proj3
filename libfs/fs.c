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
    char filename[16];
    int fd_return;
    int offset;
};

typedef struct superblock super_block;
typedef struct file_descriptor fd_t;
super_block sb;
fd_t file_d[FS_OPEN_MAX_COUNT];
struct root_dir * rd;
uint8_t open_files = 0;
uint16_t *fat_table;

/**
 * fs_mount - Mount a file system
 * @diskname: Name of the virtual disk file
 *
 * Open the virtual disk file @diskname and mount the file system that it
 * contains. A file system needs to be mounted before files can be read from it
 * with fs_read() or written to it with fs_write().
 *
 * Return: -1 if virtual disk file @diskname cannot be opened, or if no valid
 * file system can be located. 0 otherwise.
 */
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

    // create root directory and read into it
    rd = (struct root_dir*)malloc(sizeof(struct root_dir) * FS_FILE_MAX_COUNT);
    block_read(sb.root_directory_block_index, rd);

    // create fat table and read into it
    fat_table = malloc(sizeof(uint16_t) * sb.fat_blocks_count * BLOCK_SIZE);
    for (int i=0; i <= sb.fat_blocks_count; i++){
        block_read(i+1, &fat_table[i*(BLOCK_SIZE/2)]);
    }
   
    return 0;
}

/**
 * fs_umount - Unmount file system
 *
 * Unmount the currently mounted file system and close the underlying virtual
 * disk file.
 *
 * Return: -1 if no FS is currently mounted, or if the virtual disk cannot be
 * closed, or if there are still open file descriptors. 0 otherwise.
 */
int fs_umount(void)
{
    if(block_disk_count() == -1){
        return -1;
    }
    // write all meta info and file data to disk
    block_write(sb.root_directory_block_index, rd);
    
    for (int i=0; i<sb.fat_blocks_count;i++){
        block_write(i+1, &fat_table[i*(BLOCK_SIZE/2)]);
    }
    
    
    memset(sb.signature, '\0', 8);
    sb.fat_blocks_count = 0;
    sb.virtual_disk_blocks_count = 0;
    sb.data_block_start_index = 0;
    sb.root_directory_block_index = 0;

    free(fat_table);
    free(rd);

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

/**
 * fs_info - Display information about file system
 *
 * Display some information about the currently mounted file system.
 *
 * Return: -1 if no underlying virtual disk was opened. 0 otherwise.
 */
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

/**
 * fs_create - Create a new file
 * @filename: File name
 *
 * Create a new and empty file named @filename in the root directory of the
 * mounted file system. String @filename must be NULL-terminated and its total
 * length cannot exceed %FS_FILENAME_LEN characters (including the NULL
 * character).
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if a
 * file named @filename already exists, or if string @filename is too long, or
 * if the root directory already contains %FS_FILE_MAX_COUNT files. 0 otherwise.
 */
int fs_create(const char *filename)
{
    if (filename == NULL || strlen(filename) > FS_FILENAME_LEN){
        return -1;
    }

    int count =0;
    // iterate over root directory
    for (int i=1; i<FS_FILE_MAX_COUNT; i++){
        if (strcmp(rd[i].filename, filename) == 0 ){
            return -1;
        }
        
        if (rd[i].filename[0] != '\0'){
            count++;
        }
         else if (rd[i].filename[0] == '\0'){
           // found empty entry, now fill it with file name, set size to 0, and set index to FAT_EOC
           memcpy(rd[i].filename, filename, FS_FILENAME_LEN);
           fat_table[i] = i;
           rd[i].file_size = 0;
           rd[i].first_data_block_index = FAT_EOC;
           break;
        }
    }
    
    if(count > FS_FILE_MAX_COUNT){
        return -1;
    }

    return -1;
}
/**
 * fs_delete - Delete a file
 * @filename: File name
 *
 * Delete the file named @filename from the root directory of the mounted file
 * system.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * Return: -1 if @filename is invalid, if there is no file named @filename to
 * delete, or if file @filename is currently open. 0 otherwise.
 */
int fs_delete(const char *filename)
{
    if (filename == NULL || strlen(filename) > FS_FILENAME_LEN){
        return -1;
    }

    int found = -1;
    uint16_t current_index = FAT_EOC;
    for (int i=1; i<FS_FILE_MAX_COUNT; i++){
        if (rd[i].filename[0] != '\0'){
            // find the file and set entry name back to null
            if (strcmp((char*)rd[i].filename, filename) == 0){
                memset(rd[i].filename, '\0', FS_FILENAME_LEN);
                found = 1;
                current_index = rd[i].first_data_block_index;
                break;
            }
        }
    }
    
    if (found == -1){
        return -1;
    }
    
    // free FAT contents
   while (current_index != FAT_EOC){
       uint16_t temp_index = fat_table[current_index];
       fat_table[current_index] = 0;
       current_index = temp_index;

   }
    return 0;
}

/**
 * fs_ls - List files on file system
 *
 * List information about the files located in the root directory.
 *
 * Return: -1 if no FS is currently mounted. 0 otherwise.
 */
int fs_ls(void)
{
    if (block_disk_count() == -1){
        
    }
    printf("FS Ls:\n");
    for (int i=0; i<FS_FILE_MAX_COUNT; i++){
        if (rd[i].filename[0] != '\0'){
            printf("file: %s, size: %d, data_blk: %d\n", rd[i].filename, rd[i].file_size, rd[i].first_data_block_index);
        }
    }
    return 0;
}

/**
 * fs_open - Open a file
 * @filename: File name
 *
 * Open file named @filename for reading and writing, and return the
 * corresponding file descriptor. The file descriptor is a non-negative integer
 * that is used subsequently to access the contents of the file. The file offset
 * of the file descriptor is set to 0 initially (beginning of the file). If the
 * same file is opened multiple files, fs_open() must return distinct file
 * descriptors. A maximum of %FS_OPEN_MAX_COUNT files can be open
 * simultaneously.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * there is no file named @filename to open, or if there are already
 * %FS_OPEN_MAX_COUNT files currently open. Otherwise, return the file
 * descriptor.
 */
int fs_open(const char *filename)
{
    if (filename == NULL || strlen(filename) > FS_FILENAME_LEN){
        return -1;
    }

    int found = -1;
    for (int i=0; i<FS_FILE_MAX_COUNT; i++){
       if (strcmp((char*)rd[i].filename, filename) == 0){
           found = 1;
           open_files++;
           strcpy(file_d[i].filename, filename);
           file_d[i].offset = 0;
           file_d[i].fd_return = i;
           return file_d[i].fd_return;
       }
    }

    if (found == -1){
        return -1;
    }
       
    return 0;
}

/**
 * fs_close - Close a file
 * @fd: File descriptor
 *
 * Close file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open). 0 otherwise.
 */
int fs_close(int fd)
{
    if (fd > FS_OPEN_MAX_COUNT || fd < 0 || file_d[fd].fd_return == -1){
        return -1;
    }
    strcpy(file_d[fd].filename, "");
    file_d[fd].offset = 0;
    file_d[fd].fd_return = -1;
    open_files--;
    return 0;
}

/**
 * fs_stat - Get file status
 * @fd: File descriptor
 *
 * Get the current size of the file pointed by file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, of if file descriptor @fd is
 * invalid (out of bounds or not currently open). Otherwise return the current
 * size of file.
 */
int fs_stat(int fd)
{
    if(fd > FS_OPEN_MAX_COUNT || fd < 0 || file_d[fd].fd_return == -1){
        return -1;
    }

    char *file = file_d[fd].filename;
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if(strcmp(rd[i].filename, file) == 0){
            return rd[i].file_size;
        }
    }

    return 0;
}

/**
 * fs_lseek - Set file offset
 * @fd: File descriptor
 * @offset: File offset
 *
 * Set the file offset (used for read and write operations) associated with file
 * descriptor @fd to the argument @offset. To append to a file, one can call
 * fs_lseek(fd, fs_stat(fd));
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (i.e., out of bounds, or not currently open), or if @offset is larger
 * than the current file size. 0 otherwise.
 */
int fs_lseek(int fd, size_t offset)
{
    if (fd > FS_OPEN_MAX_COUNT || fd < 0 || file_d[fd].fd_return == -1 || offset > rd[fd].file_size){
        return -1;
    }
    file_d[fd].offset = offset;
    return 0;
}

// returns index of data block corresponding to file's offset
int data_block_index(size_t offset, uint16_t file_start){
    int index = file_start;
    while(index != FAT_EOC && BLOCK_SIZE < offset ){
        index = fat_table[index];
    }
    return index;
}


/**
 * fs_write - Write to a file
 * @fd: File descriptor
 * @buf: Data buffer to write in the file
 * @count: Number of bytes of data to be written
 *
 * Attempt to write @count bytes of data from buffer pointer by @buf into the
 * file referenced by file descriptor @fd. It is assumed that @buf holds at
 * least @count bytes.
 *
 * When the function attempts to write past the end of the file, the file is
 * automatically extended to hold the additional bytes. If the underlying disk
 * runs out of space while performing a write operation, fs_write() should write
 * as many bytes as possible. The number of written bytes can therefore be
 * smaller than @count (it can even be 0 if there is no more space on disk).
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually written.
 */
int fs_write(int fd, void *buf, size_t count)
{
    
    char *filename = (char*)file_d[fd].filename;
    int offset = file_d[fd].offset;
    // locate file
    int file_location = -1;
    uint16_t file_start = FAT_EOC;

    for (int i=0; i<FS_FILE_MAX_COUNT; i++){
        if (strcmp(rd[i].filename, filename) == 0){
            file_location = i;
            file_start = rd[i].first_data_block_index;
            break;
        }
    }

    if (file_location == -1){
        return -1;
    }

    struct root_dir *rdir = &rd[file_location];


    char *write_buf = (char*)buf;
    void *bounce_buffer = (void*)malloc(BLOCK_SIZE);
    uint16_t index = data_block_index(offset, file_start);
    int bytes_written =0;
    
    
    return bytes_written;
}

/**
 * fs_read - Read from a file
 * @fd: File descriptor
 * @buf: Data buffer to be filled with data
 * @count: Number of bytes of data to be read
 *
 * Attempt to read @count bytes of data from the file referenced by file
 * descriptor @fd into buffer pointer by @buf. It is assumed that @buf is large
 * enough to hold at least @count bytes.
 *
 * The number of bytes read can be smaller than @count if there are less than
 * @count bytes until the end of the file (it can even be 0 if the file offset
 * is at the end of the file). The file offset of the file descriptor is
 * implicitly incremented by the number of bytes that were actually read.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually read.
 */

int fs_read(int fd, void *buf, size_t count)
{
     //check if fd is invalid
    if(fd > FS_OPEN_MAX_COUNT || fd < 0 || file_d[fd].fd_return == -1){
            return -1;
    }

    //check if buf is null
    if(buf == NULL){
        return -1;
    }

    size_t offset = file_d[fd].offset;
    void * buffer_b = malloc(BLOCK_SIZE);
    int read_bytes = 0;

    while(count > 0){
        if (offset >= rd->file_size){
            break;
        } //at end of file

        //      size_t offset = file_d[fd].offset;
        uint16_t b_iter = rd->first_data_block_index; // block iterator
        int b_current = offset/BLOCK_SIZE; //current block
        int byte_location = offset % BLOCK_SIZE;
        int new_shift = BLOCK_SIZE;
        void* b_bounce = buffer_b + byte_location;

        for (int i = 0; i < b_current; i++){
            b_iter = fat_table[b_iter];
        } //obtain current block

        for(int i = 0; i < ((count/BLOCK_SIZE) + 1); i++) {
            block_read(b_iter + sb.data_block_start_index, buffer_b);

            if(byte_location > 0){
               new_shift = BLOCK_SIZE - byte_location;
            } else if(BLOCK_SIZE <= count+byte_location){
               b_bounce -= byte_location;
            } else {
               memcpy(buf+read_bytes, b_bounce, count);
               read_bytes += count;
               offset += count;
               break;
            }

            memcpy(buf+read_bytes, b_bounce, new_shift);
            read_bytes += new_shift;
            offset += new_shift;
            count -= new_shift;
        }
    }
    file_d[fd].offset += read_bytes;
    return read_bytes;
}

