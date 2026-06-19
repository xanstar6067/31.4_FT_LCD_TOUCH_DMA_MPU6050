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

#define ST7789_ORIENTATION_PORTRAIT        (ST7789_MADCTL_MX | ST7789_MADCTL_BGR)
#define ST7789_ORIENTATION_PORTRAIT_UPSIDE (ST7789_MADCTL_MY | ST7789_MADCTL_BGR)
#define ST7789_ORIENTATION_LANDSCAPE       (ST7789_MADCTL_MV | ST7789_MADCTL_MX | ST7789_MADCTL_BGR)
#define ST7789_ORIENTATION_LANDSCAPE_LEFT  (ST7789_MADCTL_MV | ST7789_MADCTL_BGR)

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

/* Compatibility layer for existing rendering code. */
#define ILI9341_SPI_PORT ST7789_SPI_PORT

#define ILI9341_MADCTL_MY  ST7789_MADCTL_MY
#define ILI9341_MADCTL_MX  ST7789_MADCTL_MX
#define ILI9341_MADCTL_MV  ST7789_MADCTL_MV
#define ILI9341_MADCTL_ML  ST7789_MADCTL_ML
#define ILI9341_MADCTL_RGB ST7789_MADCTL_RGB
#define ILI9341_MADCTL_BGR ST7789_MADCTL_BGR
#define ILI9341_MADCTL_MH  ST7789_MADCTL_MH

#define ILI9341_ORIENTATION_PORTRAIT        ST7789_ORIENTATION_PORTRAIT
#define ILI9341_ORIENTATION_PORTRAIT_UPSIDE ST7789_ORIENTATION_PORTRAIT_UPSIDE
#define ILI9341_ORIENTATION_LANDSCAPE       ST7789_ORIENTATION_LANDSCAPE
#define ILI9341_ORIENTATION_LANDSCAPE_LEFT  ST7789_ORIENTATION_LANDSCAPE_LEFT

#define ili9341_orientation st7789_orientation
#define ILI9341_SCREEN_WIDTH ST7789_SCREEN_WIDTH
#define ILI9341_SCREEN_HEIGHT ST7789_SCREEN_HEIGHT
#define ILI9341_TOUCH_SCALE_X ST7789_TOUCH_SCALE_X
#define ILI9341_TOUCH_SCALE_Y ST7789_TOUCH_SCALE_Y

#define ILI9341_TOUCH_MIN_RAW_X ST7789_TOUCH_MIN_RAW_X
#define ILI9341_TOUCH_MAX_RAW_X ST7789_TOUCH_MAX_RAW_X
#define ILI9341_TOUCH_MIN_RAW_Y ST7789_TOUCH_MIN_RAW_Y
#define ILI9341_TOUCH_MAX_RAW_Y ST7789_TOUCH_MAX_RAW_Y

#define ILI9341_BLACK   ST7789_BLACK
#define ILI9341_BLUE    ST7789_BLUE
#define ILI9341_RED     ST7789_RED
#define ILI9341_GREEN   ST7789_GREEN
#define ILI9341_CYAN    ST7789_CYAN
#define ILI9341_MAGENTA ST7789_MAGENTA
#define ILI9341_YELLOW  ST7789_YELLOW
#define ILI9341_WHITE   ST7789_WHITE
#define ILI9341_ORANGE      ST7789_ORANGE
#define ILI9341_PURPLE      ST7789_PURPLE
#define ILI9341_PINK        ST7789_PINK
#define ILI9341_LIME        ST7789_LIME
#define ILI9341_TEAL        ST7789_TEAL
#define ILI9341_GRAY        ST7789_GRAY
#define ILI9341_DARK_BLUE   ST7789_DARK_BLUE
#define ILI9341_DARK_RED    ST7789_DARK_RED
#define ILI9341_DARK_GREEN  ST7789_DARK_GREEN
#define ILI9341_BROWN       ST7789_BROWN

#define ILI9341_COLOR565 ST7789_COLOR565

#define ILI9341_Unselect ST7789_Unselect
#define ILI9341_Init ST7789_Init
#define ILI9341_SetOrientation ST7789_SetOrientation
#define ILI9341_InvertColors ST7789_InvertColors
#define ILI9341_DrawPixel ST7789_DrawPixel
#define ILI9341_WriteString ST7789_WriteString
#define ILI9341_FillRectangle ST7789_FillRectangle
#define ILI9341_FillScreen ST7789_FillScreen
#define ILI9341_DrawImage ST7789_DrawImage
#define ILI9341_DrawLine ST7789_DrawLine
#define ILI9341_DrawRect ST7789_DrawRect
#define ILI9341_DrawCircle ST7789_DrawCircle
#define ILI9341_FillCircle ST7789_FillCircle
#define ILI9341_FillScreen_DMA ST7789_FillScreen_DMA
#define ILI9341_FillRectangle_DMA ST7789_FillRectangle_DMA
#define ILI9341_WriteString_DMA ST7789_WriteString_DMA
#define ILI9341_DrawImage_DMA ST7789_DrawImage_DMA
#define ILI9341_FillCircle_DMA ST7789_FillCircle_DMA
#define ILI9341_DrawImage_DMA_1D ST7789_DrawImage_DMA_1D

#endif // __ST7789_H__
