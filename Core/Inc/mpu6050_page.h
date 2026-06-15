#ifndef INC_MPU6050_PAGE_H_
#define INC_MPU6050_PAGE_H_

#include <stdint.h>
#include "mpu6050.h"

typedef struct {
    MPU6050_Data_t data;
    MPU6050_Data_t displayed_data;
    uint32_t successful_reads;
    uint32_t read_errors;
    uint32_t displayed_reads;
    uint32_t displayed_errors;
    int16_t previous_bubble_x;
    int16_t previous_bubble_y;
    int16_t previous_bars[6];
    int16_t displayed_temperature;
    uint8_t who_am_i;
    uint8_t online;
    uint8_t displayed_who_am_i;
    uint8_t displayed_online;
    uint8_t text_divider;
    uint8_t footer_divider;
    uint8_t identity_divider;
    uint8_t renderer_initialized;
} MPU6050Page;

void MPU6050Page_Init(MPU6050Page *page);
void MPU6050Page_Update(MPU6050Page *page,
                        const MPU6050_Data_t *data,
                        HAL_StatusTypeDef read_status);
void MPU6050Page_Render(MPU6050Page *page);

#endif /* INC_MPU6050_PAGE_H_ */
