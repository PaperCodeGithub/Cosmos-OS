[bits 32]
[extern kernel_main]   ; Tell NASM that 'main' exists in our C file

; This will be placed exactly at 0x1000
global _start
_start:
    call kernel_main   ; Safely jump into the C code
    jmp $       ; Hang forever if the C code ever returns


[extern keyboard_handler_main]  ; Tell Assembly our C function exists
global keyboard_isr             ; Make this wrapper visible to our C code

keyboard_isr:
    pushad                      ; Safely save all CPU registers
    cld                         ; Clear direction flag (C compiler needs this)
    call keyboard_handler_main  ; Call our C function!
    popad                       ; Restore all registers
    iretd                       ; Special Interrupt Return!

; --- Timer Interrupt Wrapper ---
[extern timer_handler_main]  ; Tell Assembly our C function exists
global timer_isr             ; Make this wrapper visible to our C code

timer_isr:
    pushad                      ; Save CPU state
    cld
    call timer_handler_main     ; Call the C clock tick function
    popad                       ; Restore CPU state
    iretd                       ; Special Interrupt Return