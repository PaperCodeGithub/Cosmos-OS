#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include "storage/ata.h"
#include "cio.h"
#include "pmm.h"


// The BIOS Parameter Block for FAT32
typedef struct {
    uint8_t  boot_jmp[3];
    uint8_t  oem_name[8];
    uint16_t  bytes_per_sector;     // Usually 512
    uint8_t  sectors_per_cluster;  // Power of 2 (1, 2, 4, 8, etc.)
    uint16_t reserved_sector_count; // Sectors before the first FAT
    uint8_t  num_fats;             // Usually 2 (for redundancy)
    uint16_t root_entry_count;     // 0 for FAT32
    uint16_t total_sectors_16;     // 0 for FAT32
    uint8_t  media_type;
    uint16_t table_size_16;        // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t head_side_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 Extended Fields
    uint32_t table_size_32;        // Size of one FAT in sectors
    uint16_t extended_flags;
    uint16_t fat_version;
    uint32_t root_cluster;         // Usually 2
    uint16_t fat_info;
    uint16_t backup_BS_sector;
    uint8_t  reserved_0[12];
    uint8_t  drive_number;
    uint8_t  reserved_1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fat_type_label[8];    // Should say "FAT32   "
} __attribute__((packed)) fat32_bpb_t;

// Structure for a Directory Entry (32 bytes)
typedef struct {
    uint8_t  name[11];             // 8 chars name, 3 chars extension
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_acc_date;
    uint16_t first_cluster_high;   // High 16 bits of first cluster
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;    // Low 16 bits of first cluster
    uint32_t file_size;
} __attribute__((packed)) directory_entry_t;

// The 32-byte Long File Name (LFN) Entry
typedef struct {
    uint8_t  sequence_number; // Tells us the order of the fragments
    uint16_t name1[5];        // First 5 characters (UTF-16)
    uint8_t  attributes;      // ALWAYS 0x0F
    uint8_t  type;            // ALWAYS 0x00
    uint8_t  checksum;        // Matches the Short File Name
    uint16_t name2[6];        // Next 6 characters (UTF-16)
    uint16_t first_cluster;   // ALWAYS 0x0000
    uint16_t name3[2];        // Last 2 characters (UTF-16)
} __attribute__((packed)) lfn_entry_t;

// --- Global State for the File System ---
static uint32_t data_start_lba;
static uint32_t root_cluster;
static uint8_t  sectors_per_cluster;

static uint32_t current_dir_cluster;


// Initialize the File System and calculate the offsets
void fat32_init() {
    uint8_t *boot_sector = (uint8_t*)malloc(512);
    
    if (!ata_read_sector(0, boot_sector)) {
        printf("FAT32 Init Failed: Disk not found.\n");
        free(boot_sector);
        return;
    }

    fat32_bpb_t *bpb = (fat32_bpb_t*)boot_sector;

    // The OS Math: Where does the File Allocation Table start?
    uint32_t fat_start_lba = bpb->reserved_sector_count;
    
    // Where does the actual Data start? (Skip the reserved sectors and the FAT tables)
    uint32_t fat_size = bpb->table_size_32; 
    data_start_lba = fat_start_lba + (bpb->num_fats * fat_size);
    
    // Save the important variables for later
    root_cluster = bpb->root_cluster; // Usually Cluster 2
    sectors_per_cluster = bpb->sectors_per_cluster;

    root_cluster = bpb->root_cluster; 
    sectors_per_cluster = bpb->sectors_per_cluster;

    current_dir_cluster = root_cluster;

    free(boot_sector);
}

// Convert a Cluster Number into a raw hard drive LBA
uint32_t cluster_to_lba(uint32_t cluster) {
    // FAT32 Data Clusters start counting at 2 (Cluster 0 and 1 are reserved)
    return data_start_lba + ((cluster - 2) * sectors_per_cluster);
}

