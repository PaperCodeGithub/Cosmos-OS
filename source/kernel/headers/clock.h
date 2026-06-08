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

/*

    Inside the PIT chip on your motherboard, there is a physical crystal oscillator. 
    ]Because of the exact shape and size of this crystal, it vibrates exactly 1,193,180 times per second (~1.19 MHz). 
    This is a hardware constant that has not changed since the 1980s.

    But 1.19 million ticks a second is way too fast for our OS to handle. It would overwhelm the CPU.
    So, we use a Divisor.

    When you call init_timer(1000), you are telling the OS you want the clock to tick 1,000 times a second (once every millisecond).
    The CPU does the math: 1193180 / 1000 = 1193.
    This tells the PIT: "Count 1,193 raw crystal vibrations, and THEN send one electrical pulse to the CPU."


*/


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

