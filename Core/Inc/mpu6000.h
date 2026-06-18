/*
 * mpu6000.h
 *
 * Driver for MPU-6000-family IMU modules on I2C.
 * Supported devices:
 * - MPU6050, WHO_AM_I = 0x68
 * - MPU6500-compatible chips, WHO_AM_I = 0x70
 */

#ifndef INC_MPU6000_H_
#define INC_MPU6000_H_

#include "main.h"

#define MPU6000_I2C hi2c1

#define MPU6000_DEFAULT_ADDRESS 0x68

#define MPU6000_REG_PWR_MGMT_1   0x6B
#define MPU6000_REG_ACCEL_XOUT_H 0x3B
#define MPU6000_REG_GYRO_XOUT_H  0x43
#define MPU6000_REG_WHO_AM_I     0x75

#define MPU6000_WHO_AM_I_MPU6050 0x68U
#define MPU6000_WHO_AM_I_MPU6500 0x70U

typedef enum {
    MPU6000_DEVICE_UNKNOWN = 0,
    MPU6000_DEVICE_MPU6050,
    MPU6000_DEVICE_MPU6500_COMPAT
} MPU6000_DeviceType_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temperature;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} MPU6000_Data_t;

uint8_t MPU6000_Init(void);
uint8_t MPU6000_Check(void);
uint8_t MPU6000_IsKnownWhoAmI(uint8_t who_am_i);
MPU6000_DeviceType_t MPU6000_DeviceFromWhoAmI(uint8_t who_am_i);
MPU6000_DeviceType_t MPU6000_GetDeviceType(void);
const char *MPU6000_GetDeviceName(void);
const char *MPU6000_GetDeviceNameFromWhoAmI(uint8_t who_am_i);
int32_t MPU6000_TemperatureCenti(const MPU6000_Data_t *data);
HAL_StatusTypeDef MPU6000_ReadData(MPU6000_Data_t *data);

#endif /* INC_MPU6000_H_ */
