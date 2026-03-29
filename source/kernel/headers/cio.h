#ifndef CIO_H
#define CIO_H

#include <stdarg.h>
#include <stdint.h>

// --- Global Keyboard State ---
#define KBD_BUFFER_SIZE 256
volatile char kbd_buffer[KBD_BUFFER_SIZE];
volatile int kbd_buffer_index = 0;
volatile int kbd_enter_pressed = 0; // Acts as a boolean flag

// VGA Display Dim
#define VGA_COLS 80
#define VGA_ROWS 25

// PIC Ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

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

// Forces the CPU to wait 1 microsecond (allows older hardware to catch up)
static inline void io_wait() {
    port_byte_out(0x80, 0); 
}

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

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(unsigned char num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

// Installs the IDT directly into the CPU
void idt_install() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t) &idt;
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    __asm__ volatile("lidt %0" : : "m" (idtp));
}

// ==========================================================================
// 5. PROGRAMMABLE INTERRUPT CONTROLLER (PIC)
// ==========================================================================

// Remaps the hardware interrupts to prevent fatal CPU exceptions
void pic_remap() {
    unsigned char a1 = port_byte_in(PIC1_DATA);
    unsigned char a2 = port_byte_in(PIC2_DATA);

    port_byte_out(PIC1_COMMAND, ICW1_INIT); io_wait();
    port_byte_out(PIC2_COMMAND, ICW1_INIT); io_wait();
    
    port_byte_out(PIC1_DATA, 0x20); io_wait(); // Master starts at INT 32
    port_byte_out(PIC2_DATA, 0x28); io_wait(); // Slave starts at INT 40
    
    port_byte_out(PIC1_DATA, 4); io_wait();
    port_byte_out(PIC2_DATA, 2); io_wait();
    
    port_byte_out(PIC1_DATA, ICW4_8086); io_wait();
    port_byte_out(PIC2_DATA, ICW4_8086); io_wait();
    
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


// ==========================================================================
// 7. PROGRAMMABLE INTERVAL TIMER (PIT) & SYSTEM CLOCK
// ==========================================================================


volatile uint32_t timer_ticks = 0;
extern void timer_isr();

// The ISR called by the CPU every single millisecond
void timer_handler_main() {
    timer_ticks++; // Increase our system clock by 1 millisecond
    
    // ACKNOWLEDGE THE INTERRUPT (Send EOI to the Master PIC)
    port_byte_out(PIC1_COMMAND, 0x20);
}

// Configures the hardware PIT to fire at a specific frequency
void init_timer(uint32_t freq) {
    // The hardware clock beats at 1193180 Hz. 
    uint32_t divisor = 1193180 / freq;

    // Send the Command Word to the PIT (Port 0x43)
    port_byte_out(0x43, 0x36);

    // Send the divisor to Channel 0 (Port 0x40)
    // We must send it in two pieces: the lower 8 bits, then the upper 8 bits
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// Pauses the execution of the OS for a specific amount of milliseconds
void sleep(uint32_t ms) {
    uint32_t start_ticks = timer_ticks;
    
    // Wait until the system clock has advanced by 'ms' ticks
    while (timer_ticks < start_ticks + ms) {
        // Sleep the CPU to save power while we wait!
        __asm__ volatile("hlt"); 
    }
}


#endif