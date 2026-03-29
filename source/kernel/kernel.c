#include "headers/cio.h"
#include "headers/string.h"

void init_system();

void kernel_main() {

    init_system();
    
    printf("System Uptime: %d milliseconds\n\n", timer_ticks);

    char *cmd;
    while (1) {
        printf("root@nbos> ");
        gets(cmd);
        printf("You typed: %s\n", cmd);
    }
}

void init_system(){
    set_terminal_color(VGA_COLOR_CYAN, VGA_COLOR_BLUE);
    clear_screen();
    
    idt_install(); 
    pic_remap();
    port_byte_out(PIC1_DATA, 0xFF);
    port_byte_out(PIC2_DATA, 0xFF);
    idt_set_gate(32, (uint32_t)timer_isr, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_isr, 0x08, 0x8E);
    init_timer(1000);
    irq_clear_mask(0);
    irq_clear_mask(1);  
    __asm__ volatile("sti");
    enable_cursor(14, 15);
}