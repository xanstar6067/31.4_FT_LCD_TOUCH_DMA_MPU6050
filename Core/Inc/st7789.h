#ifndef __ST7789_H__
#define __ST7789_H__

#include "fonts.h"
#include "main.h"
#include <stdbool.h>

#define ST7789_SPI_PORT hspi1
extern SPI_HandleTypeDef ST7789_SPI_PORT;

#define ST7789_MADCTL_MY  0x80
#define ST7789_MADCTL_MX  0x40
#define ST7789_MADCTL_MV  0x20
#define ST7789_MADCTL_ML  0x10
#define ST7789_MADCTL_RGB 0x00
#define ST7789_MADCTL_BGR 0x08
#define ST7789_MADCTL_MH  0x04

#define ST7789_ORIENTATION_PORTRAIT        (ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB)
#define ST7789_ORIENTATION_PORTRAIT_UPSIDE (ST7789_MADCTL_RGB)
#define ST7789_ORIENTATION_LANDSCAPE       (ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB)
#define ST7789_ORIENTATION_LANDSCAPE_LEFT  (ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB)

extern uint8_t st7789_orientation;
extern uint16_t ST7789_SCREEN_WIDTH;
extern uint16_t ST7789_SCREEN_HEIGHT;
extern uint16_t ST7789_TOUCH_SCALE_X;
extern uint16_t ST7789_TOUCH_SCALE_Y;

#define ST7789_TOUCH_MIN_RAW_X 2000
#define ST7789_TOUCH_MAX_RAW_X 30000
#define ST7789_TOUCH_MIN_RAW_Y 2800
#define ST7789_TOUCH_MAX_RAW_Y 30500

#define ST7789_XSTART 0U
#define ST7789_YSTART 0U

#define ST7789_BLACK   0x0000
#define ST7789_BLUE    0x001F
#define ST7789_RED     0xF800
#define ST7789_GREEN   0x07E0
#define ST7789_CYAN    0x07FF
#define ST7789_MAGENTA 0xF81F
#define ST7789_YELLOW  0xFFE0
#define ST7789_WHITE   0xFFFF
#define ST7789_ORANGE      0xFCA0
#define ST7789_PURPLE      0x8010
#define ST7789_PINK        0xFDB8
#define ST7789_LIME        0xF3E0
#define ST7789_TEAL        0x0410
#define ST7789_GRAY        0x8410
#define ST7789_DARK_BLUE   0x0010
#define ST7789_DARK_RED    0x8000
#define ST7789_DARK_GREEN  0x0400
#define ST7789_BROWN       0xA945

#define ST7789_COLOR565(r, g, b) \
    (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

#ifndef swap
#define swap(a,b) {int16_t t=a;a=b;b=t;}
#endif

void ST7789_Unselect(void);
void ST7789_Init(void);
void ST7789_SetOrientation(uint8_t orientation);
void ST7789_InvertColors(bool invert);

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_FillScreen(uint16_t color);
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ST7789_DrawLine(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_DrawRect(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, int r, uint16_t color);
void ST7789_FillCircle(uint16_t x0, uint16_t y0, int r, uint16_t color);

void ST7789_FillScreen_DMA(uint16_t color);
void ST7789_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_WriteString_DMA(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data);
void ST7789_FillCircle_DMA(uint16_t x0, uint16_t y0, int r, uint16_t color);
void ST7789_DrawImage_DMA_1D(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);

#endif // __ST7789_H__
