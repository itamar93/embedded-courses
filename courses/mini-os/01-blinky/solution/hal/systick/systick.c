#include "systick.h"

void systick_delay_ms(uint32_t delay) {
    SYSTICK->LOAD = ONE_MSEC_LOAD - 1; // Load value for 1 ms
    SYSTICK->VAL = 0;                 // Clear current value
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSRC;  // Use processor clock
    SYSTICK->CTRL |= SYSTICK_CTRL_ENABLE; // Enable SysTick

    for (uint32_t i = 0; i < delay; i++) {
        // Wait until the COUNTFLAG is set
        while ((SYSTICK->CTRL & SYSTICK_CTRL_COUNTFLAG) == 0);
    }

    SYSTICK->CTRL = 0; // Disable SysTick
}

void systick_delay_us(uint32_t delay) {
    SYSTICK->LOAD = ONE_uSEC_LOAD - 1; // Load value for 1 us
    SYSTICK->VAL = 0;                 // Clear current value
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSRC;  // Use processor clock
    SYSTICK->CTRL |= SYSTICK_CTRL_ENABLE; // Enable SysTick

    for (uint32_t i = 0; i < delay; i++) {
        // Wait until the COUNTFLAG is set
        while ((SYSTICK->CTRL & SYSTICK_CTRL_COUNTFLAG) == 0);
    }

    SYSTICK->CTRL = 0; // Disable SysTick
}