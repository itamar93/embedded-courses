#include "string.h"

uint32_t my_strlen(const char *str) {
    const char *s = str;
    while (*s) {
        s++;
    }
    return s - str;
}