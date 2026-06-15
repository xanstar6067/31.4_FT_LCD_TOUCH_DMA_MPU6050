#ifndef INC_W25QXX_H_
#define INC_W25QXX_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef enum {
    W25QXX_RESULT_OK = 0,
    W25QXX_RESULT_NOT_FOUND,
    W25QXX_RESULT_SPI_ERROR,
    W25QXX_RESULT_SPI_TIMEOUT
} W25QxxResult;

typedef struct {
    W25QxxResult result;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    uint8_t status_register_1;
    uint8_t status_register_2;
    uint8_t status_register_3;
    uint8_t unique_id[8];
    uint32_t capacity_bytes;
} W25QxxInfo;

void W25Qxx_Unselect(void);
W25QxxResult W25Qxx_ReadInfo(SPI_HandleTypeDef *hspi,
                             W25QxxInfo *info);

#endif /* INC_W25QXX_H_ */
