#ifndef __ST7789_TOUCH_H__
#define __ST7789_TOUCH_H__

#include <stdbool.h>
#include "st7789.h"

#define ST7789_TOUCH_SPI_PORT hspi2
extern SPI_HandleTypeDef ST7789_TOUCH_SPI_PORT;

#define ST7789_TOUCH_CS_GPIO_Port  ILI9341_TOUCH_CS_GPIO_Port
#define ST7789_TOUCH_CS_Pin        ILI9341_TOUCH_CS_Pin
#define ST7789_TOUCH_IRQ_GPIO_Port ILI9341_TOUCH_IRQ_GPIO_Port
#define ST7789_TOUCH_IRQ_Pin       ILI9341_TOUCH_IRQ_Pin

extern uint32_t raw_x, raw_y;
extern uint16_t touch_x, touch_y;
extern uint16_t update_counter;

void ST7789_TouchUnselect(void);
bool ST7789_TouchPressed(void);
bool ST7789_TouchGetCoordinates(uint16_t* x, uint16_t* y);

/* Compatibility layer for existing touch/UI code. */
#define ILI9341_TouchUnselect ST7789_TouchUnselect
#define ILI9341_TouchPressed ST7789_TouchPressed
#define ILI9341_TouchGetCoordinates ST7789_TouchGetCoordinates

#endif // __ST7789_TOUCH_H__
