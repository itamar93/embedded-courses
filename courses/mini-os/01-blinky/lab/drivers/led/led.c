#include "led.h"

void led_init(void) {
    // TODO 3: enable the clock of the LED's port, then make LED_PIN an output.
}

void led_toggle(void) {
    // TODO 4: flip LED_PIN in the port's Output Data Register (ODR).
}

void led_on(void) {
    gpio_write_pin(LED_GPIO, LED_PIN, GPIO_PIN_LOW);   // active low
}

void led_off(void) {
    gpio_write_pin(LED_GPIO, LED_PIN, GPIO_PIN_HIGH);
}
