ASM=nasm
QEMU= qemu-system-i386 
SRC_DIR=source
BUILD_DIR=build

CC = gcc
LD = ld
CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -O2 -Wall -Wextra
LDFLAGS = -m elf_i386 -Ttext 0x1000 --oformat binary

.PHONY: all floppy_image kernel bootloader clean always run

all: floppy_image hdd_image

#
# Floppy Image (Bootloader and Kernel ONLY)
#
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader kernel
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12 -n "COSMOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"


#
# Hard Drive Image (FAT32 and User Files ONLY)
#
hdd_image: always
	dd if=/dev/zero of=$(BUILD_DIR)/hdd.img bs=1M count=64
	mkfs.fat -F 32 -n "COSMOS_DRV" $(BUILD_DIR)/hdd.img
	mmd -i $(BUILD_DIR)/hdd.img ::DOCS
	mcopy -i $(BUILD_DIR)/hdd.img welcome.txt "::DOCS/welcome.txt"


#	
# Bootloader
#
bootloader: $(BUILD_DIR)/bootloader.bin

$(BUILD_DIR)/bootloader.bin: always
	$(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin

#
# Kernel
#
#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

# Compile C code
$(BUILD_DIR)/kernel.o: always
	$(CC) $(CFLAGS) -c $(SRC_DIR)/kernel/kernel.c -o $(BUILD_DIR)/kernel.o

# Assemble the Entry Stub (Note we use -f elf32 here to match the C object file)
$(BUILD_DIR)/kernel_entry.o: always
	$(ASM) $(SRC_DIR)/kernel/kernel_entry.asm -f elf32 -o $(BUILD_DIR)/kernel_entry.o

# Link them together! kernel_entry.o MUST be listed first!
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o
	$(LD) $(LDFLAGS) $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o -o $(BUILD_DIR)/kernel.bin

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	rm -rf $(BUILD_DIR)/*

#
# Run
#
run: floppy_image hdd_image
	$(QEMU) -boot a -fda $(BUILD_DIR)/main_floppy.img -drive format=raw,file=$(BUILD_DIR)/hdd.img,index=0,media=disk
	