#include "headers/cio.h"
#include "headers/string.h"
#include "headers/system.h"
#include "headers/clock.h"
#include "headers/pmm.h"
#include "headers/paging.h"
#include "headers/fat32.h"

void init_system();
void showSysInfo();

void kernel_main(int argc, char* argv[]) {

    init_system();

    char cmd_buffer[256];
    char *args[10]; 
    int arg_count;

    char current_path[256] = "/";

    while (1) {
        printf("root@cosmos%s >> ", current_path);
        gets(cmd_buffer);

        arg_count = tokenize(cmd_buffer, args);

        if (arg_count == 0) continue;

        char *cmd = args[0];

        if(!strcmp(cmd, "sys-info")){
            showSysInfo();
        }else if(!strcmp(cmd, "ls")){
            fat32_list_root();
        }else if (strcmp(cmd, "cd") == 0) {
            if (arg_count < 2) {
                printf("Usage: cd <directory>\n");
            } else {
                if (fat32_cd(args[1])) {
                    if (strcmp(args[1], "..") == 0) {
                        int len = strlen(current_path);
                        if (len > 1) { 
                            for (int i = len - 1; i >= 0; i--) {
                                if (current_path[i] == '/') {
                                    if (i == 0) {
                                        current_path[1] = '\0'; 
                                    } else {
                                        current_path[i] = '\0';
                                    }
                                    break;
                                }
                            }
                        }
                    } else {
                        if (strlen(current_path) + strlen(args[1]) + 2 < 256) {
                            if (strcmp(current_path, "/") != 0) {
                                strcat(current_path, "/");
                            }
                            
                            for (int i = 0; args[1][i] != '\0'; i++) {
                                if (args[1][i] >= 'a' && args[1][i] <= 'z') args[1][i] -= 32;
                            }
                            
                            strcat(current_path, args[1]);
                        }
                    }
                } else {
                    printf("Directory not found.\n");
                }
            }
        }else if (strcmp(cmd, "cat") == 0) {
            if (arg_count < 2) {
                printf("Usage: cat <filename.ext>\n");
            } else {
                FILE *f = fopen(args[1], "r");
                
                if (f != NULL) {
                    char *file_buf = (char*)malloc(f->file_size + 1);
                    fread(file_buf, 1, f->file_size, f);
                    file_buf[f->file_size] = '\0';
                    printf("%s\n", file_buf);
                    
                    free(file_buf);
                    fclose(f);
                } else {
                    printf("File not found: %s\n", args[1]);
                }
            }
        }else {
            printf("Not an recognizable command or oparetion\n");
        }
    }
}

void init_system(){
    set_terminal_color(VGA_COLOR_CYAN, VGA_COLOR_BLUE);
    clear_screen();
    
    idt_install();
    pic_remap();

    port_byte_out(PIC1_DATA, 0xFF);
    port_byte_out(PIC2_DATA, 0xFF);
    
    idt_set_gate(32, (uint32_t)timer_isr, 0x08, 0x8E);      // filling 32 number idt to timer_isr function
    idt_set_gate(33, (uint32_t)keyboard_isr, 0x08, 0x8E);   // filling 33 number to keyboard_isr
    idt_set_gate(14, (uint32_t)page_fault_handler, 0x08, 0x8E);   // pagefault handler interrupt
    init_timer(1000);

    irq_clear_mask(0);
    irq_clear_mask(1);  
    
    __asm__ volatile("sti");

    pmm_init();
    pmm_reserve_region(0x00000000, 0x200000);
    
    init_heap();
    pmm_reserve_region(HEAP_START, HEAP_SIZE);

    init_paging();
    fat32_init();

    clear_screen();

    enable_cursor(14, 15);
}

void showSysInfo(){
    printf("System: Cosmos\nArchitecture : 32-bit x86 Protected Mode\nCPU Vendor   : %s\nUptime       : %d milliseconds\n", get_cpu_vendor(), timer_ticks);
}
