#include "led.h"

void led_init(void) {
    gpio_init(LED_GPIO_PORT);
    gpio_set_mode(LED_GPIO, LED_PIN, GPIO_MODE_OUTPUT);
}

void led_toggle(void) {
    LED_GPIO->ODR ^= (GPIO_PIN_HIGH << LED_PIN);
}

void led_on(void) {
    gpio_write_pin(LED_GPIO, LED_PIN, GPIO_PIN_LOW);   // active low
}

void led_off(void) {
    gpio_write_pin(LED_GPIO, LED_PIN, GPIO_PIN_HIGH);
}
