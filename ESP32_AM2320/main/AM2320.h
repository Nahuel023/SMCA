#ifndef _AM2320_H
#define _AM2320_H

#include <stdint.h>
#include <stddef.h>

// Definición de la estructura de control para las funciones específicas del hardware
typedef struct {
    void (*I2C_Init)(void);                          // Función para inicializar I2C
    void (*I2C_Start)(void);                         // Función para iniciar la comunicación I2C
    void (*I2C_WriteByte)(uint8_t data);             // Función para escribir un byte en I2C
    void (*I2C_ReadBytes)(uint8_t *data, size_t len);  // Función para leer varios bytes en I2C
    void (*I2C_Stop)(void);                          // Función para detener la comunicación I2C
    void (*DelayUs)(int usDelay);                    // Función para realizar un delay en microsegundos
} _sAM2320Control;

// Inicialización del sensor AM2320
void AM2320_Init(_sAM2320Control *ctrl);

// Obtener los datos del sensor AM2320
int AM2320_GetSensorData(_sAM2320Control *ctrl, float *temperature, float *humidity);

#endif /* _AM2320_H */

