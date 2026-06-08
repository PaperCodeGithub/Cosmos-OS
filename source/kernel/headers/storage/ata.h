#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include "../ports/ports_io.h"

/*

    This currently use PIO we will convert it to DMA later

*/


// --- ATA Status Bits ---
#define ATA_SR_BSY  0x80    // Busy
#define ATA_SR_DRQ  0x08    // Data Request (Ready to transfer)
#define ATA_SR_ERR  0x01    // Error

// --- ATA Commands ---
#define ATA_CMD_READ_PIO  0x20

// ---------------------------------------------------------
// Wait for the drive to stop being busy
static void ata_wait_bsy() {
    while (port_byte_in(ATA_STATUS_PORT) & ATA_SR_BSY);
}

// Wait for the drive to say "I have data ready for you"
static void ata_wait_drq() {
    while (!(port_byte_in(ATA_STATUS_PORT) & ATA_SR_DRQ));
}
// ---------------------------------------------------------

// Modify your ata_read_sector function to return an integer (1 for success, 0 for fail)
int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    
    // SAFETY CHECK: Does the drive actually exist?
    // If the status port returns 0xFF, the bus is floating (no drive plugged in)
    uint8_t status = port_byte_in(ATA_STATUS_PORT);
    if (status == 0xFF) {
        return 0; // Fail early so we don't freeze!
    }

    // 1. Wait until the drive is not busy
    ata_wait_bsy();

    // 2. Select the Master Drive
    port_byte_out(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));

    // --- THE 400ns DELAY ---
    // The ATA spec demands we wait 400 nanoseconds after selecting a drive
    // Reading the status port takes about 100ns, so we read it 4 times and throw the data away.
    port_byte_in(ATA_STATUS_PORT);
    port_byte_in(ATA_STATUS_PORT);
    port_byte_in(ATA_STATUS_PORT);
    port_byte_in(ATA_STATUS_PORT);

    // 3. Send the Sector Count
    port_byte_out(ATA_SECTOR_COUNT_PORT, 1);

    // 4. Send the rest of the LBA address
    port_byte_out(ATA_LBA_LOW_PORT, (uint8_t)(lba & 0xFF));
    port_byte_out(ATA_LBA_MID_PORT, (uint8_t)((lba >> 8) & 0xFF));
    port_byte_out(ATA_LBA_HIGH_PORT, (uint8_t)((lba >> 16) & 0xFF));

    // 5. Send the "READ" command
    port_byte_out(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);

    // 6. Wait for the drive
    ata_wait_bsy();
    ata_wait_drq();

    // 7. Read the Data
    for (int i = 0; i < 256; i++) {
        uint16_t word = port_word_in(ATA_DATA_PORT);
        buffer[i * 2] = (uint8_t)(word & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)((word >> 8) & 0xFF);
    }
    
    return 1; // Success!
}

#endif