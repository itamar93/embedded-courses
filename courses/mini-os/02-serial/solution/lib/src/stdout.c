#include "stdout.h"

void print_message(const char *msg) {
    uart_send_data((uint8_t*)msg, my_strlen(msg));
}