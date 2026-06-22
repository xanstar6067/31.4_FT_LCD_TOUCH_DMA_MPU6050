#include "w25qxx.h"

#include <string.h>
#include "display_driver.h"
#include "main.h"

#define W25QXX_COMMAND_RELEASE_POWER_DOWN  0xABU
#define W25QXX_COMMAND_READ_JEDEC_ID        0x9FU
#define W25QXX_COMMAND_READ_STATUS_1        0x05U
#define W25QXX_COMMAND_READ_STATUS_2        0x35U
#define W25QXX_COMMAND_READ_STATUS_3        0x15U
#define W25QXX_COMMAND_READ_UNIQUE_ID       0x4BU
#define W25QXX_SPI_TIMEOUT_MS               100U

static W25QxxResult W25Qxx_WaitForSpi(SPI_HandleTypeDef *hspi) {
    uint32_t start_tick = HAL_GetTick();

    while (HAL_SPI_GetState(hspi) != HAL_SPI_STATE_READY) {
        if ((HAL_GetTick() - start_tick) >= W25QXX_SPI_TIMEOUT_MS) {
            return W25QXX_RESULT_SPI_TIMEOUT;
        }
    }
    return W25QXX_RESULT_OK;
}

static W25QxxResult W25Qxx_Select(SPI_HandleTypeDef *hspi) {
    W25QxxResult result = W25Qxx_WaitForSpi(hspi);

    if (result != W25QXX_RESULT_OK) {
        return result;
    }

    HAL_GPIO_WritePin(DISPLAY_CS_GPIO_Port,
                      DISPLAY_CS_Pin,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port,
                      W25Q_CS_Pin,
                      GPIO_PIN_RESET);
    return W25QXX_RESULT_OK;
}

void W25Qxx_Unselect(void) {
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port,
                      W25Q_CS_Pin,
                      GPIO_PIN_SET);
}

static W25QxxResult W25Qxx_Transmit(SPI_HandleTypeDef *hspi,
                                    const uint8_t *data,
                                    uint16_t size) {
    if (HAL_SPI_Transmit(hspi,
                         (uint8_t *)data,
                         size,
                         W25QXX_SPI_TIMEOUT_MS) != HAL_OK) {
        return W25QXX_RESULT_SPI_ERROR;
    }
    return W25QXX_RESULT_OK;
}

static W25QxxResult W25Qxx_TransmitReceive(
    SPI_HandleTypeDef *hspi,
    const uint8_t *transmit_data,
    uint8_t *receive_data,
    uint16_t size) {
    if (HAL_SPI_TransmitReceive(hspi,
                                (uint8_t *)transmit_data,
                                receive_data,
                                size,
                                W25QXX_SPI_TIMEOUT_MS) != HAL_OK) {
        return W25QXX_RESULT_SPI_ERROR;
    }
    return W25QXX_RESULT_OK;
}

static W25QxxResult W25Qxx_SendCommand(SPI_HandleTypeDef *hspi,
                                       uint8_t command) {
    W25QxxResult result = W25Qxx_Select(hspi);

    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_Transmit(hspi, &command, 1U);
    }
    W25Qxx_Unselect();
    return result;
}

static W25QxxResult W25Qxx_ReadBytes(SPI_HandleTypeDef *hspi,
                                     uint8_t command,
                                     uint8_t dummy_count,
                                     uint8_t *data,
                                     uint8_t data_size) {
    uint8_t transmit_data[16];
    uint8_t receive_data[16];
    uint8_t transfer_size = dummy_count + data_size;
    W25QxxResult result;

    if (transfer_size > sizeof(transmit_data)) {
        return W25QXX_RESULT_SPI_ERROR;
    }

    memset(transmit_data, 0xFF, sizeof(transmit_data));
    result = W25Qxx_Select(hspi);
    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_Transmit(hspi, &command, 1U);
    }
    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_TransmitReceive(hspi,
                                        transmit_data,
                                        receive_data,
                                        transfer_size);
    }
    W25Qxx_Unselect();

    if (result == W25QXX_RESULT_OK) {
        memcpy(data, &receive_data[dummy_count], data_size);
    }
    return result;
}

static uint32_t W25Qxx_DecodeCapacity(uint8_t capacity_id) {
    if ((capacity_id >= 0x10U) && (capacity_id < 0x20U)) {
        return 1UL << capacity_id;
    }
    return 0U;
}

W25QxxResult W25Qxx_ReadInfo(SPI_HandleTypeDef *hspi,
                             W25QxxInfo *info) {
    uint8_t jedec_id[3];
    W25QxxResult result;

    memset(info, 0, sizeof(*info));
    info->result = W25QXX_RESULT_SPI_ERROR;
    W25Qxx_Unselect();

    result = W25Qxx_SendCommand(
        hspi,
        W25QXX_COMMAND_RELEASE_POWER_DOWN);
    if (result != W25QXX_RESULT_OK) {
        info->result = result;
        return result;
    }
    HAL_Delay(1U);

    result = W25Qxx_ReadBytes(hspi,
                              W25QXX_COMMAND_READ_JEDEC_ID,
                              0U,
                              jedec_id,
                              sizeof(jedec_id));
    if (result != W25QXX_RESULT_OK) {
        info->result = result;
        return result;
    }

    info->manufacturer_id = jedec_id[0];
    info->memory_type = jedec_id[1];
    info->capacity_id = jedec_id[2];
    info->capacity_bytes =
        W25Qxx_DecodeCapacity(info->capacity_id);

    if (((jedec_id[0] == 0x00U) &&
         (jedec_id[1] == 0x00U) &&
         (jedec_id[2] == 0x00U)) ||
        ((jedec_id[0] == 0xFFU) &&
         (jedec_id[1] == 0xFFU) &&
         (jedec_id[2] == 0xFFU))) {
        info->result = W25QXX_RESULT_NOT_FOUND;
        return info->result;
    }

    result = W25Qxx_ReadBytes(hspi,
                              W25QXX_COMMAND_READ_STATUS_1,
                              0U,
                              &info->status_register_1,
                              1U);
    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_ReadBytes(hspi,
                                  W25QXX_COMMAND_READ_STATUS_2,
                                  0U,
                                  &info->status_register_2,
                                  1U);
    }
    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_ReadBytes(hspi,
                                  W25QXX_COMMAND_READ_STATUS_3,
                                  0U,
                                  &info->status_register_3,
                                  1U);
    }
    if (result == W25QXX_RESULT_OK) {
        result = W25Qxx_ReadBytes(hspi,
                                  W25QXX_COMMAND_READ_UNIQUE_ID,
                                  4U,
                                  info->unique_id,
                                  sizeof(info->unique_id));
    }

    info->result = result;
    return result;
}