// List all files and folders in the Root Directory
void fat32_list_root() {
    // 1. Calculate exactly which hard drive sector holds the Root Directory
    uint32_t dir_lba = cluster_to_lba(current_dir_cluster);
    
    // 2. Ask the ATA Driver to grab that sector
    uint8_t *dir_buffer = (uint8_t*)malloc(512); 
    ata_read_sector(dir_lba, dir_buffer);
    
    // 3. Drop the 32-Byte Stencil over the raw data!
    directory_entry_t *dir = (directory_entry_t*)dir_buffer;
    
    printf("\n--- ROOT DIRECTORY ---\n");
    
    // A 512-byte sector holds exactly 16 of our 32-byte structs
    for (int i = 0; i < 16; i++) {
        
        // Check the first byte of the name to see if the entry is valid
        if (dir[i].name[0] == 0x00) break;       // 0x00 means "End of Directory"
        if (dir[i].name[0] == 0xE5) continue;    // 0xE5 means "Deleted File" (skip it)
        if (dir[i].attributes == 0x0F) continue; // 0x0F means "Long File Name" (skip for now)
        if (dir[i].attributes & 0x08) continue;  // 0x08 means "Volume Label"
        
        // Print the 8-character File Name (Ignoring trailing spaces)
        for(int j = 0; j < 8; j++) {
            if(dir[i].name[j] != ' ') printf("%c", dir[i].name[j]);
        }
        
        // Is it a Directory or a File?
        if (dir[i].attributes & 0x10) {
            // It's a folder!
            printf(" <DIR>\n");
        } else {
            // It's a file! Print the 3-character Extension
            printf(".");
            for(int j = 8; j < 11; j++) {
                if(dir[i].name[j] != ' ') printf("%c", dir[i].name[j]);
            }
            // Print the file size
            printf(" (%d bytes)\n", dir[i].file_size);
        }
    }
    printf("----------------------\n\n");
    
    free(dir_buffer);
}

// Change the current directory
int fat32_cd(char *folder_name) {
    // 1. Convert user input ("mydir") into FAT32 format ("MYDIR      ")
    char fat_name[11];
    for(int i = 0; i < 11; i++) fat_name[i] = ' '; // Fill with spaces
    
    for(int i = 0; i < 8 && folder_name[i] != '\0'; i++) {
        // Convert lowercase to uppercase
        if (folder_name[i] >= 'a' && folder_name[i] <= 'z') {
            fat_name[i] = folder_name[i] - 32;
        } else {
            fat_name[i] = folder_name[i];
        }
    }

    // 2. Read the current directory from the disk
    uint32_t dir_lba = cluster_to_lba(current_dir_cluster);
    uint8_t *dir_buffer = (uint8_t*)malloc(512);
    ata_read_sector(dir_lba, dir_buffer);
    directory_entry_t *dir = (directory_entry_t*)dir_buffer;

    // 3. Search for the folder
    for (int i = 0; i < 16; i++) {
        if (dir[i].name[0] == 0x00) break; // End of list
        if (dir[i].name[0] == 0xE5) continue; // Deleted file
        if (!(dir[i].attributes & 0x10)) continue; // MUST be a directory (0x10)!

        // Compare the 11-character name
        int match = 1;
        for(int j = 0; j < 11; j++) {
            if (dir[i].name[j] != fat_name[j]) {
                match = 0; break;
            }
        }

        // 4. We found it!
        if (match) {
            // FAT32 stores the cluster number in two halves (High and Low 16-bits).
            // We must shift the High bits to the left and merge them with the Low bits.
            uint32_t next_cluster = ((uint32_t)dir[i].first_cluster_high << 16) | dir[i].first_cluster_low;

            // FAT32 Quirk: If you cd into "..", and the parent is the Root Directory, 
            // the cluster number is technically 0. We must manually fix this to 2.
            if (next_cluster == 0) next_cluster = root_cluster;

            current_dir_cluster = next_cluster; // Update our global location!
            free(dir_buffer);
            return 1; // Success
        }
    }

    free(dir_buffer);
    return 0; // Folder not found
}

// The standard File Handle structure
typedef struct {
    uint32_t first_cluster;   // Where the file starts on disk
    uint32_t current_cluster; // Which cluster we are currently reading
    uint32_t file_size;       // Total size of the file in bytes
    uint32_t position;        // How many bytes we have read so far (Offset)
    uint8_t  mode;            // 'r' for read, 'w' for write
} FILE;

#define NULL ((void*)0)

