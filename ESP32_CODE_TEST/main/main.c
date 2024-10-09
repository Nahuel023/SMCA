#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "HX711.h"
#include "rom/ets_sys.h"

// Definiciones de los pines del HX711
#define HX711_DOUT_PIN GPIO_NUM_26
#define HX711_SCK_PIN GPIO_NUM_25

static const char *TAG = "HX711_example";

_sHX711Handle hx711;

// Funciones de configuración de pines para HX711
void SetHX711PinInput(void) {
    gpio_set_direction(HX711_DOUT_PIN, GPIO_MODE_INPUT);
}

void SetHX711PinOutput(void) {
    gpio_set_direction(HX711_DOUT_PIN, GPIO_MODE_OUTPUT);
}

uint8_t ReadHX711Pin(void) {
    return gpio_get_level(HX711_DOUT_PIN);
}

void WriteHX711Pin(uint8_t value) {
    gpio_set_level(HX711_SCK_PIN, value);
}

void HX711DelayUs(int usDelay) {
    ets_delay_us(usDelay);
}

void app_main(void) {
    // Inicializar GPIOs para HX711
    gpio_set_direction(HX711_DOUT_PIN, GPIO_MODE_INPUT);   // DOUT
    gpio_set_direction(HX711_SCK_PIN, GPIO_MODE_OUTPUT);  // SCK

    // Asignar funciones al manejador HX711
    hx711.SetPinInput = SetHX711PinInput;
    hx711.SetPinOutput = SetHX711PinOutput;
    hx711.WritePin = WriteHX711Pin;
    hx711.ReadPin = ReadHX711Pin;
    hx711.DelayUs = HX711DelayUs;

    // Inicializar el HX711 con un factor de escala estimado
    HX711_Init(&hx711, 454.45f);

    // Realizar la tara (poner a cero) del sensor
    if (HX711_Tare(&hx711, 10) == HX711_OK) {
        ESP_LOGI(TAG, "Tare completed successfully.");
    } else {
        ESP_LOGE(TAG, "Tare failed.");
    }

    while (1) {
        // Leer el peso en gramos
        float weight = HX711_GetWeight(&hx711, 10);
        ESP_LOGI(TAG, "Weight: %.2f grams", weight);
        
        // Esperar 1 segundo antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



