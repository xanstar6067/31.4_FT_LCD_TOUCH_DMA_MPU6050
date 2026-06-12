/*
 * mpu6050.c
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */
#include "mpu6050.h"

// Экземпляр I2C
extern I2C_HandleTypeDef MPU6050_I2C;

// Инициализация MPU-6050
void MPU6050_Init(void) {
    uint8_t data;

    // Проверка соединения
    if (MPU6050_Check() != 0x68) {
        // Ошибка: MPU-6050 не отвечает
    	Error_Handler(); // Можно заменить на вызов Error_Handler()
    }

    // Сброс питания (выход из спящего режима)
    data = 0x00;
    HAL_I2C_Mem_Write(&MPU6050_I2C, MPU6050_DEFAULT_ADDRESS << 1, MPU6050_REG_PWR_MGMT_1, 1, &data, 1, HAL_MAX_DELAY);
    HAL_Delay(100); // Ждём стабилизации
}

// Проверка WHO_AM_I (должен вернуть 0x68)
uint8_t MPU6050_Check(void) {
    uint8_t who_am_i;
    HAL_I2C_Mem_Read(&MPU6050_I2C, MPU6050_DEFAULT_ADDRESS << 1, MPU6050_REG_WHO_AM_I, 1, &who_am_i, 1, HAL_MAX_DELAY);
    return who_am_i;
}

// Чтение данных акселерометра и гироскопа
void MPU6050_ReadData(MPU6050_Data_t *data) {
    uint8_t buffer[14];

    // Читаем 14 байт, начиная с ACCEL_XOUT_H (0x3B)
    HAL_I2C_Mem_Read(&MPU6050_I2C, MPU6050_DEFAULT_ADDRESS << 1, MPU6050_REG_ACCEL_XOUT_H, 1, buffer, 14, HAL_MAX_DELAY);

    // Собираем данные
    data->accel_x = (int16_t)(buffer[0] << 8 | buffer[1]);
    data->accel_y = (int16_t)(buffer[2] << 8 | buffer[3]);
    data->accel_z = (int16_t)(buffer[4] << 8 | buffer[5]);
    data->gyro_x  = (int16_t)(buffer[8] << 8 | buffer[9]);
    data->gyro_y  = (int16_t)(buffer[10] << 8 | buffer[11]);
    data->gyro_z  = (int16_t)(buffer[12] << 8 | buffer[13]);
}