FILE* fopen(char* filename, char* mode) {
    // For now, we only support Read Mode ('r')
    if (mode[0] != 'r') {
        printf("Error: Only read mode ('r') is supported right now.\n");
        return NULL;
    }

    // 1. Convert "TEST.TXT" into FAT32 format: "TEST    TXT"
    char fat_name[11];
    for(int i = 0; i < 11; i++) fat_name[i] = ' '; // Fill with spaces
    
    int i = 0, j = 0;
    // Parse the Name (before the dot)
    while (filename[i] != '.' && filename[i] != '\0' && j < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
        fat_name[j++] = c;
    }
    
    // Skip the dot if it exists
    if (filename[i] == '.') i++;
    
    // Parse the Extension (after the dot)
    j = 8;
    while (filename[i] != '\0' && j < 11) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
        fat_name[j++] = c;
    }

    // 2. Read the current directory from the disk
    uint32_t dir_lba = cluster_to_lba(current_dir_cluster);
    uint8_t *dir_buffer = (uint8_t*)malloc(512);
    ata_read_sector(dir_lba, dir_buffer);
    directory_entry_t *dir = (directory_entry_t*)dir_buffer;

    // 3. Search for the file
    for (int k = 0; k < 16; k++) {
        if (dir[k].name[0] == 0x00) break; // End of list
        if (dir[k].name[0] == 0xE5) continue; // Deleted file
        if (dir[k].attributes & 0x10) continue; // Skip Directories! (Must be a file)
        if (dir[k].attributes == 0x0F) continue; // Skip Long File Names

        // Compare the 11-character name
        int match = 1;
        for (int m = 0; m < 11; m++) {
            if (dir[k].name[m] != fat_name[m]) {
                match = 0; break;
            }
        }

        // 4. We found the file!
        if (match) {
            // Ask the heap for memory to track this file's state
            FILE *file = (FILE*)malloc(sizeof(FILE));
            
            // Populate the handle with data from the Directory Entry
            file->first_cluster = ((uint32_t)dir[k].first_cluster_high << 16) | dir[k].first_cluster_low;
            file->current_cluster = file->first_cluster;
            file->file_size = dir[k].file_size;
            file->position = 0;
            file->mode = 'r';
            
            free(dir_buffer);
            return file; // Return the memory address of the handle!
        }
    }

    // File not found
    free(dir_buffer);
    return NULL; 
}

int fclose(FILE *stream) {
    if (stream == NULL) {
        return -1; // Error: Trying to close a null pointer
    }
    
    // Free the memory we allocated in fopen
    free(stream);
    
    return 0; // Success
}

// Reads data from the disk into your memory buffer
uint32_t fread(void *buffer, uint32_t size, uint32_t count, FILE *file) {
    if (file == NULL || buffer == NULL) return 0;

    // 1. Calculate how many bytes the user actually wants
    uint32_t bytes_to_read = size * count;
    
    // Safety Net: Never read past the end of the file!
    uint32_t bytes_remaining = file->file_size - file->position;
    if (bytes_to_read > bytes_remaining) {
        bytes_to_read = bytes_remaining; 
    }
    if (bytes_to_read == 0) return 0; // We reached the End of File (EOF)

    // 2. Find the physical hard drive sectors for this cluster
    uint32_t cluster_lba = cluster_to_lba(file->current_cluster);
    uint32_t cluster_bytes = sectors_per_cluster * 512;

    // 3. Allocate a temporary buffer to hold the raw hardware data
    uint8_t *raw_cluster = (uint8_t*)malloc(cluster_bytes);

    // Tell the ATA driver to read every sector in this cluster
    for (int i = 0; i < sectors_per_cluster; i++) {
        ata_read_sector(cluster_lba + i, raw_cluster + (i * 512));
    }

    // 4. Copy the exact requested bytes into the user's buffer.
    // We use file->position so if they call fread twice, we pick up where we left off!
    uint8_t *dest = (uint8_t*)buffer;
    for (uint32_t i = 0; i < bytes_to_read; i++) {
        dest[i] = raw_cluster[file->position + i];
    }

    // 5. Update the Handle's state
    file->position += bytes_to_read;

    // Clean up our temporary raw buffer
    free(raw_cluster);
    
    // Return how many "items" we successfully read
    return bytes_to_read / size; 
}


#endif