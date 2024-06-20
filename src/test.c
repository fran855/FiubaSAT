#include "FreeRTOS.h"
#include <stdio.h>
#include <stdint.h>

#include "uart1.h"
#include "uart2.h"

#include "libopencm3/stm32/rcc.h"

// Función para reiniciar el sistema
void reset_system(void) {
    scb_reset_system();
}

void test_setup() {
    FILE *file;

    file = fopen("datos.txt", "a");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return;
    }
}

void taskTest(void *args __attribute__((unused))){
    char string_1[] = "Testing UART 1\r\n";
    char string_2[] = "Testing UART 2\r\n";

    uint16_t nsent_1 = UART1_puts(string_1);
    uint16_t nsent_2 = UART2_puts(string_2);

    if(nsent_1 != sizeof(string_1) - 1){
        printf("Error al enviar datos por UART1\n");
    }

    if(nsent_2 != sizeof(string_2) - 1){
        printf("Error al enviar datos por UART2\n");
    }
}


