#ifndef CIO_H
#define CIO_H

#include <stdarg.h>
#include <stdint.h>
#include "ports/ports_io.h"

// --- Global Keyboard State ---
#define KBD_BUFFER_SIZE 256
volatile char kbd_buffer[KBD_BUFFER_SIZE];
volatile int kbd_buffer_index = 0;
volatile int kbd_enter_pressed = 0; // Acts as a boolean flag

// VGA Display Dim
#define VGA_COLS 80
#define VGA_ROWS 25


// ==========================================================================
// 2. VGA DISPLAY DRIVER
// ==========================================================================


enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static unsigned int cursor_pos = 0;
unsigned char terminal_color = VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4);

// Sets the global terminal color for future text
void set_terminal_color(unsigned char foreground, unsigned char background) {
    terminal_color = foreground | (background << 4);
}

// Instantly changes the color of all text currently on the screen
void repaint_screen(unsigned char foreground, unsigned char background) {
    volatile char* video_memory = (volatile char*) 0xb8000;
    unsigned char new_color = foreground | (background << 4);
    for (int i = 0; i < VGA_COLS * VGA_ROWS * 2; i += 2) {
        video_memory[i + 1] = new_color;
    }
}

// Clears the screen and resets the cursor to the top-left
void clear_screen() {
    volatile char* video_memory = (volatile char*) 0xb8000;
    int screen_size = VGA_COLS * VGA_ROWS * 2;
    for(int i = 0; i < screen_size; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = terminal_color;
    }
    cursor_pos = 0;
}

// Enables the hardware cursor and sets its shape
// (Start=14, End=15 draws a classic underscore. Start=0, End=15 draws a solid block)
void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    port_byte_out(0x3D4, 0x0A);
    port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xC0) | cursor_start);
 
    port_byte_out(0x3D4, 0x0B);
    port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xE0) | cursor_end);
}

// Tells the VGA hardware to move the blinking cursor to our current software position
void update_cursor() {
    // Our cursor_pos counts color bytes too (0 to 3999). 
    // The hardware just wants the character index (0 to 1999), so we divide by 2.
    uint16_t pos = cursor_pos / 2;
 
    port_byte_out(0x3D4, 0x0F); // Tell VGA we are sending the Low byte
    port_byte_out(0x3D5, (uint8_t) (pos & 0xFF));
    
    port_byte_out(0x3D4, 0x0E); // Tell VGA we are sending the High byte
    port_byte_out(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

// Prints a single character, handling special escape sequences (\n, \b, \t)
void print_char(char c) {
    volatile char* video_memory = (volatile char*) 0xb8000;
    
    if (c == '\n') {
        cursor_pos = (cursor_pos / (VGA_COLS * 2) + 1) * (VGA_COLS * 2);
    } else if (c == '\b') {
        if (cursor_pos >= 2) { // Prevent underflow
            cursor_pos -= 2;
            video_memory[cursor_pos] = ' ';
            video_memory[cursor_pos + 1] = terminal_color;
        }
    } else if (c == '\t') {
        cursor_pos += 8;
        video_memory[cursor_pos] = ' ';
        video_memory[cursor_pos + 1] = terminal_color;
    } else {
        video_memory[cursor_pos] = c;
        video_memory[cursor_pos + 1] = terminal_color;
        cursor_pos += 2;
    }
    update_cursor();
}

// Prints a null-terminated string
void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

// ==========================================================================
// 4. INTERRUPT DESCRIPTOR TABLE (IDT)
// ==========================================================================
/*
    IDT is an array of exactly 256 entries in RAM. Each entry is an 8-byte block of data called a "Gate."
    The CPU goes through a loop, filling these entries with memory addresses. It is essentially writing down rules:

    "If someone triggers Interrupt 0 (Divide by Zero), jump to memory address 0x..."
    "If someone triggers Interrupt 14 (Page Fault), jump to memory address 0x..."

    Finally, the CPU executes a very special assembly instruction: lidt (Load IDT).
    It takes the physical memory address of this array and burns it into a special hardware register inside the CPU called the IDTR. 
    Now, the CPU permanently knows where the emergency rulebook is located in RAM.
*/


/*
    When you press 'K', the keyboard sends an electrical pulse to the PIC. The PIC tells the CPU: 
    "Execute Event 33!" The CPU stops everything, looks at slot 33 in the IDT, and blindly jumps to the memory address written there. 
    That address points to your keyboard_handler_main function. Inside that function is where your code actually asks the keyboard, 
    "Okay, what key was it?" and finds out it was 'K'.
*/

struct idt_entry {
    uint16_t base_low;  // The bottom 16 bits of your function's memory address.
    uint16_t sel;       // You put 0x08 here. This tells the CPU, "The code I am jumping to belongs to the Kernel, so give it full permissions."
    uint8_t  always0;   // Literally must be a zero. The hardware demands it.
    uint8_t  flags;     // ou put 0x8E here. 0x8E in binary is 10001110. It tells the CPU: "This gate is Present (1), it runs in Ring 0 Kernel Mode (00), and it is a 32-bit Interrupt Gate (01110)."
    uint16_t base_high; // The top 16 bits of your function's memory address.
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;         // 2 bytes: The size of the table
    uint32_t base;          // 4 bytes: The memory address of the table
} __attribute__((packed));  // "packed" tells C not to add invisible padding

struct idt_entry idt[256];
struct idt_ptr idtp;

/*
    If we want the CPU to jump to a 32-bit memory address (let's say 0x12345678), why don't we just have uint32_t address;? 
    Why chop it into base_low and base_high?
    The Answer: 1980s backwards compatibility. When Intel built the 32-bit 386 processor, they had to make it compatible with the old 16-bit 286 processor. 
    Because of how the old chips were wired, Intel forced programmers to chop their 32-bit addresses in half and stick the old flags in the middle.

    The lidt assembly instruction refuses to just take a memory address. 
    It demands a very specific 6-byte package. 
    It needs to know exactly how big the table is (2 bytes), followed immediately by where the table is (4 bytes).

*/

void idt_set_gate(unsigned char num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

// Installs the IDT directly into the CPU
void idt_install() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;  //This calculates 8 bytes * 256 entries = 2048. Intel oddly requires the size to be exactly one byte less than the true size, so we subtract 1 to get 2047.
    idtp.base = (uint32_t) &idt;                        // This just grabs the physical memory address of your idt array.
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);                       // Initalize all mapped functions to null, we will fill it later
    }
    __asm__ volatile("lidt %0" : : "m" (idtp));
}

