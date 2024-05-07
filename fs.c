#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>

// Constants
#define MAX_FILE_NAME_LENGTH 15 // Maximum length of a file name
#define MAX_FILE_DESCRIPTOR_COUNT 32 // Maximum number of file descriptors
#define MAX_FILE_COUNT 64 // Maximum number of files
#define MAX_FILE_SIZE (1024 * 1024) // Maximum file size (1 MiB)

// Data structures
struct SuperBlock {
    int inode_table_offset; // Offset of the inode table on disk
    int data_blocks_offset; // Offset of the data blocks on disk
    int bitmap_offset; // Offset of the bitmap on disk
    int root_directory_offset; // Offset of the root directory on disk
};

struct Inode {
    int size; // Size of the file in bytes
    int data_block_offsets[256]; // Offsets of data blocks on disk
};

struct DirectoryEntry {
    char name[MAX_FILE_NAME_LENGTH + 1]; // Name of the file
    int inode_index; // Index of the corresponding inode in the inode table
};

// Global variables
struct SuperBlock super_block; // Super block
struct Inode inode_table[MAX_FILE_COUNT]; // Inode table
struct DirectoryEntry root_directory[MAX_FILE_COUNT]; // Root directory
int file_descriptor_table[MAX_FILE_DESCRIPTOR_COUNT]; // File descriptor table
int bitmap[256]; // Bitmap to track free blocks on disk

// Helper functions
int find_free_inode() { // Find a free inode in the inode table
    for (int i = 0; i < MAX_FILE_COUNT; i++) {
        if (inode_table[i].size == 0) {
            return i; // Return index of free inode
        }
    }
    return -1; // No free inode available
}

int find_free_file_descriptor() { // Find a free file descriptor
    for (int i = 0; i < MAX_FILE_DESCRIPTOR_COUNT; i++) {
        if (file_descriptor_table[i] == 0) {
            return i; // Return index of free file descriptor
        }
    }
    return -1; // No free file descriptor available
}

int find_file(const char *name) { // Find a file by name in the root directory
    for (int i = 0; i < MAX_FILE_COUNT; i++) {
        if (strcmp(root_directory[i].name, name) == 0) {
            return i; // Return index of the file in the root directory
        }
    }
    return -1; // File not found
}

// Management Routines

int make_fs(const char *disk_name) { // Create a file system on disk
    if (make_disk(disk_name) == -1) {
        return -1; // Error creating disk
    }

    if (open_disk(disk_name) == -1) {
        return -1; // Error opening disk
    }

    // Initialize super block, inode table, bitmap, and root directory
    super_block.inode_table_offset = 1;
    super_block.data_blocks_offset = MAX_FILE_COUNT + 1;
    super_block.bitmap_offset = super_block.data_blocks_offset + 256;
    super_block.root_directory_offset = super_block.bitmap_offset + 256;

    for (int i = 0; i < MAX_FILE_COUNT; i++) {
        inode_table[i].size = 0;
        memset(inode_table[i].data_block_offsets, -1, sizeof(inode_table[i].data_block_offsets));
        memset(root_directory[i].name, 0, sizeof(root_directory[i].name));
        root_directory[i].inode_index = -1;
    }

    memset(bitmap, 0, sizeof(bitmap));

    // Write super block, inode table, bitmap, and root directory to disk
    if (write_blocks(0, 1, &super_block) == -1) {
        return -1; // Error writing super block to disk
    }
    if (write_blocks(super_block.inode_table_offset, MAX_FILE_COUNT, inode_table) == -1) {
        return -1; // Error writing inode table to disk
    }
    if (write_blocks(super_block.bitmap_offset, 1, bitmap) == -1) {
        return -1; // Error writing bitmap to disk
    }
    if (write_blocks(super_block.root_directory_offset, MAX_FILE_COUNT, root_directory) == -1) {
        return -1; // Error writing root directory to disk
    }

    if (close_disk(disk_name) == -1) {
        return -1; // Error closing disk
    }

    return 0; // File system creation successful
}

