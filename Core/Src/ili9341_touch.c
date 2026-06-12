#include "stm32f4xx_hal.h"
#include "main.h"
#include "ili9341_touch.h"
#include <stdlib.h>

#define READ_X 0xD0
#define READ_Y 0x90

// Глобальные переменные
uint32_t raw_x = 0, raw_y = 0;
uint16_t touch_x = 0, touch_y = 0;
uint16_t update_counter = 0;

static void ILI9341_TouchSelect() {
    HAL_GPIO_WritePin(ILI9341_TOUCH_CS_GPIO_Port, ILI9341_TOUCH_CS_Pin, GPIO_PIN_RESET);
}

void ILI9341_TouchUnselect() {
    HAL_GPIO_WritePin(ILI9341_TOUCH_CS_GPIO_Port, ILI9341_TOUCH_CS_Pin, GPIO_PIN_SET);
}

bool ILI9341_TouchPressed() {
    return HAL_GPIO_ReadPin(ILI9341_TOUCH_IRQ_GPIO_Port, ILI9341_TOUCH_IRQ_Pin) == GPIO_PIN_RESET;
}

bool ILI9341_TouchGetCoordinates(uint16_t* x, uint16_t* y) {
    static const uint8_t cmd_read_x[] = { READ_X };
    static const uint8_t cmd_read_y[] = { READ_Y };
    static const uint8_t zeroes_tx[] = { 0x00, 0x00 };

    ILI9341_TouchSelect();

    uint32_t avg_x = 0, avg_y = 0;
    for(uint8_t i = 0; i < 16; i++) {
        HAL_SPI_Transmit(&ILI9341_TOUCH_SPI_PORT, (uint8_t*)cmd_read_y, sizeof(cmd_read_y), HAL_MAX_DELAY);
        uint8_t y_raw[2];
        HAL_SPI_TransmitReceive(&ILI9341_TOUCH_SPI_PORT, (uint8_t*)zeroes_tx, y_raw, sizeof(y_raw), HAL_MAX_DELAY);

        HAL_SPI_Transmit(&ILI9341_TOUCH_SPI_PORT, (uint8_t*)cmd_read_x, sizeof(cmd_read_x), HAL_MAX_DELAY);
        uint8_t x_raw[2];
        HAL_SPI_TransmitReceive(&ILI9341_TOUCH_SPI_PORT, (uint8_t*)zeroes_tx, x_raw, sizeof(x_raw), HAL_MAX_DELAY);

        avg_x += (((uint16_t)x_raw[0]) << 8) | ((uint16_t)x_raw[1]);
        avg_y += (((uint16_t)y_raw[0]) << 8) | ((uint16_t)y_raw[1]);
    }

    ILI9341_TouchUnselect();

    avg_x = avg_x / 16;
    avg_y = avg_y / 16;

    raw_x = avg_x;
    raw_y = avg_y;

    if(avg_x < ILI9341_TOUCH_MIN_RAW_X) avg_x = ILI9341_TOUCH_MIN_RAW_X;
    if(avg_x > ILI9341_TOUCH_MAX_RAW_X) avg_x = ILI9341_TOUCH_MAX_RAW_X;
    if(avg_y < ILI9341_TOUCH_MIN_RAW_Y) avg_y = ILI9341_TOUCH_MIN_RAW_Y;
    if(avg_y > ILI9341_TOUCH_MAX_RAW_Y) avg_y = ILI9341_TOUCH_MAX_RAW_Y;

    // Динамический расчёт координат
    if (ili9341_orientation == ILI9341_ORIENTATION_LANDSCAPE) {
        *x = ILI9341_TOUCH_SCALE_X - ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) * ILI9341_TOUCH_SCALE_X / (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
        *y = (avg_x - ILI9341_TOUCH_MIN_RAW_X) * ILI9341_TOUCH_SCALE_Y / (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X);
    } else if (ili9341_orientation == ILI9341_ORIENTATION_LANDSCAPE_LEFT) {
        *x = ILI9341_TOUCH_SCALE_X - ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) * ILI9341_TOUCH_SCALE_X / (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
        *y = ILI9341_TOUCH_SCALE_Y - ((avg_x - ILI9341_TOUCH_MIN_RAW_X) * ILI9341_TOUCH_SCALE_Y / (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X));
    } else if (ili9341_orientation == ILI9341_ORIENTATION_PORTRAIT) {
        *x = (avg_x - ILI9341_TOUCH_MIN_RAW_X) * ILI9341_TOUCH_SCALE_X / (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X);
        *y = ILI9341_TOUCH_SCALE_Y - ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) * ILI9341_TOUCH_SCALE_Y / (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
    } else if (ili9341_orientation == ILI9341_ORIENTATION_PORTRAIT_UPSIDE) {
        *x = ILI9341_TOUCH_SCALE_X - ((avg_x - ILI9341_TOUCH_MIN_RAW_X) * ILI9341_TOUCH_SCALE_X / (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X));
        *y = (avg_y - ILI9341_TOUCH_MIN_RAW_Y) * ILI9341_TOUCH_SCALE_Y / (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y);
    }

    return true;
}

 // Фильтр шума (сохранён для тебя, закомментирован)
    /*
    static uint16_t last_x = 0, last_y = 0;
    if (abs(*x - last_x) < 10 && abs(*y - last_y) < 10) { // Было 5
        *x = last_x;
        *y = last_y;
    } else {
        last_x = *x;
        last_y = *y;
    }
    */
