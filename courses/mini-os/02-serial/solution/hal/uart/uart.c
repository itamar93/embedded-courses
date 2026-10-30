#include "uart.h"

static void enable_clocks(void) {
    gpio_init(GPIO_PORT_A);
    RCC->APB1ENR |= UART2_RCC_EN;
}

static void configure_pins() {
    // Configure TX Pin (PA2) to Alternate Function Mode
    gpio_set_mode(GPIOA, UART2_TX_PIN, GPIO_MODE_ALTERNATE);

    // Configure RX Pin (PA3) to Alternate Function Mode
    gpio_set_mode(GPIOA, UART2_RX_PIN, GPIO_MODE_ALTERNATE);

    // Set Alternate Function to AF7 (USART2) in AF Low Register (AFRL)
    // TX Pin
    gpio_set_alt_function(GPIOA, UART2_TX_PIN, UART2_AF);

    // RX Pin
    gpio_set_alt_function(GPIOA, UART2_RX_PIN, UART2_AF);
}

static uint32_t calculate_baudrate(void) {
    // (PCLK + (Baud/2)) / Baud
    return (APB1_CLK_FREQ + (UART2_BAUDRATE / 2)) / UART2_BAUDRATE;
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
    for (uint32_t i = 0; i < length; i++) {
        while (!(USART2->SR & UART2_TXE_BIT)); // Wait for Empty
        USART2->DR = data[i];
    }
}
