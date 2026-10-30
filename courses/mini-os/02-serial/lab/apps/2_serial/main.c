#include "drivers/led/led.h"
#include "hal/systick/systick.h"
#include "lib/inc/stdout.h"

void delay(void) {
    systick_delay_ms(2000);
}

void kmain(void) {
    uart_init();
    led_init();
    print_message("Hello, World!\n\r");
    while(1) {
        print_message("LED ON\n\r");
        led_on();
        delay();
        print_message("LED OFF\n\r");
        led_off();
        delay();
    }
}
