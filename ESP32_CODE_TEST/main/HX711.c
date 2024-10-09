#include "HX711.h"
#include <stdlib.h>

// Inicialización del HX711 con un factor de escala
void HX711_Init(_sHX711Handle *hx711, float scaleFactor) {
    hx711->taskData.tareValue = 0;
    hx711->taskData.scaleFactor = scaleFactor;
    hx711->taskData.status = HX711_OK;
}

// Función para tarear la balanza (calibración a cero)
_eHX711Status HX711_Tare(_sHX711Handle *hx711, int samples) {
    int64_t sum = 0;
    for (int i = 0; i < samples; i++) {
        int32_t count = 0;

        // Asegurarse de que el pin DOUT esté en LOW
        while (hx711->ReadPin() == 1); // Espera a que DOUT sea LOW
        
        for (int j = 0; j < 24; j++) {
            hx711->WritePin(1);
            hx711->DelayUs(1);
            count = count << 1 | hx711->ReadPin();
            hx711->WritePin(0);
            hx711->DelayUs(1);
        }

        // Configura la ganancia a 128
        hx711->WritePin(1);
        hx711->DelayUs(1);
        hx711->WritePin(0);

        if (count & 0x800000) {
            count |= 0xFF000000; // Convierte el valor a negativo si es necesario
        }

        sum += count;
    }

    hx711->taskData.tareValue = sum / samples;
    return HX711_OK;
}

// Función para obtener el peso en gramos
float HX711_GetWeight(_sHX711Handle *hx711, int samples) {
    int64_t sum = 0;
    for (int i = 0; i < samples; i++) {
        int32_t count = 0;

        // Asegurarse de que el pin DOUT esté en LOW
        while (hx711->ReadPin() == 1); // Espera a que DOUT sea LOW
        
        for (int j = 0; j < 24; j++) {
            hx711->WritePin(1);
            hx711->DelayUs(1);
            count = count << 1 | hx711->ReadPin();
            hx711->WritePin(0);
            hx711->DelayUs(1);
        }

        // Configura la ganancia a 128
        hx711->WritePin(1);
        hx711->DelayUs(1);
        hx711->WritePin(0);

        if (count & 0x800000) {
            count |= 0xFF000000; // Convierte el valor a negativo si es necesario
        }

        sum += count;
    }

    int32_t rawValue = sum / samples;
    return (rawValue - hx711->taskData.tareValue) / hx711->taskData.scaleFactor;
}


