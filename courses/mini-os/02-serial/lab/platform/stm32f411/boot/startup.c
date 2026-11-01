#include <stdint.h>

/* Linker Script Symbols */
extern uint32_t _etext;  // End of code in Flash 
extern uint32_t _sdata;  // Start of data in SRAM
extern uint32_t _edata;  // End of data in SRAM
extern uint32_t _sbss;   // Start of bss in SRAM
extern uint32_t _ebss;   // End of bss in SRAM
extern uint32_t _estack; // Top of Stack

/* Prototypes */
void kmain(void);
void Reset_Handler(void);
void Default_Handler(void);

/* Weak aliases point to Default_Handler if not implemented elsewhere */
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Vector Table */
__attribute__((section(".isr_vector")))
uint32_t vector_table[] = {
    (uint32_t)&_estack,
    (uint32_t)&Reset_Handler,
    (uint32_t)&NMI_Handler,
    (uint32_t)&HardFault_Handler
};

/* Default Handler */
void Default_Handler(void) {
    while(1);
}

/* Reset Handler */
void Reset_Handler(void) {
    /* 1. Copy .data segment from Flash (LMA) to SRAM (VMA) */
    uint32_t *src = &_etext;
    uint32_t *dst = &_sdata;

    while(dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero fill the .bss segment */
    uint32_t *bss = &_sbss;
    while(bss < &_ebss) {
        *bss++ = 0;
    }

    /* Jump to Kernel Main */
    kmain();

    /* Trap if main returns */
    while(1);
}