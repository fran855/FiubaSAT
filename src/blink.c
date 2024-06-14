#include "FreeRTOS.h"
#include "blink.h"

void taskBlink(void *args __attribute__((unused))) {
    for (;;) {
        gpio_toggle(GPIOC, GPIO13);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void blink_setup(void) {
    // Enable clock for GPIO channel C
    rcc_periph_clock_enable(RCC_GPIOC);

    // Set pinmode for PC13
	gpio_set_mode(
		GPIOC,
		GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		GPIO13);

	// Turn LED off
	gpio_set(GPIOC, GPIO13);
}