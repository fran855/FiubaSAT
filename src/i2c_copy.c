#include "i2c.h"

/**************************************** ESTRUCTURAS Y VARIABLES ****************************************/
/**
 * @brief Estructura para manejar el periférico I2C
 * 
 * @param i2c_id Identificador del periférico I2C (I2C1 o I2C2)
 * @param txq Cola de mensajes para transmitir datos
 * @param rxq Cola de mensajes para recibir datos
 * @param mutex Semáforo para controlar el acceso a los registros del periférico
 * @param request Semáforo binario para solicitar acceso al periférico
 */
typedef struct {
    uint32_t i2c_id;
    QueueHandle_t responses;
    SemaphoreHandle_t mutex;
} i2c_t;

/**
 * @brief Estructura para manejar un mensaje I2C
 * 
 * @param addr Dirección I2C del esclavo
 * @param data Datos a enviar
 * @param request true si es una solicitud
 */
typedef struct {
    uint8_t addr;  // Dirección I2C del esclavo
    uint8_t data[I2C_MAX_BUFFER];  // Datos a enviar
    size_t length;  // Longitud de los datos
    bool request;  // true si es una solicitud
} msg_t;

// Handlers de los puertos I2C
static i2c_t i2c1;
static i2c_t i2c2;

/**************************************** FUNCIONES PRIVADAS ****************************************/
/**
 * @brief Obtiene el handler de I2C basado en el identificador de 32 bits del periférico
 * 
 * @param i2c_id Identificador del periférico I2C (I2C1 o I2C2)
 * @return Puntero a la estructura que maneja el periférico I2C correspondiente o NULL si no se encuentra
 */

static i2c_t * get_i2c(uint32_t i2c_id) {
    switch(i2c_id) {
        case I2C1:
            return &i2c1;
        case I2C2:
            return &i2c2;
        default:
            return NULL;
    }
}

/**
 * @brief Encola un mensaje en la cola especificada
 * 
 * @param msg Puntero al mensaje a encolar [REVISAR] (!)
 * @param queue Cola donde se encolará el mensaje
 * @return pdPASS si el mensaje fue encolado, pdFALSE si la cola está llena o hubo un error
 */
static BaseType_t enqueue_i2c_msg(msg_t *msg, QueueHandle_t queue) {
    if (uxQueueSpacesAvailable(queue) == 0) {
        return pdFALSE;
    }

    msg_t msg_copy;
    memcpy(&msg_copy, msg, sizeof(msg_t)); // Copiar el mensaje
    if (xQueueSend(queue, &msg_copy, pdMS_TO_TICKS(10)) != pdPASS) {
        return pdFALSE;
    }
    return pdPASS;
}


/**
 * @brief Desencola un mensaje de la cola especificada
 * 
 * @param queue Cola de mensajes
 * @return Mensaje desencolado
 */
static msg_t dequeue_i2c_msg(QueueHandle_t queue) {
    msg_t msg;
    if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(10)) != pdPASS) {
        msg.addr = 0;
        return msg;
    }
    return msg;
}

/**
 * @brief Espera hasta que el periférico I2C especificado esté listo
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @return void
 */

static void i2c_wait_until_ready(uint32_t i2c_id) {
    while (I2C_SR2(i2c_id) & I2C_SR2_BUSY) {
        taskYIELD();
    }
}

/**
 * @brief Inicia una comunicación I2C con el esclavo especificado en modo lectura/escritura
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @param addr Dirección de 8 bits del esclavo I2C
 * @param read true si se va a leer, false si se va a escribir
 * @return pdPASS si la comunicación fue exitosa, pdFALSE si hubo un error
 */
static BaseType_t i2c_start(uint32_t i2c_id, uint8_t addr, bool read) {
    i2c_wait_until_ready(i2c_id);
    i2c_send_start(i2c_id);

    // Esperar hasta que el bit de Start esté establecido
    while (!(I2C_SR1(i2c_id) & I2C_SR1_SB)) {
        taskYIELD();
    }

    i2c_send_7bit_address(i2c_id, addr, read ? I2C_READ : I2C_WRITE);

    // Esperar hasta que el bit de Address esté establecido
    while (!(I2C_SR1(i2c_id) & I2C_SR1_ADDR)) {
        // Verificar si ocurrió un NACK
        if (I2C_SR1(i2c_id) & I2C_SR1_AF) {
            I2C_SR1(i2c_id) &= ~I2C_SR1_AF; // Limpiar bandera de fallo de ACK
            i2c_send_stop(i2c_id); // Detener comunicación
            return pdFALSE; // Indicar que la comunicación falló
        }
        taskYIELD();
    }

    // Limpiar la bandera de dirección
    (void)I2C_SR2(i2c_id);
    
    return pdPASS; // Indicar que la comunicación fue exitosa
}

