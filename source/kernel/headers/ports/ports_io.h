#ifndef PORT_H
#define PORT_H

// PIC Ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

// --- ATA Ports ---
#define ATA_DATA_PORT         0x1F0
#define ATA_ERROR_PORT        0x1F1
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA_LOW_PORT      0x1F3
#define ATA_LBA_MID_PORT      0x1F4
#define ATA_LBA_HIGH_PORT     0x1F5
#define ATA_DRIVE_PORT        0x1F6
#define ATA_COMMAND_PORT      0x1F7
#define ATA_STATUS_PORT       0x1F7

// ==========================================================================
// 1. HARDWARE I/O PORTS
// ==========================================================================

// Send a single byte to a port
static inline void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ volatile("out %%al, %%dx" : : "a" (data), "d" (port));
}

// Send a 16-bit word to a port (Used for QEMU Shutdown)
static inline void port_word_out(unsigned short port, unsigned short data) {
    __asm__ volatile("out %%ax, %%dx" : : "a" (data), "d" (port));
}

// Reads a single byte from a specified CPU port
static inline unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ volatile("in %%dx, %%al" : "=a" (result) : "d" (port));
    return result;
}

// Reads a 16-bit word from a hardware port
static inline uint16_t port_word_in(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

// Forces the CPU to wait 1 microsecond (allows older hardware to catch up)
static inline void io_wait() {
    port_byte_out(0x80, 0); 
}

#endif