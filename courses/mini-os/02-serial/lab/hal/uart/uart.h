#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "../gpio/gpio.h"
#define UART2_TX_PIN 2 // Port A
#define UART2_RX_PIN 3
#define UART2_AF    7
#define UART2_BAUDRATE 9600
#define APB1_CLK_FREQ 16000000UL
#define UART2_CLOCK_ENABLE_BIT (1U << 17)
#define UART2_ENABLE_BIT (1U << 13)
#define UART2_TE_BIT     (1U << 3)
#define UART2_RE_BIT     (1U << 2)
#define UART2_TXE_BIT    (1U << 7)
#define UART2_RXNE_BIT   (1U << 5)
#define UART2_RCC_EN       (1U << 17)

void uart_init(void);
uint32_t uart_receive_char(void);
void uart_send_data(uint8_t *data, uint32_t length);

#endif