/**
 * @brief Envia un byte de datos por I2C en modo WRITE
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @param data Byte de datos a enviar
 * @return pdPASS si el byte fue enviado, pdFALSE si hubo un error
 */
static BaseType_t i2c_write(uint32_t i2c_id, uint8_t data) {
    i2c_send_data(i2c_id, data);
    // Esperar hasta que se complete la transferencia de datos
    while (!(I2C_SR1(i2c_id) & I2C_SR1_BTF)) {
        taskYIELD();
    }

    // Comprobar el bit de ACK después de la transferencia
    if (I2C_SR1(i2c_id) & I2C_SR1_AF) {
        // No se recibió ACK del esclavo (NACK)
        // Limpia la bandera de fallo de ACK
        I2C_SR1(i2c_id) &= ~I2C_SR1_AF;
        // Manejar el error de NACK aquí (por ejemplo, reenviar el mensaje o detener la comunicación)
        return pdFALSE;
    }

    return pdPASS;
}

/**
 * @brief Lee un byte de datos por I2C en modo READ
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @param last true si es el último byte a leer, false si no
 * @return uint8_t Byte de datos leído [REVISAR: tratamiento de error?] (!)
 */
static uint8_t i2c_read(uint32_t i2c_id, bool last) {
    if (last) {
        i2c_disable_ack(i2c_id);
    } else {
        i2c_enable_ack(i2c_id);
    }

    while (!(I2C_SR1(i2c_id) & I2C_SR1_RxNE)) {
        // Esperar hasta que el buffer de recepción no esté vacío
        taskYIELD();
    }

    return i2c_get_data(i2c_id);
}

/**************************************** FUNCIONES PUBLICAS ****************************************/

/**
 * @brief Configura y habilita el periférico I2C especificado [REVISAR: habilitacion por separado?] (!)
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @return pdPASS si la configuración fue exitosa, pdFALSE si hubo un error
 */
BaseType_t i2c_setup(uint32_t i2c_id) {
    i2c_t * i2c = get_i2c(i2c_id);
    
    if(i2c_id == I2C1){
        i2c -> i2c_id = I2C1;
        rcc_periph_clock_enable(RCC_GPIOB); // Habilitar el reloj para el puerto B
        rcc_periph_clock_enable(RCC_I2C1); // Habilitar el reloj para I2C2

        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                    GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C1_SCL | GPIO_I2C1_SDA); // Configurar pines para I2C2

        i2c_peripheral_disable(i2c->i2c_id); // Desactivar I2C2 antes de configurar
        rcc_periph_reset_pulse(RST_I2C1);

        i2c_set_standard_mode(i2c->i2c_id); // Configurar modo estándar
        i2c_set_clock_frequency(i2c->i2c_id, I2C_CR2_FREQ_36MHZ); // Configurar frecuencia del reloj a 36 MHz
        i2c_set_trise(i2c->i2c_id, 36); // Configurar tiempo de subida
        i2c_set_dutycycle(i2c->i2c_id, I2C_CCR_DUTY_DIV2); // Configurar ciclo de trabajo
        i2c_set_ccr(i2c->i2c_id, 180); // Configurar CCR para 100 kHz
        i2c_peripheral_enable(i2c->i2c_id); // Habilitar I2C2
    } else if(i2c_id == I2C2){
        i2c -> i2c_id = I2C2;
        rcc_periph_clock_enable(RCC_GPIOB); // Habilitar el reloj para el puerto B
        rcc_periph_clock_enable(RCC_I2C2); // Habilitar el reloj para I2C2

        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
                    GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C2_SCL | GPIO_I2C2_SDA); // Configurar pines para I2C2

        i2c_peripheral_disable(i2c->i2c_id); // Desactivar I2C2 antes de configurar
        rcc_periph_reset_pulse(RST_I2C2);

        i2c_set_standard_mode(i2c->i2c_id); // Configurar modo estándar
        i2c_set_clock_frequency(i2c->i2c_id, I2C_CR2_FREQ_36MHZ); // Configurar frecuencia del reloj a 36 MHz
        i2c_set_trise(i2c->i2c_id, 36); // Configurar tiempo de subida
        i2c_set_dutycycle(i2c->i2c_id, I2C_CCR_DUTY_DIV2); // Configurar ciclo de trabajo
        i2c_set_ccr(i2c->i2c_id, 180); // Configurar CCR para 100 kHz
        i2c_peripheral_enable(i2c->i2c_id); // Habilitar I2C2
    }
    
    if ((i2c -> mutex = xSemaphoreCreateMutex()) == NULL)
        return pdFALSE;

    if((i2c -> responses = xQueueCreate(10, sizeof(uint8_t))) == NULL)
        return pdFALSE;
    
    return pdPASS;
}


