#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "string.h"

#define NULL ((void*)0)
typedef unsigned int size_t;

// ============================================================================
// PART 1: PHYSICAL MEMORY MANAGER (PMM)
// ============================================================================

// --- PMM Configuration ---
#define BLOCK_SIZE 4096                                // 4 KB blocks
#define MAX_BLOCKS ((32 * 1024 * 1024) / BLOCK_SIZE)   // For 32MB RAM: 8192 blocks

// The Bitmap: 8192 bits / 8 bits per byte = 1024 bytes long
uint32_t memory_bitmap[MAX_BLOCKS / 32]; 

// Tracks how many blocks are currently in use
uint32_t used_blocks = 0;
uint32_t max_blocks  = MAX_BLOCKS;

// --- Bitwise Math Helpers ---
// Sets a bit to 1 (Allocated)
static inline void bitmap_set(int bit) {
    memory_bitmap[bit / 32] |= (1U << (bit % 32));
}

// Sets a bit to 0 (Free)
static inline void bitmap_unset(int bit) {
    memory_bitmap[bit / 32] &= ~(1U << (bit % 32));
}

// Checks if a bit is 1 or 0
static inline int bitmap_test(int bit) {
    return memory_bitmap[bit / 32] & (1U << (bit % 32));
}

// --- Core PMM Functions ---

// Initialize the PMM (Set everything to 0)
void pmm_init() {
    for (uint32_t i = 0; i < (max_blocks / 32); i++) {
        memory_bitmap[i] = 0;
    }
    used_blocks = 0;
}

// Scans the bitmap to find the first 0 (Free Block)
int pmm_find_first_free() {
    for (uint32_t i = 0; i < (max_blocks / 32); i++) {
        if (memory_bitmap[i] != 0xFFFFFFFF) {            // If this 32-bit chunk isn't totally full
            for (int j = 0; j < 32; j++) {               // Test each bit
                uint32_t bit = 1U << j;                  // (Fixed unsigned shift)
                if (!(memory_bitmap[i] & bit)) {
                    return (i * 32) + j;                 // Return the absolute block index
                }
            }
        }
    }
    return -1; // Out of memory!
}

// Asks the OS for a free 4KB block of RAM
void* pmm_alloc_block() {
    if (used_blocks >= max_blocks) return 0; // OOM

    int frame = pmm_find_first_free();
    if (frame == -1) return 0; // OOM

    bitmap_set(frame);
    used_blocks++;

    // Multiply the block index by 4096 to get the actual physical memory address
    uint32_t address = frame * BLOCK_SIZE;
    return (void*)address;
}

// Gives a 4KB block back to the OS
void pmm_free_block(void* address) {
    uint32_t phys_addr = (uint32_t)address;
    int frame = phys_addr / BLOCK_SIZE;

    bitmap_unset(frame);
    used_blocks--;
}

// Forcefully allocates a specific region of memory so the PMM doesn't hand it out
void pmm_reserve_region(uint32_t start_addr, uint32_t size) {
    uint32_t align_start = start_addr / BLOCK_SIZE;
    uint32_t align_size  = size / BLOCK_SIZE;
    
    // If the size isn't a perfect multiple of 4KB, reserve one extra block to be safe
    if (size % BLOCK_SIZE != 0) align_size++;

    for (uint32_t i = 0; i < align_size; i++) {
        bitmap_set(align_start + i);
        used_blocks++;
    }
}


// ============================================================================
// PART 2: DYNAMIC MEMORY ALLOCATOR (HEAP)
// ============================================================================

// --- Heap Configuration ---
#define HEAP_START 0x00200000 // Place heap right after 2MB reserved kernel space
#define HEAP_SIZE  0x00200000 // 2 MB Heap Size

// The Hidden Metadata Header
typedef struct memory_block {
    size_t size;                 // Size of the usable memory
    int free;                    // 1 if free, 0 if in use
    struct memory_block *next;   // Pointer to the next block in the list
} memory_block_t;

memory_block_t *free_list = (memory_block_t*)HEAP_START;

// --- Core Heap Functions ---

// Initializes the first giant free block
void init_heap() {
    free_list->size = HEAP_SIZE - sizeof(memory_block_t);
    free_list->free = 1;
    free_list->next = NULL;
}

// MALLOC: Find a free block and split it if necessary
void* malloc(size_t size) {
    memory_block_t *current = free_list;

    while (current != NULL) {
        // Did we find a free block that is big enough?
        if (current->free && current->size >= size) {
            
            // Is the block big enough to split into two?
            // (Needs enough room for requested size + new header + 1 byte data)
            if (current->size > size + sizeof(memory_block_t)) {
                
                // Pointer math to create a new header right after requested memory
                memory_block_t *new_block = (memory_block_t*)((char*)current + sizeof(memory_block_t) + size);
                
                new_block->free = 1;
                new_block->size = current->size - size - sizeof(memory_block_t);
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->free = 0; // Mark as used
            
            // Return pointer to actual data area (Right after header)
            return (void*)(current + 1); 
        }
        current = current->next;
    }
    
    return NULL; // Out of Heap Memory!
}

// FREE: Mark as free and merge adjacent free blocks
void free(void* ptr) {
    if (ptr == NULL) return;

    // Pointer math to find hidden header just behind data
    memory_block_t *block = (memory_block_t*)ptr - 1;
    block->free = 1;

    // Coalescing: Merge with the next block if it is also free
    memory_block_t *current = free_list;
    while (current != NULL) {
        if (current->free && current->next != NULL && current->next->free) {
            current->size += current->next->size + sizeof(memory_block_t);
            current->next = current->next->next;
        }
        current = current->next;
    }
}

// CALLOC: Allocate and fill with zeros
void* calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = malloc(total_size);
    
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

// REALLOC: Resize an existing block
void* realloc(void* ptr, size_t new_size) {
    if (ptr == NULL) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    // Check how big the current block is
    memory_block_t *block = (memory_block_t*)ptr - 1;
    if (block->size >= new_size) {
        return ptr; // It's already big enough!
    }

    // Too small. Allocate a new block, copy the data, and free the old one.
    void* new_ptr = malloc(new_size);
    if (new_ptr != NULL) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    
    return new_ptr;
}

#endif