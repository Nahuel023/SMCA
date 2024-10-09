/*
 * Proyecto: Lectura del Sensor AM2320 con ESP32 utilizando I2C
 * Autor: Nahuel Medina
 * Fecha: 29 de Agosto de 2024
 *
 * Descripción:
 * Este programa implementa la lectura de un sensor de temperatura y humedad AM2320 
 * conectado a un microcontrolador ESP32 mediante el protocolo I2C. 
 * Se configuran las conexiones I2C y se realizan lecturas periódicas del sensor,
 * mostrando los valores de temperatura y humedad en la consola.
 *
 * Configuraciones:
 * - GPIO 21: Pin SDA del bus I2C.
 * - GPIO 22: Pin SCL del bus I2C.
 * - Frecuencia I2C: 100 kHz.
 * - Dirección I2C del sensor: 0x5C.
 * - Intervalo de lectura: 2 segundos.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/ets_sys.h"  // Incluye ets_delay_us()

// Pines y configuración del I2C
#define I2C_MASTER_SCL_IO           22       // Cambia este valor por el pin SCL que estás usando
#define I2C_MASTER_SDA_IO           21       // Cambia este valor por el pin SDA que estás usando
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000  // Frecuencia I2C de 100kHz
#define I2C_MASTER_TIMEOUT_MS       1000

// Dirección del sensor AM2320
#define AM2320_SENSOR_ADDR          0x5C    // Dirección I2C del sensor AM2320
#define TIMEOUT_MS                  1000
#define DELAY_MS                    2000

static const char *TAG = "AM2320_SENSOR";

// Buffer para recibir datos del sensor
uint8_t _buf[8];

/**
 * @brief Función para inicializar el bus I2C en modo maestro.
 * 
 * @return esp_err_t ESP_OK si la configuración es exitosa, de lo contrario un error de ESP-IDF.
 */
static esp_err_t set_i2c(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0));
    return ESP_OK;
}

/**
 * @brief Función para despertar el sensor AM2320 antes de realizar una lectura.
 * 
 * El sensor AM2320 requiere un pulso de inicio y un breve retraso antes de estar listo para la comunicación I2C.
 */
static void wake_sensor() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AM2320_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    ets_delay_us(800);  // Retraso de 800 microsegundos para despertar el sensor
}

/**
 * @brief Función para calcular el CRC16 de los datos recibidos del sensor.
 * 
 * @param data Puntero al buffer de datos.
 * @param len Longitud de los datos a verificar.
 * @return uint16_t CRC calculado.
 */
static uint16_t crc16(uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x01) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Función para leer los registros del sensor AM2320.
 * 
 * @param reg Dirección del registro inicial a leer.
 * @param len Cantidad de bytes a leer.
 * @return esp_err_t ESP_OK si la lectura es exitosa, de lo contrario un error de ESP-IDF.
 */
static esp_err_t read_registers(uint8_t reg, uint8_t len) {
    esp_err_t ret;

    // Despertar el sensor
    wake_sensor();

    // Comando de lectura
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AM2320_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x03, true); // Función de lectura (0x03)
    i2c_master_write_byte(cmd, reg, true);  // Dirección del registro inicial
    i2c_master_write_byte(cmd, len, true);  // Número de bytes a leer
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return ret;
    }
    ets_delay_us(1500);  // Retraso de 1.5 ms según las especificaciones del sensor

    // Leer los datos del sensor
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AM2320_SENSOR_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, _buf, len + 4, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return ret;
    }

    // Verificar el CRC
    uint16_t received_crc = (_buf[len + 3] << 8) | _buf[len + 2];
    if (crc16(_buf, len + 2) != received_crc) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

/**
 * @brief Función para obtener los valores de temperatura y humedad del sensor AM2320.
 * 
 * @param temperature Puntero para almacenar el valor de temperatura (en °C).
 * @param humidity Puntero para almacenar el valor de humedad (en %).
 * @return esp_err_t ESP_OK si la lectura es exitosa, de lo contrario un error de ESP-IDF.
 */
static esp_err_t get_sensor_data(float *temperature, float *humidity) {
    esp_err_t ret = read_registers(0x00, 4); // Leer 4 bytes desde el registro 0x00
    if (ret != ESP_OK) {
        return ret;
    }

    // Convertir la humedad y la temperatura
    uint16_t hum_raw = (_buf[2] << 8) | _buf[3];
    uint16_t temp_raw = (_buf[4] << 8) | _buf[5];

    *humidity = hum_raw / 10.0;
    *temperature = ((temp_raw & 0x7FFF) / 10.0) * ((temp_raw & 0x8000) ? -1 : 1);

    return ESP_OK;
}

/**
 * @brief Función principal que se ejecuta en el núcleo. 
 * Inicializa el bus I2C, lee los valores del sensor AM2320 y los muestra en la consola.
 */
void app_main(void) {
    ESP_ERROR_CHECK(set_i2c());

    float temperature, humidity;
    while (1) {
        esp_err_t ret = get_sensor_data(&temperature, &humidity);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.1f C, Humidity: %.1f%%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read sensor data: %s", esp_err_to_name(ret));
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}