int mount_fs(const char *disk_name) { // Mount an existing file system from disk
    if (open_disk(disk_name) == -1) {
        return -1; // Error opening disk
    }

    // Read super block, inode table, bitmap, and root directory from disk
    if (read_blocks(0, 1, &super_block) == -1) {
        return -1; // Error reading super block from disk
    }
    if (read_blocks(super_block.inode_table_offset, MAX_FILE_COUNT, inode_table) == -1) {
        return -1; // Error reading inode table from disk
    }
    if (read_blocks(super_block.bitmap_offset, 1, bitmap) == -1) {
        return -1; // Error reading bitmap from disk
    }
    if (read_blocks(super_block.root_directory_offset, MAX_FILE_COUNT, root_directory) == -1) {
        return -1; // Error reading root directory from disk
    }

    return 0; // File system mounting successful
}

int umount_fs(const char *disk_name) { // Unmount the file system and write changes to disk
    // Write super block, inode table, bitmap, and root directory to disk
    if (write_blocks(0, 1, &super_block) == -1) {
        return -1; // Error writing super block to disk
    }
    if (write_blocks(super_block.inode_table_offset, MAX_FILE_COUNT, inode_table) == -1) {
        return -1; // Error writing inode table to disk
    }
    if (write_blocks(super_block.bitmap_offset, 1, bitmap) == -1) {
        return -1; // Error writing bitmap to disk
    }
    if (write_blocks(super_block.root_directory_offset, MAX_FILE_COUNT, root_directory) == -1) {
        return -1; // Error writing root directory to disk
    }

    if (close_disk(disk_name) == -1) {
        return -1; // Error closing disk
    }

    return 0; // File system unmounting successful
}

// File System Functions

int fs_open(const char *name) { // Open a file
    int fd = find_free_file_descriptor();
    if (fd == -1) {
        return -1; // No free file descriptor
    }

    int index = find_file(name);
    if (index == -1) {
        return -1; // File not found
    }

    file_descriptor_table[fd] = index;
    return fd; // Return file descriptor
}

int fs_close(int fd) { // Close a file
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTOR_COUNT || file_descriptor_table[fd] == 0) {
        return -1; // Invalid file descriptor
    }

    file_descriptor_table[fd] = 0;
    return 0; // File closed successfully
}

int fs_create(const char *name) { // Create a new file
    int index = find_file(name);
    if (index != -1) {
        return -1; // File already exists
    }

    index = find_free_inode();
    if (index == -1) {
        return -1; // No free inode
    }

    if (strlen(name) > MAX_FILE_NAME_LENGTH) {
        return -1; // File name too long
    }

    strcpy(root_directory[index].name, name);
    root_directory[index].inode_index = index;
    inode_table[index].size = 0;

    return 0; // File created successfully
}

int fs_delete(const char *name) { // Delete a file
    int index = find_file(name);
    if (index == -1) {
        return -1; // File not found
    }

    // Check if the file is open
    for (int i = 0; i < MAX_FILE_DESCRIPTOR_COUNT; i++) {
        if (file_descriptor_table[i] == index) {
            return -1; // File is open
        }
    }

    // Free data blocks and inode
    for (int i = 0; i < 256; i++) {
        if (inode_table[index].data_block_offsets[i] != -1) {
            bitmap[inode_table[index].data_block_offsets[i]] = 0;
            inode_table[index].data_block_offsets[i] = -1;
        }
    }
    inode_table[index].size = 0;

    memset(root_directory[index].name, 0, sizeof(root_directory[index].name));
    root_directory[index].inode_index = -1;

    return 0; // File deleted successfully
}

int fs_read(int fd, void *buf, size_t nbyte) { // Read from a file
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTOR_COUNT || file_descriptor_table[fd] == 0) {
        return -1; // Invalid file descriptor
    }

    int index = file_descriptor_table[fd];
    int size = inode_table[index].size;
    int bytes_to_read = (size < nbyte) ? size : nbyte;

    int bytes_read = 0; // Total bytes read
    int remaining_bytes = bytes_to_read; // Remaining bytes to read
    int current_block = 0; // Current block index

    // Read data blocks from disk
    while (remaining_bytes > 0 && current_block < 256) {
        int block_offset = inode_table[index].data_block_offsets[current_block];
        if (block_offset == -1) {
            break; // No more blocks to read
        }
        
        int bytes_in_block = (remaining_bytes < 4096) ? remaining_bytes : 4096; // Bytes to read from current block
        char temp_buf[bytes_in_block]; // Temporary buffer to hold block data
        
        if (read_blocks(super_block.data_blocks_offset + block_offset, 1, temp_buf) == -1) {
            return -1; // Error reading block from disk
        }
        
        memcpy(buf + bytes_read, temp_buf, bytes_in_block); // Copy block data to output buffer
        bytes_read += bytes_in_block; // Update total bytes read
        remaining_bytes -= bytes_in_block; // Update remaining bytes
        current_block++; // Move to next block
    }

    return bytes_read; // Return total bytes read
}

