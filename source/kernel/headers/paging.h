#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "pmm.h"

// --- Page Table/Directory Flags ---
#define PAGE_PRESENT 1  // 001 in binary (Bit 0)
#define PAGE_RW      2  // 010 in binary (Bit 1 - Read/Write)
#define PAGE_USER    4  // 100 in binary (Bit 2 - User mode accessible)

// The Master Page Directory (An array of 1024 32-bit entries)
uint32_t *page_directory;

// --- 1. The Mapping Function ---
// Takes a Physical Address and maps it to a fake Virtual Address
void map_page(uint32_t phys_addr, uint32_t virt_addr, uint32_t flags) {
    // Chop the virtual address to find the Directory Index (Top 10 bits)
    uint32_t pd_index = virt_addr >> 22;
    
    // Chop the virtual address to find the Table Index (Middle 10 bits)
    uint32_t pt_index = (virt_addr >> 12) & 0x03FF;

    // Check if a Page Table already exists for this Directory entry
    if ((page_directory[pd_index] & PAGE_PRESENT) == 0) {
        // It doesn't exist! Ask the PMM for a 4KB block of RAM for a new Page Table
        uint32_t *new_table = (uint32_t*)pmm_alloc_block();
        
        // Clear the new table so the CPU doesn't read garbage memory
        for(int i = 0; i < 1024; i++) {
            new_table[i] = 0;
        }

        // Add the new table to the Directory. 
        // We use bitwise OR (|) to combine the memory address with our flags!
        page_directory[pd_index] = ((uint32_t)new_table) | PAGE_PRESENT | PAGE_RW | flags;
    }

    // Get the exact memory address of the Page Table.
    // (We use & ~0xFFF to strip away the flags at the bottom 12 bits)
    uint32_t *page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);

    // Finally, map the physical frame into the Page Table!
    page_table[pt_index] = (phys_addr & ~0xFFF) | PAGE_PRESENT | PAGE_RW | flags;
}

// --- 2. The Hardware Switches ---
static inline void load_page_directory(uint32_t* pd) {
    // Load the physical address of the directory into Control Register 3 (CR3)
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd));
}

static inline void enable_paging() {
    uint32_t cr0;
    // Read the current state of CR0
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    
    // Flip the 31st bit to 1 (0x80000000 is 10000000... in binary)
    cr0 |= 0x80000000; 
    
    // Write it back to CR0 to turn the matrix on!
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

// --- 3. The Initialization Function ---
void init_paging() {
    // Ask the PMM for a 4KB block for the main Directory
    page_directory = (uint32_t*)pmm_alloc_block();
    
    // Clear it completely
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0; 
    }

    // THE LIFESAVER: Identity Map the first 8 Megabytes of RAM.
    // 8MB = 0x800000 bytes. We step by 4096 (4KB) for every block.
    for (uint32_t i = 0; i < 0x800000; i += 4096) {
        map_page(i, i, 0); // Map physical 'i' to virtual 'i'
    }

    // Lock and Load
    load_page_directory(page_directory);
    
    // Flip the switch
    enable_paging();
}

void page_fault_handler() {
    uint32_t faulting_address;
    
    // Ask the CPU: "Which memory address caused the crash?"
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    // Print the error (Assuming you have a printf or print_string function)
    printf("\n[SECURITY BREACH] PAGE FAULT!\n");
    printf("A program tried to illegally access: 0x%x\n", faulting_address);
    printf("System halted to protect the kernel.\n");
    
    // Freeze the computer permanently
    while(1) {
        __asm__ volatile("cli; hlt"); 
    }
}

#endif