#include "FreeRTOS.h"
#include "task.h"
#include "uart.h"
#include "blink.h"
#include "timers.h"
#include <stdio.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>


// Declaración del prototipo de la función taskUART
static TaskHandle_t blink_handle;
static TimerHandle_t auto_reload_timer;
static void taskPeriodic(void *pvParameters);
void autoReloadCallback(TimerHandle_t xTimer);

/* Handler in case our application overflows the stack */
void vApplicationStackOverflowHook(TaskHandle_t xTask __attribute__((unused)), char *pcTaskName __attribute__((unused))) {
        
	for (;;);
}

static void taskPeriodic(void *pvParameters) {
    TaskHandle_t xHandle = (TaskHandle_t) pvParameters;
    char *taskName = "taskPeriodic is running\n";
    int i = 0;
    for (;;) {
        if (i == 0) {
            vTaskResume(xHandle);
            i = 1;
        } else {
            vTaskSuspend(xHandle);
            i = 0;
        }
        UART_puts(taskName);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void autoReloadCallback(TimerHandle_t xTimer) {
    char *autoReloadMessage = "Timer expired, auto-reloading\n";
    UART_puts(autoReloadMessage);
}

/* Main loop, this is where our program starts */
int main(void) {
    // Setup main clock, using external 8MHz crystal 
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    UART_setup();
    blink_setup();
	xTaskCreate(taskBlink, "LED", 100, NULL, 2, &blink_handle);  // Crear tarea para parpadear el LED
    xTaskCreate(taskPeriodic, "Periodic", 100, (void *)blink_handle, 2, NULL);  // Crear tarea Periódica
    xTaskCreate(taskUART, "UART", 100, NULL, 2, NULL);  // Crear tarea para UART
    
    auto_reload_timer = xTimerCreate("AutoReload", pdMS_TO_TICKS(5000), pdTRUE, (void *) 0, autoReloadCallback);
    if (auto_reload_timer == NULL) {
        UART_puts("Timer creation failed\n");
    } else {
        if (xTimerStart(auto_reload_timer, 0) != pdPASS) {
            UART_puts("Timer start failed\n");
        }
    }

    // Start RTOS Task scheduler
	vTaskStartScheduler();

    // The task scheduler is blocking, so we should never come here...
	for (;;);
	return 0;
}
