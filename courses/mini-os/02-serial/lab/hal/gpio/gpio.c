#include "gpio.h"

void gpio_init(uint32_t port) {
    RCC->AHB1ENR |= (1U << port);
}

void gpio_set_mode(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t mode) {
    gpio_struct->MODER &= ~(3U << (pin * 2)); // Clear bits
    gpio_struct->MODER |=  (mode << (pin * 2)); // Set mode
}

void gpio_set_alt_function(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t alt_function) {
    if (pin < 8) {
        gpio_struct->AFRL &= ~(0xFU << (pin * 4)); // Clear
        gpio_struct->AFRL |=  (alt_function << (pin * 4)); // Set
    } else {
        gpio_struct->AFRH &= ~(0xFU << ((pin - 8) * 4)); // Clear
        gpio_struct->AFRH |=  (alt_function << ((pin - 8) * 4)); // Set
    }
}

void gpio_write_pin(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t value) {
    if (value == GPIO_PIN_HIGH) {
        gpio_struct->ODR |= (1U << pin);
    } else {
        gpio_struct->ODR &= ~(1U << pin);
    }
}
