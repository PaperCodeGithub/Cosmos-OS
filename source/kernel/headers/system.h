// Queries the physical CPU hardware for its 12-character manufacturer string
// Safely queries the physical CPU hardware without crashing the C compiler
char* get_cpu_vendor() {
    char *vendor_str;
    uint32_t eax = 0, ebx, ecx, edx;

    // Trigger the CPUID instruction safely by pushing and popping EBX
    __asm__ volatile (
        "pushl %%ebx\n\t"         // 1. Save the compiler's EBX register
        "cpuid\n\t"               // 2. Run the hardware command
        "movl %%ebx, %0\n\t"      // 3. Move the hardware's EBX output into our C variable
        "popl %%ebx\n\t"          // 4. Restore the compiler's EBX register
        : "=r"(ebx), "=c"(ecx), "=d"(edx) // Outputs
        : "a"(eax)                        // Inputs
    );

    *((uint32_t*)(vendor_str)) = ebx;
    *((uint32_t*)(vendor_str + 4)) = edx;
    *((uint32_t*)(vendor_str + 8)) = ecx;
    vendor_str[12] = '\0';
    return vendor_str;
}