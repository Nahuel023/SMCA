/*
 * Proyecto: Ejemplo de Sensor DS18B20 con ESP32 utilizando FreeRTOS
 * Autor: Nahuel Medina
 * Fecha: 29 de Agosto de 2024
 *
 * Descripción:
 * Este programa implementa la lectura de un sensor de temperatura DS18B20 conectado a un microcontrolador ESP32.
 * Se utiliza FreeRTOS para gestionar dos tareas principales:
 * 1. Tarea de parpadeo de un LED conectado al GPIO 2.
 * 2. Tarea de medición de temperatura con un DS18B20 conectado al GPIO 4.
 *
 * El programa incluye funciones para configurar y leer del sensor DS18B20, así como un mecanismo 
 * para ajustar el intervalo de lectura de temperatura de manera configurable. 
 * La temperatura se muestra en la consola cada vez que se realiza una lectura válida.
 *
 * Configuraciones:
 * - GPIO 2: Control del LED de estado.
 * - GPIO 4: Pin de datos del sensor DS18B20.
 * - Intervalo de lectura de temperatura predeterminado: 5 segundos.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"  
#include "rom/ets_sys.h"
#include "ONEWire.h"
#include "DS18B20.h"

static const char *TAG = "sensor_example";

/* Configuración del GPIO para el LED */
#define BLINK_GPIO 2
#define DS18B20_PIN 4

static uint8_t s_led_state = 0;

// DS18B20
_sOWHandle ow0;
_sDS18B20Handle ds18b20_0;

int16_t tempDS18B20;
uint32_t readInterval_us = 5000000; // Configurable: 5 segundos por defecto
bool measurementActive = false; // Bandera para indicar si la medición está en curso

/* Prototipos de funciones para el manejo del sensor DS18B20 */
void OneWireSetPinInput(void);
void OneWireSetPinOutput(void);
uint8_t OneWireReadPin(void);
void OneWireWritePin(uint8_t value);
static inline int delayBlocking(int time_us);

/*********************************** FUNCIONES DS18B20 ****************************************/

/**
 * @brief Configura el pin de datos como entrada.
 */
void OneWireSetPinInput(void) {
    gpio_set_direction(DS18B20_PIN, GPIO_MODE_INPUT);
}

/**
 * @brief Configura el pin de datos como salida.
 */
void OneWireSetPinOutput(void) {
    gpio_set_direction(DS18B20_PIN, GPIO_MODE_OUTPUT);
}

/**
 * @brief Lee el estado del pin de datos.
 * 
 * @return uint8_t Nivel lógico del pin (0 o 1).
 */
uint8_t OneWireReadPin(void) {
    return gpio_get_level(DS18B20_PIN);
}

/**
 * @brief Escribe un nivel lógico en el pin de datos.
 * 
 * @param value Nivel lógico a escribir (0 o 1).
 */
void OneWireWritePin(uint8_t value) {
    gpio_set_level(DS18B20_PIN, value);
}

/**
 * @brief Genera un delay bloqueante en microsegundos.
 * 
 * @param time_us Tiempo en microsegundos a esperar.
 * @return int Devuelve 1 después de completar el delay.
 */
static inline int delayBlocking(int time_us) {
   ets_delay_us(time_us);
   return 1;
}

/*********************************** TAREA DE PARPADEO DEL LED ****************************************/

/**
 * @brief Parpadea el LED configurado en el pin BLINK_GPIO.
 */
static void blink_led(void) {
    gpio_set_level(BLINK_GPIO, s_led_state);
}

/**
 * @brief Configura el pin del LED como salida.
 */
static void configure_led(void) {
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

/**
 * @brief Tarea FreeRTOS para controlar el parpadeo del LED.
 * 
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
void led_blink_task(void *pvParameter) {
    while (1) {
        blink_led();
        s_led_state = !s_led_state;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

/*********************************** TAREA DE TEMPERATURA ****************************************/

/**
 * @brief Tarea FreeRTOS para manejar la lectura de temperatura del sensor DS18B20.
 * 
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
void start_temp_task(void *pvParameter) {
    // Configuración del DS18B20
    ow0.DELAYus = &delayBlocking;
    ow0.SETPinInput = &OneWireSetPinInput;
    ow0.SETPinOutput = &OneWireSetPinOutput;
    ow0.ReadPinBit = &OneWireReadPin;
    ow0.WritePinBit = &OneWireWritePin;

    ds18b20_0.OW = &ow0;
    DS18B20_Init(&ds18b20_0, NULL);
    
    uint32_t lastReadTime = esp_timer_get_time();
    uint32_t currentTime_us;
    const TickType_t delayAfterTempRead = pdMS_TO_TICKS(3000); // 3 segundos de espera
    bool measurementActive = false;

    while (1) {
        currentTime_us = esp_timer_get_time();

        // Si la bandera está activa, realizar la medición
        if (measurementActive) {
            DS18B20_Task(&ds18b20_0, currentTime_us);

            // Verificar el estado y leer la temperatura si está lista
            _eDS18B20Status state = DS18B20_Status(&ds18b20_0);
            if (state == DS18B20_ST_TEMPOK) {
                tempDS18B20 = DS18B20_ReadLastTemp(&ds18b20_0);
                ESP_LOGI(TAG, "Temperature: %d.%04d C", tempDS18B20 / 16, (tempDS18B20 % 16) * 625);

                // Desactivar la bandera y esperar 3 segundos antes de la siguiente medición
                measurementActive = false;
                vTaskDelay(delayAfterTempRead);
                lastReadTime = esp_timer_get_time();
            } else if (state == DS18B20_ST_TEMPCRCERROR) {
                ESP_LOGE(TAG, "CRC Error reading temperature!");
            }

            // Ceder el procesador sin una demora fija
            taskYIELD();
        } else {
            // Verificar si ha pasado el intervalo configurado para iniciar una nueva medición
            if ((currentTime_us - lastReadTime) >= readInterval_us) {
                if (DS18B20_Status(&ds18b20_0) == DS18B20_ST_IDLE) {
                    DS18B20_StartReadTemp(&ds18b20_0);
                    ESP_LOGI(TAG, "START READ TEMP");
                    measurementActive = true; // Activar la bandera al iniciar una medición
                } else {
                    ESP_LOGW(TAG, "DS18B20 no está en estado IDLE. Estado actual: %d", DS18B20_Status(&ds18b20_0));
                }
            }

            // Si no es tiempo de iniciar una nueva medición, esperar un poco antes de continuar
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

/*********************************** MAIN ****************************************/

/**
 * @brief Función principal que inicializa las tareas FreeRTOS para el parpadeo del LED y la medición de temperatura.
 */
void app_main(void) {
    // Configurar el LED y crear la tarea de parpadeo
    configure_led();
    xTaskCreate(&led_blink_task, "LED Blink Task", 2048, NULL, 5, NULL);  
    xTaskCreate(&start_temp_task, "TEMP Start Task", 2048, NULL, 5, NULL);  

    // El bucle principal se deja vacío, ya que las tareas FreeRTOS manejan todo
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Esperar para evitar que el bucle se ejecute continuamente
    }
}

/**
 * @brief Ajusta el intervalo de lectura de temperatura.
 * 
 * @param interval_sec Intervalo en segundos entre lecturas de temperatura.
 */
void set_read_interval(uint32_t interval_sec) {
    readInterval_us = interval_sec * 1000000;
}
