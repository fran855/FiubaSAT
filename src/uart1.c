#include "FreeRTOS.h"
#include "uart1.h"
#include <stdio.h>

#define SIZE_BUFFER_USART 256

static QueueHandle_t uart1_txq; // TX queue for UART
static QueueHandle_t uart1_rxq; // RX queue for UART

SemaphoreHandle_t uart1_mutex;

void UART1_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART1);

    // -> GPIO
    gpio_set_mode(GPIO_BANK_USART1_TX, 
        GPIO_MODE_OUTPUT_50_MHZ, 
        GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, 
        GPIO_USART1_TX);

    gpio_set_mode(GPIO_BANK_USART1_RX, 
        GPIO_MODE_INPUT, 
        GPIO_CNF_INPUT_FLOAT, 
        GPIO_USART1_RX);

    usart_set_mode(USART1,USART_MODE_TX_RX);
    usart_set_parity(USART1,USART_PARITY_NONE);
    usart_set_baudrate(USART1,115200);
    usart_set_databits(USART1,8);
    usart_set_stopbits(USART1,USART_STOPBITS_1);
    usart_set_flow_control(USART1,USART_FLOWCONTROL_NONE);

    // Habilitar la UART
    usart_enable(USART1);

    // Habilitar la interrupción de recepción de la UART
    usart_enable_rx_interrupt(USART1);
    nvic_enable_irq(NVIC_USART1_IRQ);
    
    // Create a queue for data to transmit from UART
    uart1_txq = xQueueCreate(SIZE_BUFFER_USART, sizeof(uint8_t));
    uart1_rxq = xQueueCreate(SIZE_BUFFER_USART, sizeof(uint8_t));

    // Create a mutex for UART
    uart1_mutex = xSemaphoreCreateBinary();
    if(uart1_mutex == NULL) {
        UART1_puts("Error al crear mutex\n");
    }
    else {
        UART1_puts("Se creó el mutex\n");
    }
    xSemaphoreGive(uart1_mutex);
}

void taskUART1_transmit(void *args __attribute__((unused))) {
    uint8_t ch;
    for (;;) {
        // Receive char to be TX
        while (xQueueReceive(uart1_txq, &ch, pdMS_TO_TICKS(500)) == pdPASS) {
            while (!usart_get_flag(USART1,USART_SR_TXE) )
                taskYIELD(); // Yield until ready
            usart_send_blocking(USART1,ch);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void taskUART1_receive(void *args __attribute__((unused))) {
    uint8_t data;
    for(;;) {
        while (xQueueReceive(uart1_rxq, &data, pdMS_TO_TICKS(500)) == pdPASS) {
            UART1_putchar(data);
            UART2_putchar(data);
            if (data == '\r') {        // Esto es para el Putty
                data = '\n';
                UART1_putchar(data);
                UART2_putchar(data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

uint16_t UART1_puts(const char *s) {
    uint16_t nsent = 0;
    for ( ; *s; s++) {
        // blocks when queue is full
        if(xQueueSend(uart1_txq, s, portMAX_DELAY) != pdTRUE) {
            xQueueReset(uart1_txq);
            return nsent; // Queue full
        }
        nsent++;
    }
    return nsent;
}

void UART1_putchar(char ch) {
    xQueueSend(uart1_txq, &ch, portMAX_DELAY);
}

void usart1_isr() {
    while (usart_get_flag(USART1, USART_SR_RXNE)) {
        uint8_t data = (uint8_t)usart_recv(USART1);  // Leer el byte recibido
        if(xQueueSendToBackFromISR(uart1_rxq, &data, NULL) != pdTRUE) { // Encolar el byte en RXQ
            xQueueReset(uart1_rxq);
        }
    }
}