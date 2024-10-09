#include "AM2320.h"

// Buffer para recibir datos del sensor
static uint8_t _buf[8];

// Función interna para despertar el sensor AM2320
static void AM2320_WakeSensor(_sAM2320Control *ctrl) {
    ctrl->I2C_Start();
    ctrl->I2C_WriteByte(0xB8);  // Comando de escritura a la dirección I2C del sensor (0x5C << 1)
    ctrl->I2C_Stop();
    ctrl->DelayUs(800);  // Retraso de 800 microsegundos para despertar el sensor
}

// Función interna para calcular el CRC16
static uint16_t AM2320_CRC16(uint8_t *data, size_t len) {
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

// Inicialización del sensor AM2320
void AM2320_Init(_sAM2320Control *ctrl) {
    ctrl->I2C_Init();
}

// Función para leer los registros del sensor AM2320
static int AM2320_ReadRegisters(_sAM2320Control *ctrl, uint8_t reg, uint8_t len) {
    // Despertar el sensor
    AM2320_WakeSensor(ctrl);

    // Enviar comando de lectura
    ctrl->I2C_Start();
    ctrl->I2C_WriteByte(0xB8);  // Comando de escritura a la dirección I2C del sensor (0x5C << 1)
    ctrl->I2C_WriteByte(0x03);  // Función de lectura (0x03)
    ctrl->I2C_WriteByte(reg);   // Dirección del registro inicial
    ctrl->I2C_WriteByte(len);   // Número de bytes a leer
    ctrl->I2C_Stop();
    ctrl->DelayUs(1500);  // Retraso de 1.5 ms según las especificaciones del sensor

    // Leer los datos del sensor
    ctrl->I2C_Start();
    ctrl->I2C_WriteByte(0xB9);  // Comando de lectura a la dirección I2C del sensor (0x5C << 1) | 1
    ctrl->I2C_ReadBytes(_buf, len + 4);  // Leer len + 4 bytes (2 bytes adicionales para CRC)
    ctrl->I2C_Stop();

    // Verificar el CRC
    uint16_t received_crc = (_buf[len + 3] << 8) | _buf[len + 2];
    if (AM2320_CRC16(_buf, len + 2) != received_crc) {
        return -1; // Error de CRC
    }

    return 0; // OK
}

// Función para obtener los valores de temperatura y humedad del sensor AM2320
int AM2320_GetSensorData(_sAM2320Control *ctrl, float *temperature, float *humidity) {
    int ret = AM2320_ReadRegisters(ctrl, 0x00, 4); // Leer 4 bytes desde el registro 0x00
    if (ret != 0) {
        return ret;
    }

    // Convertir la humedad y la temperatura
    uint16_t hum_raw = (_buf[2] << 8) | _buf[3];
    uint16_t temp_raw = (_buf[4] << 8) | _buf[5];

    *humidity = hum_raw / 10.0;
    *temperature = ((temp_raw & 0x7FFF) / 10.0) * ((temp_raw & 0x8000) ? -1 : 1);

    return 0; // OK
}
