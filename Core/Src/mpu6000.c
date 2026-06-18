/*
 * mpu6000.c
 *
 * Runtime-selecting driver for MPU6050 and MPU6500-compatible IMUs.
 */
#include "mpu6000.h"

extern I2C_HandleTypeDef MPU6000_I2C;

#define MPU6000_I2C_TIMEOUT_MS      20U
#define MPU6000_I2C_ATTEMPTS        3U
#define MPU6000_I2C_RETRY_DELAY_MS  2U

static MPU6000_DeviceType_t detected_device =
    MPU6000_DEVICE_UNKNOWN;
static uint8_t detected_who_am_i = 0U;

static void MPU6000_SetDetectedDevice(uint8_t who_am_i) {
    detected_who_am_i = who_am_i;
    detected_device = MPU6000_DeviceFromWhoAmI(who_am_i);
}

MPU6000_DeviceType_t MPU6000_DeviceFromWhoAmI(uint8_t who_am_i) {
    switch (who_am_i) {
    case MPU6000_WHO_AM_I_MPU6050:
        return MPU6000_DEVICE_MPU6050;
    case MPU6000_WHO_AM_I_MPU6500:
        return MPU6000_DEVICE_MPU6500_COMPAT;
    default:
        return MPU6000_DEVICE_UNKNOWN;
    }
}

uint8_t MPU6000_IsKnownWhoAmI(uint8_t who_am_i) {
    return (MPU6000_DeviceFromWhoAmI(who_am_i) !=
            MPU6000_DEVICE_UNKNOWN);
}

MPU6000_DeviceType_t MPU6000_GetDeviceType(void) {
    return detected_device;
}

const char *MPU6000_GetDeviceNameFromWhoAmI(uint8_t who_am_i) {
    switch (MPU6000_DeviceFromWhoAmI(who_am_i)) {
    case MPU6000_DEVICE_MPU6050:
        return "MPU6050";
    case MPU6000_DEVICE_MPU6500_COMPAT:
        return "MPU6500";
    default:
        return "UNKNOWN";
    }
}

const char *MPU6000_GetDeviceName(void) {
    return MPU6000_GetDeviceNameFromWhoAmI(detected_who_am_i);
}

static void MPU6000_ClearData(MPU6000_Data_t *data) {
    data->accel_x = 0;
    data->accel_y = 0;
    data->accel_z = 0;
    data->temperature = 0;
    data->gyro_x = 0;
    data->gyro_y = 0;
    data->gyro_z = 0;
}

static void MPU6000_RecoverI2C(void) {
    (void)HAL_I2C_DeInit(&MPU6000_I2C);
    HAL_Delay(MPU6000_I2C_RETRY_DELAY_MS);
    (void)HAL_I2C_Init(&MPU6000_I2C);
    HAL_Delay(MPU6000_I2C_RETRY_DELAY_MS);
}

static HAL_StatusTypeDef MPU6000_ReadRegisters(uint8_t reg,
                                               uint8_t *data,
                                               uint16_t size) {
    HAL_StatusTypeDef status = HAL_ERROR;

    for (uint8_t attempt = 0U;
         attempt < MPU6000_I2C_ATTEMPTS;
         attempt++) {
        status = HAL_I2C_Mem_Read(&MPU6000_I2C,
                                  MPU6000_DEFAULT_ADDRESS << 1,
                                  reg,
                                  I2C_MEMADD_SIZE_8BIT,
                                  data,
                                  size,
                                  MPU6000_I2C_TIMEOUT_MS);
        if (status == HAL_OK) {
            return HAL_OK;
        }
        MPU6000_RecoverI2C();
    }
    return status;
}

static HAL_StatusTypeDef MPU6000_WriteRegister(uint8_t reg,
                                               uint8_t value) {
    HAL_StatusTypeDef status = HAL_ERROR;

    for (uint8_t attempt = 0U;
         attempt < MPU6000_I2C_ATTEMPTS;
         attempt++) {
        status = HAL_I2C_Mem_Write(&MPU6000_I2C,
                                   MPU6000_DEFAULT_ADDRESS << 1,
                                   reg,
                                   I2C_MEMADD_SIZE_8BIT,
                                   &value,
                                   1U,
                                   MPU6000_I2C_TIMEOUT_MS);
        if (status == HAL_OK) {
            return HAL_OK;
        }
        MPU6000_RecoverI2C();
    }
    return status;
}

uint8_t MPU6000_Init(void) {
    uint8_t data;

    if (MPU6000_IsKnownWhoAmI(MPU6000_Check()) == 0U) {
        return 0U;
    }

    data = 0x00;
    if (MPU6000_WriteRegister(MPU6000_REG_PWR_MGMT_1,
                              data) != HAL_OK) {
        return 0U;
    }
    HAL_Delay(100U);
    return 1U;
}

uint8_t MPU6000_Check(void) {
    uint8_t who_am_i = 0U;

    if (MPU6000_ReadRegisters(MPU6000_REG_WHO_AM_I,
                              &who_am_i,
                              1U) != HAL_OK) {
        MPU6000_SetDetectedDevice(0U);
        return 0U;
    }
    MPU6000_SetDetectedDevice(who_am_i);
    return who_am_i;
}

int32_t MPU6000_TemperatureCenti(const MPU6000_Data_t *data) {
    if (detected_device == MPU6000_DEVICE_MPU6500_COMPAT) {
        return 2100 + (((int32_t)data->temperature * 10000) /
                       33387);
    }
    return 3653 + (((int32_t)data->temperature * 100) / 340);
}

HAL_StatusTypeDef MPU6000_ReadData(MPU6000_Data_t *data) {
    uint8_t buffer[14] = {0};
    HAL_StatusTypeDef status;

    if (detected_device == MPU6000_DEVICE_UNKNOWN) {
        if (MPU6000_Init() == 0U) {
            MPU6000_ClearData(data);
            return HAL_ERROR;
        }
    }

    status = MPU6000_ReadRegisters(MPU6000_REG_ACCEL_XOUT_H,
                                   buffer,
                                   sizeof(buffer));

    if (status != HAL_OK) {
        MPU6000_ClearData(data);
        (void)MPU6000_Init();
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