int fs_write(int fd, const void *buf, size_t nbyte) { // Write to a file
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTOR_COUNT || file_descriptor_table[fd] == 0) {
        return -1; // Invalid file descriptor
    }

    int index = file_descriptor_table[fd];

    int bytes_written = 0; // Total bytes written
    int remaining_bytes = nbyte; // Remaining bytes to write
    int current_block = 0; // Current block index

    // Write data blocks to disk
    while (remaining_bytes > 0 && current_block < 256) {
        int block_offset = inode_table[index].data_block_offsets[current_block];
        if (block_offset == -1) {
            block_offset = find_free_block();
            if (block_offset == -1) {
                break; // No more free blocks
            }
            inode_table[index].data_block_offsets[current_block] = block_offset;
        }

        int bytes_in_block = (remaining_bytes < 4096) ? remaining_bytes : 4096; // Bytes to write to current block
        if (write_blocks(super_block.data_blocks_offset + block_offset, 1, (char *)buf + bytes_written) == -1) {
            return -1; // Error writing block to disk
        }

        bytes_written += bytes_in_block; // Update total bytes written
        remaining_bytes -= bytes_in_block; // Update remaining bytes
        current_block++; // Move to next block
    }

    // Update file size if necessary
    if (inode_table[index].size < bytes_written) {
        inode_table[index].size = bytes_written;
    }

    return bytes_written; // Return total bytes written
}

int fs_listfiles(char ***files) { // List all files in the file system
    char **file_list = malloc(MAX_FILE_COUNT * sizeof(char *)); // Allocate memory for file list
    if (file_list == NULL) {
        return -1; // Memory allocation failed
    }

    int file_count = 0; // Total number of files

    // Populate files array with file names
    for (int i = 0; i < MAX_FILE_COUNT; i++) {
        if (root_directory[i].inode_index != -1) {
            file_list[file_count] = malloc((strlen(root_directory[i].name) + 1) * sizeof(char)); // Allocate memory for file name
            if (file_list[file_count] == NULL) {
                // Free allocated memory before returning error
                for (int j = 0; j < file_count; j++) {
                    free(file_list[j]);
                }
                free(file_list);
                return -1; // Memory allocation failed
            }

            strcpy(file_list[file_count], root_directory[i].name); // Copy file name to list
            file_count++; // Increment file count
        }
    }

    *files = file_list; // Assign file list to output parameter

    return 0; // File list generated successfully
}

int fs_lseek(int fd, off_t offset) { // Set the file pointer to the specified offset
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTOR_COUNT || file_descriptor_table[fd] == 0) {
        return -1; // Invalid file descriptor
    }

    int index = file_descriptor_table[fd];
    if (offset < 0 || offset > inode_table[index].size) {
        return -1; // Invalid offset
    }

    // Set file pointer to the specified offset
    inode_table[index].file_pointer = offset;

    return 0; // File pointer set successfully
}


int fs_truncate(int fd, off_t length) { // Truncate a file to the specified length
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTOR_COUNT || file_descriptor_table[fd] == 0) {
        return -1; // Invalid file descriptor
    }

    int index = file_descriptor_table[fd];
    if (length < 0 || length > inode_table[index].size) {
        return -1; // Invalid length
    }

    // Truncate file
    int current_block = length / 4096; // Current block index
    int remaining_bytes = length % 4096; // Remaining bytes in last block

    // Free data blocks beyond the truncated size
    for (int i = current_block + 1; i < 256; i++) {
        if (inode_table[index].data_block_offsets[i] != -1) {
            bitmap[inode_table[index].data_block_offsets[i]] = 0;
            inode_table[index].data_block_offsets[i] = -1;
        }
    }

    // Update file size
    inode_table[index].size = length;

    return 0; // File truncated successfully
}
