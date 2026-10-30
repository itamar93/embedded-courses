#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "regs.h"

enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_ALTERNATE,
    GPIO_MODE_ANALOG
};

enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_F,
    GPIO_PORT_G,
    GPIO_PORT_H,
    GPIO_PORT_I
};

enum {
    GPIO_PIN_LOW = 0,
    GPIO_PIN_HIGH
};

void gpio_init(uint32_t port);
void gpio_set_mode(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t mode);
void gpio_set_alt_function(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t alt_function);
void gpio_write_pin(GPIO_TypeDef * gpio_struct, uint32_t pin, uint32_t value);


#endif // GPIO_H