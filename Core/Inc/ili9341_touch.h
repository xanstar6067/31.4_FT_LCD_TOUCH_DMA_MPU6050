#ifndef __ILI9341_TOUCH_H__
#define __ILI9341_TOUCH_H__

#include <stdbool.h>
#include "ili9341.h"
#include "ili9341_config.h"  // Новый конфигурационный файл

#define ILI9341_TOUCH_SPI_PORT hspi2
extern SPI_HandleTypeDef ILI9341_TOUCH_SPI_PORT;

// Глобальные переменные для доступа из main.c
extern uint32_t raw_x, raw_y;           // Сырые координаты
extern uint16_t touch_x, touch_y;       // Преобразованные координаты
extern uint16_t update_counter;         // Счётчик для обновления текста

void ILI9341_TouchUnselect();
bool ILI9341_TouchPressed();
bool ILI9341_TouchGetCoordinates(uint16_t* x, uint16_t* y);

#endif // __ILI9341_TOUCH_H__