// ==========================================================================
// 5. PROGRAMMABLE INTERRUPT CONTROLLER (PIC)
// ==========================================================================

// Remaps the hardware interrupts to prevent fatal CPU exceptions

/*


*/


void pic_remap() {
    unsigned char a1 = port_byte_in(PIC1_DATA); // 0x21 is the Master PIC's Data Port
    unsigned char a2 = port_byte_in(PIC2_DATA); // 0xA1 is the Slave PIC's Data Port

    /*
        When you read from the PIC's Data port before you start reconfiguring it, the PIC returns its Interrupt Mask Register (IMR).

        What is the IMR?
        It is exactly 8 bits (1 byte) long. Each bit represents one of the hardware pins plugged into the PIC.
        If a bit is 1, that device is Masked (Muted). The PIC will ignore it. If a bit is 0, that device is Unmasked (Listening).

        The CPU's Perspective:
        When the CPU executes these two lines, it is asking the Master and Slave PICs: 
        "Hey, before I wipe your memory and completely reconfigure your offsets, 
        tell me exactly which hardware devices are currently muted and which ones are unmuted."

        After the CPU finishes rewiring the PICs, it writes those exact same bytes back to the mailboxes. 
        It is saying: "Okay, reconfiguration complete. Go back to muting whoever you were muting before we started."


        Master PIC (a1):
        Pin 0: System Timer (The PIT we built!)
        Pin 1: Keyboard
        Pin 2: [The wire connecting to the Slave PIC]
        Pin 3: COM2 (Serial Port)
        Pin 4: COM1 (Serial Port)

        Slave PIC (a2):
        Pin 8: Real-Time Clock
        Pin 12: PS/2 Mouse
        Pin 14: Primary Hard Drive (ATA)
        Pin 15: Secondary Hard Drive
    */


    /*
    
        The 8259 PIC chips were designed in the 1970s. Your modern CPU runs at billions of cycles per second (Gigahertz), but the PIC chip is incredibly slow.
        If your C code blasts all these outb commands at the PIC instantly, the PIC will choke, drop the data, and break. 
        io_wait() is usually a tiny assembly function that wastes exactly 1 microsecond of time, 
        giving the ancient PIC chip enough time to swallow the data before the CPU shoves the next byte down its throat.
    
    */


    // Wake up calls
    port_byte_out(PIC1_COMMAND, ICW1_INIT); io_wait();
    port_byte_out(PIC2_COMMAND, ICW1_INIT); io_wait();
    
    // We send the new starting numbers to the Data ports.
    port_byte_out(PIC1_DATA, 0x20); io_wait(); // 0x20 is Decimal 32
    port_byte_out(PIC2_DATA, 0x28); io_wait(); // 0x28 is Decimal 40

    /*
    
        The CPU is saying: "Master PIC, your new starting offset is 32. 
        If your pin 0 fires (Timer), tell me it's Event 32. Slave PIC, your starting offset is 40."
        Why: This successfully moves the hardware signals out of the way of the CPU Panic codes (which occupy 0 through 31).
    
    */

    // Physical waring
    port_byte_out(PIC1_DATA, 4); io_wait();
    port_byte_out(PIC2_DATA, 2); io_wait();
    

    // We are running on standard Intel x86 computer architecture.
    port_byte_out(PIC1_DATA, ICW4_8086); io_wait();
    port_byte_out(PIC2_DATA, ICW4_8086); io_wait();
    
    // Remap prev ones
    port_byte_out(PIC1_DATA, a1);
    port_byte_out(PIC2_DATA, a2);
}