/**
 * @brief Realiza una solicitud de datos al esclavo I2C especificado
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * @param addr Dirección de 8 bits del esclavo I2C
 * @param length Longitud de los datos a solicitar
 * 
 * @return pdPASS si la solicitud fue exitosa, pdFALSE si hubo un error
 */
static BaseType_t i2c_make_request(uint32_t i2c_id, uint8_t addr, size_t length) {
    i2c_t *i2c = get_i2c(i2c_id);
    if (i2c == NULL) {
        print_uart("Error: No se pudo obtener el periférico I2C.\n\r");
        return pdFALSE;
    }

    /*if(xSemaphoreTake(i2c -> responses_available, pdMS_TO_TICKS(10)) != pdTRUE){
        return pdFALSE;
    }*/
    uint8_t data;
    if (i2c_start(i2c->i2c_id, addr, true) != pdPASS) {
        print_uart("Error al iniciar la comunicación (RQT).\n\r");
        return pdFALSE;
    }
    //taskENTER_CRITICAL(); // Iniciar sección crítica
    // Almacenar los datos recibidos en el buffer estático
    for (size_t i = 0; i < length; i++) {
        data = i2c_read(i2c->i2c_id, (i == (length - 1))); // true solo para el último byte
        if(xQueueSend(i2c->responses, &data, pdMS_TO_TICKS(10)) != pdPASS){
            print_uart("Error: No se pudo encolar el mensaje de solicitud.\n\r");
            return pdFALSE;
        }
    }
    i2c_send_stop(i2c->i2c_id);
    //taskEXIT_CRITICAL(); // Finalizar sección crítica>
    

    return pdPASS;
}

/**
 * @brief Procedimiento para solicitar la temperatura al sensor HTU21D
 * 
 * @param i2c_id Identificador de 32 bits del periférico I2C (I2C1 o I2C2)
 * 
 * @return pdPASS si la solicitud fue exitosa, pdFALSE si hubo un error
 */

