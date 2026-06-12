/*
 * mpu6050.h
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "main.h"

// Определение I2C порта
#define MPU6050_I2C hi2c1

// Адрес MPU-6050 (по умолчанию 0x68, если AD0 = GND)
#define MPU6050_DEFAULT_ADDRESS 0x68

// Регистры MPU-6050
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H  0x43
#define MPU6050_REG_WHO_AM_I     0x75

// Структура для данных MPU-6050
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} MPU6050_Data_t;

// Функции
void MPU6050_Init(void);
uint8_t MPU6050_Check(void);
void MPU6050_ReadData(MPU6050_Data_t *data);

#endif /* INC_MPU6050_H_ */
