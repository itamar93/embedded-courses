/* STM32F411 register map. Same layout as the STM32F407 for every register used here. */
#ifndef REGS_H
#define REGS_H

#include <stdint.h>

/* Peripherals Base */
#define PERIPH_BASE         0x40000000UL

/* AHB1 */
#define AHB1PERIPH_OFFSET  0x20000UL
#define AHB1PERIPH_BASE    (PERIPH_BASE + AHB1PERIPH_OFFSET)

/* APB1 */
#define APB1PERIPH_OFFSET  0x00000UL
#define APB1PERIPH_BASE    (PERIPH_BASE + APB1PERIPH_OFFSET)

/* APB2 */
#define APB2PERIPH_OFFSET  0x10000UL
#define APB2PERIPH_BASE    (PERIPH_BASE + APB2PERIPH_OFFSET)

/* RCC Registers */
typedef struct {
    volatile uint32_t DUMMY[12];
    volatile uint32_t AHB1ENR; // Offset 0x30
    volatile uint32_t DUMMY2[3];
    volatile uint32_t APB1ENR; // Offset 0x40
    volatile uint32_t APB2ENR; // Offset 0x44
} RCC_TypeDef;

#define RCC_OFFSET         0x3800UL
#define RCC_BASE           (AHB1PERIPH_BASE + RCC_OFFSET)
#define RCC ((RCC_TypeDef *)RCC_BASE)

/* GPIO structure */
typedef struct {
    volatile uint32_t MODER; // Offset 0x00
    volatile uint32_t OTYPER; // Offset 0x04
    volatile uint32_t OSPEEDR; // Offset 0x08
    volatile uint32_t PUPDR; // Offset 0x0C
    volatile uint32_t IDR; // Offset 0x10
    volatile uint32_t ODR; // Offset 0x14
    volatile uint32_t BSRR; // Offset 0x18
    volatile uint32_t LCKR; // Offset 0x1C
    volatile uint32_t AFRL; // Offset 0x20
    volatile uint32_t AFRH; // Offset 0x24
} GPIO_TypeDef;

/* GPIOA Registers */
#define GPIOA_OFFSET       0x0000UL
#define GPIOA_BASE         (AHB1PERIPH_BASE + GPIOA_OFFSET)
#define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)

/* GPIOB Registers */
#define GPIOB_OFFSET       0x0400UL
#define GPIOB_BASE         (AHB1PERIPH_BASE + GPIOB_OFFSET)
#define GPIOB ((GPIO_TypeDef *)GPIOB_BASE)

/* GPIOC Registers */
#define GPIOC_OFFSET       0x0800UL
#define GPIOC_BASE         (AHB1PERIPH_BASE + GPIOC_OFFSET)
#define GPIOC ((GPIO_TypeDef *)GPIOC_BASE)

/* GPIOD Registers */
#define GPIOD_OFFSET       0xC00UL
#define GPIOD_BASE         (AHB1PERIPH_BASE + GPIOD_OFFSET)
#define GPIOD ((GPIO_TypeDef *)GPIOD_BASE)

/* SysTick Registers */
typedef struct {
    volatile uint32_t CTRL;    // Offset 0x0
    volatile uint32_t LOAD;    // Offset 0x4
    volatile uint32_t VAL;     // Offset 0x8
} SYSTICK_TypeDef;

#define SYSTICK_BASE       0xE000E010UL
#define SYSTICK ((SYSTICK_TypeDef *)SYSTICK_BASE)

/* USART Registers */
typedef struct {
    volatile uint32_t SR;   // Offset 0x00
    volatile uint32_t DR;   // Offset 0x04
    volatile uint32_t BRR;  // Offset 0x08
    volatile uint32_t CR1;  // Offset 0x0C
} USART_TypeDef;

/* USART2 Registers */
#define USART2_OFFSET      0x4400UL
#define USART2_BASE        (APB1PERIPH_BASE + USART2_OFFSET)
#define USART2 ((USART_TypeDef *)USART2_BASE)

/* SPI Registers */
typedef struct {
    volatile uint32_t CR1;   // Offset 0x00
    volatile uint32_t CR2;   // Offset 0x04
    volatile uint32_t SR;    // Offset 0x08
    volatile uint32_t DR;    // Offset 0x0C
} SPI_TypeDef;

/* SPI1 Registers */
#define SPI1_OFFSET        0x3000UL
#define SPI1_BASE          (APB2PERIPH_BASE + SPI1_OFFSET)
#define SPI1               ((SPI_TypeDef *)SPI1_BASE)

#endif