[bits 16]
; We don't need an [org] because the bootloader jumps directly here (0x1000)

kernel_start:
    mov ah, 0x0e            ; BIOS teletype function
    mov al, 'K'             ; The letter 'K' for Kernel
    int 0x10                ; Print it
    
    mov al, ' '             ; Print a space just to be clean
    int 0x10

    jmp $                   ; Hang here safely