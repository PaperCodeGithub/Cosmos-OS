# COSMOS (Custom x86 Operating System)

Cosmos is a custom, 32-bit x86 operating system built entirely from scratch in C and x86 Assembly. 

Currently in its foundational stage, this project bypasses standard libraries and system calls to interact directly with motherboard hardware, manage memory in Protected Mode, and handle asynchronous hardware interrupts.

## Features (Part 1)
* **Custom Boot Sequence:** Boots from 16-bit Real Mode into 32-bit Protected Mode using a custom Global Descriptor Table (GDT).
* **VGA Text Mode Driver:** Direct manipulation of the `0xB8000` memory buffer for colored terminal text and hardware cursor control.
* **Bare-Metal `libc`:** Custom implementations of `printf`, `scanf`, `itoa`, and `atoi` built entirely without `<stdio.h>` or `<string.h>`.
* **Hardware Interrupts (IDT & PIC):** Custom Interrupt Descriptor Table and remapped 8259 PIC chips to handle asynchronous hardware events without CPU polling.
* **Asynchronous Keyboard Driver:** Translates raw scancodes, handles Make/Break states for modifiers (Shift), and maintains a background input buffer.
* **System Clock (PIT):** Configured the Programmable Interval Timer to fire at 1000Hz (IRQ 0), enabling hardware-accurate `sleep()` functions and uptime tracking.

## Build & Run
To compile and run NBOS in QEMU, you need an x86 cross-compiler (`gcc`) and `nasm`.
```bash
make clean
make build
make run
