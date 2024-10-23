#ifndef I2C_TEST_H
#define I2C_TEST_H

#include "i2c.h"
#include "htu21d.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>

#define I2C_ARDUINO_ADDRESS 0x04 // Dirección del esclavo (Arduino)
#define TEST_PORT GPIOB
#define TEST_RCC RCC_GPIOB
#define TEST_I2C_TRIGGER_PIN GPIO0

#define PERIOD_INTERRUPT 2

#define ECHO_LINE_PIN GPIO4
#define ECHO_LINE_PORT GPIOB
#define ECHO_LINE_RCC RCC_GPIOB
#define ECHO_LINE_DELAY 1000 // 500 ms

void setup_timer(void);
void echo_setup(void);
void vLifeLineTask(void *pvParameters);
void test_request_i2c(void *pvParameters);
void print_uart(const char *s);

#endif /* ifndef I2C_TEST_H */