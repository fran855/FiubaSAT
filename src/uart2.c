#include "FreeRTOS.h"
#include "uart2.h"
#include <stdio.h>

#define SIZE_BUFFER_USART 256

static QueueHandle_t uart2_txq; // TX queue for UART
static QueueHandle_t uart2_rxq; // RX queue for UART

SemaphoreHandle_t uart2_mutex;

void UART2_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);

    // -> GPIO
    gpio_set_mode(GPIO_BANK_USART2_TX, 
        GPIO_MODE_OUTPUT_50_MHZ, 
        GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, 
        GPIO_USART2_TX);

    gpio_set_mode(GPIO_BANK_USART2_RX, 
        GPIO_MODE_INPUT, 
        GPIO_CNF_INPUT_FLOAT, 
        GPIO_USART2_RX);

    usart_set_mode(USART2,USART_MODE_TX_RX);
    usart_set_parity(USART2,USART_PARITY_NONE);
    usart_set_baudrate(USART2,115200);
    usart_set_databits(USART2,8);
    usart_set_stopbits(USART2,USART_STOPBITS_1);
    usart_set_flow_control(USART2,USART_FLOWCONTROL_NONE);

    // Habilitar la UART
    usart_enable(USART2);

    // Habilitar la interrupción de recepción de la UART
    usart_enable_rx_interrupt(USART2);
    nvic_enable_irq(NVIC_USART2_IRQ);

    // Create a queue for data to transmit from UART
    uart2_txq = xQueueCreate(SIZE_BUFFER_USART, sizeof(uint8_t));
    uart2_rxq = xQueueCreate(SIZE_BUFFER_USART, sizeof(uint8_t));

    // Create a mutex for UART
    uart2_mutex = xSemaphoreCreateBinary();
    if(uart2_mutex == NULL) {
        UART2_puts("Error al crear mutex\n");
    }
    else {
        UART2_puts("Se creó el mutex\n\r");
    }
    xSemaphoreGive(uart2_mutex);
}

void taskUART2_transmit(void *args __attribute__((unused))) {
    uint8_t ch;
    for (;;) {
        // Receive char to be TX
        while (xQueueReceive(uart2_txq, &ch, pdMS_TO_TICKS(500)) == pdPASS) {
            while (!usart_get_flag(USART2,USART_SR_TXE) )
                taskYIELD(); // Yield until ready
            usart_send_blocking(USART2,ch);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void taskUART2_receive(void *args __attribute__((unused))) {
    uint8_t data;
    for(;;) {
        while (xQueueReceive(uart2_rxq, &data, pdMS_TO_TICKS(500)) == pdPASS) {
            UART1_putchar(data);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

uint16_t UART2_puts(const char *s) {
    uint16_t nsent = 0;
    for ( ; *s; s++) {
        // blocks when queue is full
        if(xQueueSend(uart2_txq, s, portMAX_DELAY) != pdTRUE) {
            xQueueReset(uart2_txq);
            return nsent; // Queue full
        }
        nsent++;
    }
    return nsent;
}

void UART2_putchar(char ch) {
    xQueueSend(uart2_txq, &ch, portMAX_DELAY);
}

void usart2_isr() {
    while (usart_get_flag(USART2, USART_SR_RXNE)) {
        uint8_t data = (uint8_t)usart_recv(USART2);  // Leer el byte recibido
        if(xQueueSendToBackFromISR(uart2_rxq, &data, NULL) != pdTRUE) { // Encolar el byte en RXQ
            xQueueReset(uart2_rxq);
        }
    }
}