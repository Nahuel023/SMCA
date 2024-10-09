#ifndef _HX711_H
#define _HX711_H

#include <stdint.h>

// Definición de los estados posibles para el HX711
typedef enum {
    HX711_OK = 0,
    HX711_ERROR = -1,
} _eHX711Status;

// Estructura para manejar el HX711
typedef struct {
    void (*SetPinInput)(void);
    void (*SetPinOutput)(void);
    void (*WritePin)(uint8_t value);
    uint8_t (*ReadPin)(void);
    void (*DelayUs)(int usDelay);
    
    struct {
        int32_t tareValue;
        float scaleFactor;
        _eHX711Status status;
    } taskData;
} _sHX711Handle;

// Prototipos de funciones
void HX711_Init(_sHX711Handle *hx711, float scaleFactor);
_eHX711Status HX711_Tare(_sHX711Handle *hx711, int samples);
float HX711_GetWeight(_sHX711Handle *hx711, int samples);

#endif /* _HX711_H */

