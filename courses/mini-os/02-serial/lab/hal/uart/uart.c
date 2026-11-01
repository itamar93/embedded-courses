#include "uart.h"

static void enable_clocks(void) {
    // TODO 1: USART2 talks through port A pins, and sits on the APB1 bus.
    // Enable the GPIOA clock and the USART2 clock (UART2_RCC_EN in RCC->APB1ENR).
}

static void configure_pins() {
    // TODO 2: put PA2 (TX) and PA3 (RX) in alternate function mode,
    // and route both to USART2 with alternate function UART2_AF (AF7).
}

static uint32_t calculate_baudrate(void) {
    // TODO 3: return the BRR divider for UART2_BAUDRATE at APB1_CLK_FREQ.
    // Round to the nearest integer instead of truncating.
    return 0;
}

static void configure_baud_rate(void) {
    USART2->BRR = calculate_baudrate();
}

static void enable_uart(void) {
    // Enable USART, Transmit, and Receive
    USART2->CR1 = UART2_ENABLE_BIT | UART2_TE_BIT | UART2_RE_BIT;
}

void uart_init(void) {
    enable_clocks();
    configure_pins();
    configure_baud_rate();
    enable_uart();
}

uint32_t uart_receive_char(void) {
    while (!(USART2->SR & UART2_RXNE_BIT)); // Wait for Data
    return USART2->DR & 0xFF;
}

void uart_send_data(uint8_t *data, uint32_t length) {
    // TODO 4: for each byte, wait until TXE is set in SR, then write it to DR.
}