// Unmasks a specific IRQ line
void irq_clear_mask(unsigned char irq_line) {
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) { port = PIC1_DATA; } 
    else { port = PIC2_DATA; irq_line -= 8; }
    
    value = port_byte_in(port);
    value &= ~(1 << irq_line);
    port_byte_out(port, value);
}

// ==========================================================================
// 6. ASYNCHRONOUS KEYBOARD DRIVER
// ==========================================================================

// --- Global Keyboard State ---
volatile int shift_pressed = 0; // Add this with your other volatile variables!

// Standard Keyboard Map (Lowercase and Numpad)
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,   
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*', 0, ' ',
    0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,  0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// Shifted Keyboard Map (Uppercase and Symbols)
const char keyboard_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,   
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*', 0, ' ',
    0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,  0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

extern void keyboard_isr();

// The Interrupt Service Routine
void keyboard_handler_main() {
    unsigned char status = port_byte_in(0x64);
    
    if (status & 0x01) {
        unsigned char scancode = port_byte_in(0x60);
        
        // --- 1. Catch Shift Key Releases (Break Codes) ---
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
        } 
        // --- 2. Catch Shift Key Presses (Make Codes) ---
        else if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        } 
        // --- 3. Handle Normal Keys ---
        else if (scancode < 0x80) { // Key Down event
            
            // Pick the right map based on our shift state!
            char c = shift_pressed ? keyboard_map_shifted[scancode] : keyboard_map[scancode];
            
            if (c != 0) {
                if (c == '\b') {
                    if (kbd_buffer_index > 0) {
                        kbd_buffer_index--;      
                        print_char(c);           
                    }
                } 
                else if (c == '\n') {
                    kbd_buffer[kbd_buffer_index] = '\0'; 
                    kbd_enter_pressed = 1;               
                    print_char(c);                       
                } 
                else {
                    if (kbd_buffer_index < KBD_BUFFER_SIZE - 1) {
                        kbd_buffer[kbd_buffer_index] = c;
                        kbd_buffer_index++;
                        print_char(c);
                    }
                }
            }
        }
    }
    // End of Interrupt (EOI)
    port_byte_out(PIC1_COMMAND, 0x20);
}

// ==========================================================================
// 3. STANDARD LIBRARY UTILITIES (libc replacements)
// ==========================================================================

// Converts an integer to an ASCII string (supports base 10 and 16)
void itoa(int num, char* str, int base) {
    int i = 0, is_negative = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if (num < 0 && base == 10) { is_negative = 1; num = -num; }

    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        num = num / base;
    }
    if (is_negative) { str[i++] = '-'; }
    str[i] = '\0';

    // Reverse the string
    int start = 0, end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++; end--;
    }
}

