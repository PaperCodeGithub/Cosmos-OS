[org 0x7C00]
[bits 16]

%define ENDL 0x0D, 0x0A 

jmp short start
nop

; --- FAT12 BPB ---
oem_name:               db "NBOS    "   
bytes_per_sector:       dw 512
sectors_per_cluster:    db 1
reserved_sectors:       dw 1
fat_count:              db 2
dir_entries:            dw 224
total_sectors:          dw 2880
media_descriptor:       db 0xF0
sectors_per_fat:        dw 9
sectors_per_track:      dw 18
heads_per_cylinder:     dw 2
hidden_sectors:         dd 0
large_sectors:          dd 0

drive_number:           db 0x00         
reserved:               db 0
ext_boot_signature:     db 0x29
volume_serial:          dd 0x12345678   
volume_label:           db "NBOS       "
file_system_type:       db "FAT12   "   

KERNEL_OFFSET equ 0x1000

; ==========================================================
; 16-BIT REAL MODE ENVIRONMENT
; ==========================================================
start:
    jmp main 

; --- String Print Routine ---
puts:
    push si 
    push ax 
    push bx 
.loop:
    lodsb      
    or al, al  
    jz .done
    mov ah, 0x0e 
    mov bh, 0    
    int 0x10     
    jmp .loop    
.done:
    pop bx       
    pop ax
    pop si
    ret 

; --- Global Error Handlers ---
floppy_error:
    mov si, error
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0FFFFh:0

; --- LBA to CHS Math ---
lba_to_chs:
    push ax
    push dx
    xor dx, dx
    div word [sectors_per_track]
    inc dx
    mov cx, dx
    xor dx, dx
    div word [heads_per_cylinder]
    mov dh, dl
    mov ch, al
    shl ah, 6
    or cl, ah
    pop ax
    mov dl, al
    pop ax
    ret

; --- Advanced Disk Reader ---
disk_read:
    push ax
    push bx
    push cx
    push dx
    push di

    push cx
    call lba_to_chs
    pop ax
    
    mov ah, 02h
    mov di, 3

.retry:
    pusha
    stc
    int 13h
    jnc .done_read

    popa
    call disk_reset
    dec di
    test di, di
    jnz .retry

.fail:
    jmp floppy_error

.done_read:
    popa
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; --- Disk Reset ---
disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret

; --- Main Bootloader ---
main:
    mov [BOOT_DRIVE], dl 

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00 

    mov si, msg
    call puts
    
    ; Load Kernel 
    mov ax, 33                  ; LBA 33 
    mov cl, 30                   ; Read 30 sector
    mov dl, [BOOT_DRIVE]        ; Disk ID
    mov bx, KERNEL_OFFSET       ; Buffer 0x1000
    call disk_read
    
    ; SWITCH TO 32-BIT MODE
    cli                     
    lgdt [gdt_descriptor]   
    
    mov eax, cr0            
    or eax, 0x1             
    mov cr0, eax            

    jmp CODE_SEG:init_pm    

BOOT_DRIVE db 0             

; =========================================================
; GLOBAL DESCRIPTOR TABLE (GDT)
; =========================================================
gdt_start:
    dq 0x0                  

gdt_code:                   
    dw 0xffff               
    dw 0x0                  
    db 0x0                  
    db 10011010b            
    db 11001111b            
    db 0x0                  

gdt_data:                   
    dw 0xffff               
    dw 0x0                  
    db 0x0                  
    db 10010010b            
    db 11001111b            
    db 0x0                  
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  
    dd gdt_start                

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; =========================================================
; 32-BIT PROTECTED MODE ENVIRONMENT
; =========================================================
[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    jmp KERNEL_OFFSET      ; Jump directly to the loaded C kernel!
    jmp $

; --- Data ---
msg: db 'Loading Kernel...', ENDL, 0 
error: db 'Disk Read Error!', ENDL, 0
times 510 - ($-$$) db 0 
dw 0AA55h   