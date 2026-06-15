/*
 * mpu6050.c
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */
#include "mpu6050.h"

// Экземпляр I2C
extern I2C_HandleTypeDef MPU6050_I2C;

#define MPU6050_I2C_TIMEOUT_MS 20U

uint8_t MPU6050_Init(void) {
    uint8_t data;

    if (MPU6050_Check() != 0x68) {
        return 0U;
    }

    data = 0x00;
    if (HAL_I2C_Mem_Write(&MPU6050_I2C,
                          MPU6050_DEFAULT_ADDRESS << 1,
                          MPU6050_REG_PWR_MGMT_1,
                          I2C_MEMADD_SIZE_8BIT,
                          &data,
                          1U,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
        return 0U;
    }
    HAL_Delay(100U);
    return 1U;
}

uint8_t MPU6050_Check(void) {
    uint8_t who_am_i = 0U;

    if (HAL_I2C_Mem_Read(&MPU6050_I2C,
                         MPU6050_DEFAULT_ADDRESS << 1,
                         MPU6050_REG_WHO_AM_I,
                         I2C_MEMADD_SIZE_8BIT,
                         &who_am_i,
                         1U,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
        return 0U;
    }
    return who_am_i;
}

HAL_StatusTypeDef MPU6050_ReadData(MPU6050_Data_t *data) {
    uint8_t buffer[14] = {0};
    HAL_StatusTypeDef status =
        HAL_I2C_Mem_Read(&MPU6050_I2C,
                         MPU6050_DEFAULT_ADDRESS << 1,
                         MPU6050_REG_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT,
                         buffer,
                         sizeof(buffer),
                         MPU6050_I2C_TIMEOUT_MS);

    if (status != HAL_OK) {
        data->accel_x = 0;
        data->accel_y = 0;
        data->accel_z = 0;
        data->temperature = 0;
        data->gyro_x = 0;
        data->gyro_y = 0;
        data->gyro_z = 0;
        return status;
    }

    data->accel_x = (int16_t)(buffer[0] << 8 | buffer[1]);
    data->accel_y = (int16_t)(buffer[2] << 8 | buffer[3]);
    data->accel_z = (int16_t)(buffer[4] << 8 | buffer[5]);
    data->temperature =
        (int16_t)(buffer[6] << 8 | buffer[7]);
    data->gyro_x  = (int16_t)(buffer[8] << 8 | buffer[9]);
    data->gyro_y  = (int16_t)(buffer[10] << 8 | buffer[11]);
    data->gyro_z  = (int16_t)(buffer[12] << 8 | buffer[13]);
    return HAL_OK;
}