// Converts an ASCII string to an integer
int atoi(const char* str) {
    int result = 0, sign = 1, i = 0;
    if (str[0] == '-') { sign = -1; i++; }
    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            result = result * 10 + (str[i] - '0');
        } else break;
    }
    return sign * result;
}

// Custom variadic print function
int printf(const char* format, ...) {
    int i;
    va_list args;
    va_start(args, format);

    for (i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++; 
            switch (format[i]) {
                case 'd': { 
                    int num = va_arg(args, int);
                    char buffer[32];
                    itoa(num, buffer, 10);
                    print_string(buffer);
                    break;
                }
                case 'x': {
                    int num = va_arg(args, int);
                    char buffer[32];
                    print_string("0x");
                    itoa(num, buffer, 16);
                    print_string(buffer);
                    break;
                }
                case 's': { 
                    char* str = va_arg(args, char*);
                    print_string(str);
                    break;
                }
                case 'c': { 
                    char c = (char)va_arg(args, int); 
                    print_char(c);
                    break;
                }
                case '%': print_char('%'); break;
            }
        } else {
            print_char(format[i]);
        }
    }
    va_end(args);
    return i;
}


int scanf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // 1. Reset the keyboard buffer state
    kbd_buffer_index = 0;
    kbd_enter_pressed = 0;

    // 2. THE WAITING LOOP
    while (!kbd_enter_pressed) {
        // 'hlt' puts the CPU into a low-power sleep state. 
        // It wakes up instantly when an interrupt (like a keystroke) happens!
        __asm__ volatile("hlt"); 
    }

    // 3. PARSE THE BUFFER
    int buf_idx = 0;
    int parsed_count = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            
            // Skip spaces in the user's input
            while (kbd_buffer[buf_idx] == ' ') buf_idx++;

            switch (format[i]) {
                case 'd': { // Parse Integer
                    // Note: va_arg needs a POINTER to the integer for scanf!
                    int* out_int = va_arg(args, int*);
                    
                    int sign = 1;
                    if (kbd_buffer[buf_idx] == '-') {
                        sign = -1;
                        buf_idx++;
                    }
                    
                    int res = 0;
                    while (kbd_buffer[buf_idx] >= '0' && kbd_buffer[buf_idx] <= '9') {
                        res = res * 10 + (kbd_buffer[buf_idx] - '0');
                        buf_idx++;
                    }
                    *out_int = res * sign;
                    parsed_count++;
                    break;
                }
                case 's': { // Parse String
                    char* out_str = va_arg(args, char*);
                    int s_idx = 0;
                    while (kbd_buffer[buf_idx] != ' ' && kbd_buffer[buf_idx] != '\0') {
                        out_str[s_idx++] = kbd_buffer[buf_idx++];
                    }
                    out_str[s_idx] = '\0';
                    parsed_count++;
                    break;
                }
                case 'c': { // Parse Single Character
                    char* out_char = va_arg(args, char*);
                    *out_char = kbd_buffer[buf_idx++];
                    parsed_count++;
                    break;
                }
            }
        } else if (format[i] == ' ') {
            // If the format has a space, skip spaces in the buffer
            while (kbd_buffer[buf_idx] == ' ') buf_idx++;
        } else {
            // Match literal characters (like commas)
            if (kbd_buffer[buf_idx] == format[i]) buf_idx++;
        }
    }

    va_end(args);
    return parsed_count;
}

// Reads an entire line of text, including spaces, until the user presses Enter
void gets(char* out_str) {
    // 1. Reset the keyboard buffer state
    kbd_buffer_index = 0;
    kbd_enter_pressed = 0;

    // 2. Wait for the user to press Enter
    while (!kbd_enter_pressed) {
        // Sleep until a hardware interrupt wakes the CPU
        __asm__ volatile("hlt"); 
    }

    // 3. Copy the entire buffer, spaces and all!
    int i = 0;
    while (kbd_buffer[i] != '\0') {
        out_str[i] = kbd_buffer[i];
        i++;
    }
    
    // 4. Null-terminate the user's string
    out_str[i] = '\0'; 
}


#endif