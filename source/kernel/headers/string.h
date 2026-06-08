// 1. Check if STRING_H has been defined yet
#ifndef STRING_H   
// 2. If it hasn't, define it now!
#define STRING_H   

// --- ALL YOUR EXISTING STRING CODE GOES IN BETWEEN ---

int strcmp(const char* str1, const char* str2) {
    while(*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strlen(const char *str){
    int count = 0;
    for(int i = 0; str[i] != '\0'; i++){
        ++count;
    }
    return count;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    
    while (*d != '\0') {
        d++;
    }
    
    while (*src != '\0') {
        *d = *src;
        d++;
        src++;
    }
    
    *d = '\0';
    
    return dest;
}

// Copies the C string pointed to by src into the array pointed to by dest.
// It includes the terminating null character.
char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    
    // Copy each character one by one until we hit the null terminator
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    
    // Append the null terminator to the very end of the new string
    *dest = '\0';
    
    return original_dest;
}

static inline void* memset(void* ptr, int value, unsigned int num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

static inline void* memcpy(void* dest, const void* src, unsigned int num) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (num--) *d++ = *s++;
    return dest;
}


int tokenize(char *str, char **args) {
    int count = 0;
    int in_word = 0;
    while (*str) {
        if (*str == ' ' || *str == '\n') {
            *str = '\0'; // Replace space with a null terminator
            in_word = 0;
        } else if (!in_word) {
            args[count++] = str; // Save the start of the new word
            in_word = 1;
        }
        str++;
    }
    return count;
}
#endif