//Modified by Grok3

#ifndef __ILI9341_H__
#define __ILI9341_H__

#include "fonts.h"
#include "main.h"
#include "ili9341_config.h"
#include <stdbool.h>

// SPI порт
#define ILI9341_SPI_PORT hspi1
extern SPI_HandleTypeDef ILI9341_SPI_PORT;

// Биты MADCTL для дисплея
#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_RGB 0x00
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

// Макрос для обмена значений
#define swap(a,b) {int16_t t=a;a=b;b=t;}

// Цвета
#define ILI9341_BLACK   0x0000
#define ILI9341_BLUE    0x001F
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_WHITE   0xFFFF
//Новые цвета
#define ILI9341_ORANGE      0xFCA0  // Яркий оранжевый
#define ILI9341_PURPLE      0x8010  // Тёмно-фиолетовый (оставляем, если подходит) или 0xA41E (яркий)
#define ILI9341_PINK        0xFDB8  // Нежный розовый
#define ILI9341_LIME        0xF3E0  // Совпадает с ILI9341_GREEN (на самом деле вышел коричневый)
#define ILI9341_TEAL        0x0410
#define ILI9341_GRAY        0x8410
#define ILI9341_DARK_BLUE   0x0010
#define ILI9341_DARK_RED    0x8000
#define ILI9341_DARK_GREEN  0x0400
#define ILI9341_BROWN       0xA945


#define ILI9341_COLOR565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

// Управление и инициализация
void ILI9341_Unselect(void);
void ILI9341_Init(void);
void ILI9341_SetOrientation(uint8_t orientation);
void ILI9341_InvertColors(bool invert);

// SPI функции рисования
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
void ILI9341_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ILI9341_DrawLine(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ILI9341_DrawRect(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ILI9341_DrawCircle(uint16_t x0, uint16_t y0, int r, uint16_t color);
void ILI9341_FillCircle(uint16_t x0, uint16_t y0, int r, uint16_t color); // Добавлено

// DMA SPI функции рисования
void ILI9341_FillScreen_DMA(uint16_t color);
void ILI9341_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_WriteString_DMA(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
void ILI9341_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ILI9341_FillCircle_DMA(uint16_t x0, uint16_t y0, int r, uint16_t color); // Добавлено
void ILI9341_DrawImage_DMA_1D(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);

#endif // __ILI9341_H__
