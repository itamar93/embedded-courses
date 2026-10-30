#ifndef LED_H
#define LED_H

#include "../../hal/gpio/gpio.h"

/* Black Pill user LED: PC13, wired to 3.3V through the LED, so it is active low */
#define LED_GPIO_PORT GPIO_PORT_C
#define LED_GPIO      GPIOC
#define LED_PIN       13

void led_init(void);
void led_toggle(void);
void led_on(void);
void led_off(void);

#endif