static BaseType_t request_htu21d(uint32_t i2c_id, uint8_t command) {
    BaseType_t status;
    i2c_t *i2c = get_i2c(i2c_id);
    if (i2c == NULL) {
        print_uart("Error: No se pudo obtener el periférico I2C.\n\r");
        return pdFALSE;
    }

    if (xSemaphoreTake(i2c->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (i2c_start(i2c->i2c_id, HTU21D_ADDRESS, false) == pdPASS) {
            i2c_write(i2c->i2c_id, command);
            i2c_send_stop(i2c->i2c_id);
        } else {
            print_uart("Error al iniciar comunicacion (comando).\n\r");
            xSemaphoreGive(i2c->mutex);
            return pdFALSE;
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Espera suficiente para la lectura
        if(i2c_make_request(i2c_id, HTU21D_ADDRESS, 3) != pdPASS){
            print_uart("Error: No se pudo realizar la solicitud.\n\r");
            xSemaphoreGive(i2c->mutex);
            return pdFALSE;
        }
        xSemaphoreGive(i2c->mutex);
        
    } else {
        print_uart("Error: No se pudo obtener el mutex.\n\r");
        return pdFALSE;
    }
    return pdPASS;
}

static BaseType_t reset_htu21d(uint32_t i2c_id){
    i2c_t *i2c = get_i2c(i2c_id);
    if (i2c == NULL) {
        print_uart("Error: No se pudo obtener el periférico I2C.\n\r");
        return pdFALSE;
    }

    if (xSemaphoreTake(i2c->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (i2c_start(i2c->i2c_id, HTU21D_ADDRESS, false) == pdPASS) {
            i2c_write(i2c->i2c_id, SOFT_RESET);
            i2c_send_stop(i2c->i2c_id);
            vTaskDelay(pdMS_TO_TICKS(15));
        } else {
            print_uart("Error al iniciar comunicacion (reset).\n\r");
            xSemaphoreGive(i2c->mutex);
            return pdFALSE;
        }
        xSemaphoreGive(i2c->mutex);
    } else {
        print_uart("Error: No se pudo obtener el mutex.\n\r");
        return pdFALSE;
    }
    return pdPASS;
}

static BaseType_t i2c_send_data_slave(uint32_t i2c_id, uint8_t addr, uint8_t* data, size_t length) {
    i2c_t *i2c = get_i2c(i2c_id);
    if (i2c == NULL) {
        print_uart("Error: No se pudo obtener el periférico I2C.\n\r");
        return pdFALSE;
    }

    if (xSemaphoreTake(i2c->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (i2c_start(i2c->i2c_id, addr, false) != pdPASS) {
            print_uart("Error al iniciar la comunicación (SDS).\n\r");
            xSemaphoreGive(i2c->mutex);
            return pdFALSE;
        }
        for (size_t i = 0; i < length; i++) {
            if (i2c_write(i2c->i2c_id, data[i]) != pdPASS) {
                print_uart("Error al enviar datos (SDS).\n\r");
                xSemaphoreGive(i2c->mutex);
                return pdFALSE;
            }
        }
        i2c_send_stop(i2c->i2c_id);
        xSemaphoreGive(i2c->mutex);
    } else {
        print_uart("Error: No se pudo obtener el mutex.\n\r");
        return pdFALSE;
    }
}






/******************************
 * TESTING
 * ***************************/

/**
 * @brief Imprime un mensaje por UART
 * 
 * @param s Mensaje a imprimir
 * @return void
 */
void print_uart(const char *s){
    UART_puts(USART1, s, pdMS_TO_TICKS(500));
}




/**
 * @brief Tarea de testing para solicitar la temperatura y ebviarla al Arduino
 *       Solicita tres bytes al HTU21D, calcula la temperatura y la envía al Arduino, todo por I2C
 * 
 * @param pvParameters Sin utilizar
 * @return void
 */

void test_request_i2c(void *pvParameters) {
    uint32_t i2c_id = I2C1;
    i2c_t *i2c = get_i2c(i2c_id);
    if (i2c == NULL) {
        print_uart("Error: No se pudo obtener el periférico I2C.\n\r");
        vTaskDelete(NULL);
    }

    if(reset_htu21d(i2c_id) != pdPASS){
        print_uart("Error: No se pudo realizar el reset.\n\r");
        vTaskDelete(NULL);
    }

    for (;;) {
        
        if (request_htu21d(i2c_id, TRIGGER_TEMP_MEASURE_NOHOLD) != pdPASS) {
            print_uart("Error: No se pudo realizar la solicitud.\n\r");
        }

        vTaskDelay(pdMS_TO_TICKS(2500));

        // Desencolo response
        uint8_t data[3];
        uint8_t data_aux;
        
        for(int i = 0; i < 3; i++){
            if(xQueueReceive(i2c->responses, &data_aux, pdMS_TO_TICKS(100)) != pdTRUE){
                print_uart("Error: No se pudo recibir el mensaje.\n\r");
                vTaskDelete(NULL);
            }
            data[i] = data_aux;
        }

        uint16_t rawTemperature = (data[0] << 8) | data[1];
        rawTemperature &= 0xFFFC;

        float temp = -46.85 + (175.72 * rawTemperature / 65536.0);  // Convertimos los datos a temperatura en grados Celsius
        uint8_t* bytes = (uint8_t*)&temp;

        for(size_t i = 0; i < sizeof(float); i++){
            if(i2c_send_data_slave(i2c_id, 0x04, bytes, sizeof(float)) != pdPASS){
                print_uart("Error: No se pudo enviar el mensaje.\n\r");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2500));

        if (request_htu21d(i2c_id, TRIGGER_HUMD_MEASURE_NOHOLD) != pdPASS) {
            print_uart("Error: No se pudo realizar la solicitud.\n\r");
        }

        vTaskDelay(pdMS_TO_TICKS(2500));

        // Desencolo response
        
        for(int i = 0; i < 3; i++){
            if(xQueueReceive(i2c->responses, &data_aux, pdMS_TO_TICKS(100)) != pdTRUE){
                print_uart("Error: No se pudo recibir el mensaje.\n\r");
                vTaskDelete(NULL);
            }
            data[i] = data_aux;
        }

        uint16_t rawHumidity = (data[0] << 8) | data[1];  // Leemos los dos primeros bytes

        rawHumidity &= 0xFFFC;  // Aplicamos máscara para quitar los bits de estado
        float hum = -6.0 + (125.0 * rawHumidity / 65536.0);  // Convertimos los datos a porcentaje de humedad
    
        bytes = (uint8_t*)&hum;

        for(size_t i = 0; i < sizeof(float); i++){
            if(i2c_send_data_slave(i2c_id, 0x04, bytes, sizeof(float)) != pdPASS){
                print_uart("Error: No se pudo enviar el mensaje.\n\r");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2500));

    }
}