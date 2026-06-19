#include "stm32f4xx_hal.h"
#include "st7789.h"
#include "display_dma.h"
#include <stdlib.h>

uint8_t st7789_orientation = ST7789_ORIENTATION_LANDSCAPE_LEFT;
uint16_t ST7789_SCREEN_WIDTH = 320;
uint16_t ST7789_SCREEN_HEIGHT = 240;
uint16_t ST7789_TOUCH_SCALE_X = 320;
uint16_t ST7789_TOUCH_SCALE_Y = 240;

#define ST7789_MAX_LINE_PIXELS 320U

static uint8_t st7789_image_line_buffer[ST7789_MAX_LINE_PIXELS * 2U];

static void ST7789_PackRGB565Line(const uint16_t* src,
                                  uint8_t* dst,
                                  uint16_t pixels) {
    for (uint16_t i = 0; i < pixels; i++) {
        uint16_t color = src[i];
        dst[(uint32_t)i * 2U] = (uint8_t)(color >> 8);
        dst[(uint32_t)i * 2U + 1U] = (uint8_t)(color & 0xFF);
    }
}

static void ST7789_SetBacklight(GPIO_PinState state) {
    HAL_GPIO_WritePin(ST7789_BL_GPIO_Port, ST7789_BL_Pin, state);
}

static void ST7789_Select(void) {
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port,
                      W25Q_CS_Pin,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(ST7789_CS_GPIO_Port, ST7789_CS_Pin, GPIO_PIN_RESET);
}

void ST7789_Unselect(void) {
    HAL_GPIO_WritePin(ST7789_CS_GPIO_Port, ST7789_CS_Pin, GPIO_PIN_SET);
}

static void ST7789_Reset(void) {
    HAL_GPIO_WritePin(ST7789_RES_GPIO_Port, ST7789_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(ST7789_RES_GPIO_Port, ST7789_RES_Pin, GPIO_PIN_SET);
    HAL_Delay(120);
}

static void ST7789_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&ST7789_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7789_WriteData(uint8_t* buff, size_t buff_size) {
    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    while (buff_size > 0U) {
        uint16_t chunk_size =
            (buff_size > 32768U) ? 32768U : (uint16_t)buff_size;

        HAL_SPI_Transmit(&ST7789_SPI_PORT, buff, chunk_size, HAL_MAX_DELAY);
        buff += chunk_size;
        buff_size -= chunk_size;
    }
}

static void ST7789_SetAddressWindow(uint16_t x0,
                                    uint16_t y0,
                                    uint16_t x1,
                                    uint16_t y1) {
    x0 += ST7789_XSTART;
    x1 += ST7789_XSTART;
    y0 += ST7789_YSTART;
    y1 += ST7789_YSTART;

    ST7789_WriteCommand(0x2A);
    {
        uint8_t data[] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
        };
        ST7789_WriteData(data, sizeof(data));
    }

    ST7789_WriteCommand(0x2B);
    {
        uint8_t data[] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
        };
        ST7789_WriteData(data, sizeof(data));
    }

    ST7789_WriteCommand(0x2C);
}

static void ST7789_WaitForDMA(void) {
    Display_DMA_WaitForTransfer();
}

static void ST7789_UpdateGeometry(uint8_t orientation) {
    st7789_orientation = orientation;
    if ((orientation == ST7789_ORIENTATION_PORTRAIT) ||
        (orientation == ST7789_ORIENTATION_PORTRAIT_UPSIDE)) {
        ST7789_SCREEN_WIDTH = 240;
        ST7789_SCREEN_HEIGHT = 320;
        ST7789_TOUCH_SCALE_X = 240;
        ST7789_TOUCH_SCALE_Y = 320;
    } else {
        ST7789_SCREEN_WIDTH = 320;
        ST7789_SCREEN_HEIGHT = 240;
        ST7789_TOUCH_SCALE_X = 320;
        ST7789_TOUCH_SCALE_Y = 240;
    }
}

static void ST7789_WriteOrientation(uint8_t orientation) {
    ST7789_WriteCommand(0x36);
    { uint8_t data[] = { orientation }; ST7789_WriteData(data, sizeof(data)); }
    ST7789_UpdateGeometry(orientation);
}

void ST7789_Init(void) {
    ST7789_SetBacklight(ST7789_BL_OFF_STATE);
    ST7789_Unselect();
    ST7789_Reset();
    ST7789_Select();

    ST7789_WriteCommand(0x11);
    HAL_Delay(120);

    ST7789_WriteCommand(0x3A);
    { uint8_t data[] = { 0x55 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xB2);
    { uint8_t data[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xB7);
    { uint8_t data[] = { 0x35 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xBB);
    { uint8_t data[] = { 0x19 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xC0);
    { uint8_t data[] = { 0x2C }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xC2);
    { uint8_t data[] = { 0x01 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xC3);
    { uint8_t data[] = { 0x12 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xC4);
    { uint8_t data[] = { 0x20 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xC6);
    { uint8_t data[] = { 0x0F }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteCommand(0xD0);
    { uint8_t data[] = { 0xA4, 0xA1 }; ST7789_WriteData(data, sizeof(data)); }

    ST7789_WriteOrientation(st7789_orientation);

    ST7789_WriteCommand(0xE0);
    {
        uint8_t data[] = {
            0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
            0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23
        };
        ST7789_WriteData(data, sizeof(data));
    }

    ST7789_WriteCommand(0xE1);
    {
        uint8_t data[] = {
            0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
            0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23
        };
        ST7789_WriteData(data, sizeof(data));
    }

    ST7789_WriteCommand(0x20);
    ST7789_WriteCommand(0x13);
    HAL_Delay(10);

    ST7789_WriteCommand(0x29);
    HAL_Delay(120);

    ST7789_Unselect();
    ST7789_SetBacklight(ST7789_BL_ON_STATE);
}

void ST7789_SetOrientation(uint8_t orientation) {
    ST7789_Select();
    ST7789_WriteOrientation(orientation);
    ST7789_Unselect();
}

void ST7789_InvertColors(bool invert) {
    ST7789_Select();
    ST7789_WriteCommand(invert ? 0x21 : 0x20);
    ST7789_Unselect();
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x, y);
    uint8_t data[] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    ST7789_WriteData(data, sizeof(data));
    ST7789_Unselect();
}

static void ST7789_WriteChar(uint16_t x,
                             uint16_t y,
                             char ch,
                             FontDef font,
                             uint16_t color,
                             uint16_t bgcolor) {
    uint32_t i, b, j;

    ST7789_SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);
    for (i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++) {
            uint16_t pixel = ((b << j) & 0x8000) ? color : bgcolor;
            uint8_t data[] = {
                (uint8_t)(pixel >> 8),
                (uint8_t)(pixel & 0xFF)
            };
            ST7789_WriteData(data, sizeof(data));
        }
    }
}

void ST7789_WriteString(uint16_t x,
                        uint16_t y,
                        const char* str,
                        FontDef font,
                        uint16_t color,
                        uint16_t bgcolor) {
    ST7789_Select();
    while (*str) {
        if (x + font.width >= ST7789_SCREEN_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7789_SCREEN_HEIGHT) break;
            if (*str == ' ') {
                str++;
                continue;
            }
        }
        ST7789_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
    ST7789_Unselect();
}

void ST7789_FillRectangle(uint16_t x,
                          uint16_t y,
                          uint16_t w,
                          uint16_t h,
                          uint16_t color) {
    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_SCREEN_WIDTH) w = ST7789_SCREEN_WIDTH - x;
    if ((y + h - 1) >= ST7789_SCREEN_HEIGHT) h = ST7789_SCREEN_HEIGHT - y;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    uint8_t data[] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };

    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    for (y = h; y > 0U; y--) {
        for (x = w; x > 0U; x--) {
            HAL_SPI_Transmit(&ST7789_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
        }
    }
    ST7789_Unselect();
}

void ST7789_FillScreen(uint16_t color) {
    ST7789_FillRectangle(0, 0, ST7789_SCREEN_WIDTH, ST7789_SCREEN_HEIGHT, color);
}

void ST7789_DrawImage(uint16_t x,
                      uint16_t y,
                      uint16_t w,
                      uint16_t h,
                      const uint16_t* data) {
    if ((w == 0U) || (h == 0U)) return;
    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ST7789_SCREEN_HEIGHT) return;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    for (uint16_t row = 0; row < h; row++) {
        ST7789_PackRGB565Line(&data[(uint32_t)row * w],
                              st7789_image_line_buffer,
                              w);
        ST7789_WriteData(st7789_image_line_buffer, (size_t)w * 2U);
    }
    ST7789_Unselect();
}

void ST7789_DrawLine(uint16_t color,
                     uint16_t x1,
                     uint16_t y1,
                     uint16_t x2,
                     uint16_t y2) {
    int steep = abs(y2 - y1) > abs(x2 - x1);
    if (steep) { swap(x1, y1); swap(x2, y2); }
    if (x1 > x2) { swap(x1, x2); swap(y1, y2); }

    int dx = x2 - x1;
    int dy = abs(y2 - y1);
    int err = dx / 2;
    int ystep = (y1 < y2) ? 1 : -1;

    for (; x1 <= x2; x1++) {
        if (steep) {
            ST7789_DrawPixel(y1, x1, color);
        } else {
            ST7789_DrawPixel(x1, y1, color);
        }
        err -= dy;
        if (err < 0) {
            y1 += ystep;
            err += dx;
        }
    }
}

void ST7789_DrawRect(uint16_t color,
                     uint16_t x1,
                     uint16_t y1,
                     uint16_t x2,
                     uint16_t y2) {
    ST7789_DrawLine(color, x1, y1, x2, y1);
    ST7789_DrawLine(color, x2, y1, x2, y2);
    ST7789_DrawLine(color, x1, y1, x1, y2);
    ST7789_DrawLine(color, x1, y2, x2, y2);
}

void ST7789_DrawCircle(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    ST7789_DrawPixel(x0, y0 + r, color);
    ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0 + r, y0, color);
    ST7789_DrawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + y, y0 + x, color);
        ST7789_DrawPixel(x0 - y, y0 + x, color);
        ST7789_DrawPixel(x0 + y, y0 - x, color);
        ST7789_DrawPixel(x0 - y, y0 - x, color);
    }
}

void ST7789_FillCircle(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    if (r <= 0) return;

    ST7789_Select();
    int x = 0;
    int y = r;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;

    ST7789_FillRectangle(x0 - r, y0, 2 * r + 1, 1, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_FillRectangle(x0 - x, y0 + y, 2 * x + 1, 1, color);
        ST7789_FillRectangle(x0 - x, y0 - y, 2 * x + 1, 1, color);
        ST7789_FillRectangle(x0 - y, y0 + x, 2 * y + 1, 1, color);
        ST7789_FillRectangle(x0 - y, y0 - x, 2 * y + 1, 1, color);
    }
    ST7789_Unselect();
}

void ST7789_FillScreen_DMA(uint16_t color) {
    ST7789_Select();
    ST7789_SetAddressWindow(0, 0,
                            ST7789_SCREEN_WIDTH - 1,
                            ST7789_SCREEN_HEIGHT - 1);

    uint8_t color_data[2] = {
        (uint8_t)(color >> 8),
        (uint8_t)(color & 0xFF)
    };
    static uint8_t fill_buffer[320 * 2];

    for (int i = 0; i < 320 * 2; i += 2) {
        fill_buffer[i] = color_data[0];
        fill_buffer[i + 1] = color_data[1];
    }

    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    for (uint16_t y = 0; y < ST7789_SCREEN_HEIGHT; y++) {
        Display_DMA_MarkBusy();
        HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT,
                             fill_buffer,
                             ST7789_SCREEN_WIDTH * 2);
        ST7789_WaitForDMA();
    }
    ST7789_Unselect();
}

void ST7789_FillRectangle_DMA(uint16_t x,
                              uint16_t y,
                              uint16_t w,
                              uint16_t h,
                              uint16_t color) {
    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_SCREEN_WIDTH) w = ST7789_SCREEN_WIDTH - x;
    if ((y + h - 1) >= ST7789_SCREEN_HEIGHT) h = ST7789_SCREEN_HEIGHT - y;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint8_t color_data[2] = {
        (uint8_t)(color >> 8),
        (uint8_t)(color & 0xFF)
    };
    static uint8_t fill_buffer[320 * 2];

    for (int i = 0; i < w * 2; i += 2) {
        fill_buffer[i] = color_data[0];
        fill_buffer[i + 1] = color_data[1];
    }

    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    for (uint16_t row = 0; row < h; row++) {
        Display_DMA_MarkBusy();
        HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT, fill_buffer, w * 2);
        ST7789_WaitForDMA();
    }
    ST7789_Unselect();
}

void ST7789_WriteString_DMA(uint16_t x,
                            uint16_t y,
                            const char* str,
                            FontDef font,
                            uint16_t color,
                            uint16_t bgcolor) {
    ST7789_Select();
    while (*str) {
        if (x + font.width >= ST7789_SCREEN_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ST7789_SCREEN_HEIGHT) break;
            if (*str == ' ') {
                str++;
                continue;
            }
        }

        uint32_t b, j;
        static uint8_t char_buffer[32 * 32 * 2];
        uint16_t char_width = font.width;
        uint16_t char_height = font.height;
        uint16_t buf_index = 0;

        for (uint16_t i = 0; i < char_height; i++) {
            b = font.data[(*str - 32) * char_height + i];
            for (j = 0; j < char_width; j++) {
                uint16_t pixel = ((b << j) & 0x8000) ? color : bgcolor;
                char_buffer[buf_index++] = (uint8_t)(pixel >> 8);
                char_buffer[buf_index++] = (uint8_t)(pixel & 0xFF);
            }
        }

        ST7789_SetAddressWindow(x,
                                y,
                                x + char_width - 1,
                                y + char_height - 1);
        HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
        Display_DMA_MarkBusy();
        HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT,
                             char_buffer,
                             char_width * char_height * 2);
        ST7789_WaitForDMA();

        x += font.width;
        str++;
    }
    ST7789_Unselect();
}

void ST7789_DrawImage_DMA(uint16_t x,
                          uint16_t y,
                          uint16_t w,
                          uint16_t h,
                          const uint16_t* data) {
    if ((w == 0U) || (h == 0U)) return;
    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ST7789_SCREEN_HEIGHT) return;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    for (uint16_t row = 0; row < h; row++) {
        ST7789_PackRGB565Line(&data[(uint32_t)row * w],
                              st7789_image_line_buffer,
                              w);
        Display_DMA_MarkBusy();
        HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT,
                             st7789_image_line_buffer,
                             w * 2);
        ST7789_WaitForDMA();
    }
    ST7789_Unselect();
}

void ST7789_DrawImage_DMA_1D(uint16_t x,
                             uint16_t y,
                             uint16_t w,
                             uint16_t h,
                             const uint8_t* data) {
    uint32_t offset = 0U;
    uint32_t bytes_to_send;

    if ((x >= ST7789_SCREEN_WIDTH) || (y >= ST7789_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ST7789_SCREEN_HEIGHT) return;

    ST7789_Select();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    HAL_GPIO_WritePin(ST7789_DC_GPIO_Port, ST7789_DC_Pin, GPIO_PIN_SET);
    bytes_to_send = (uint32_t)w * h * 2U;
    while (bytes_to_send > 0U) {
        uint16_t chunk_size =
            (bytes_to_send > 65534U) ? 65534U : (uint16_t)bytes_to_send;

        Display_DMA_MarkBusy();
        HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT, &data[offset], chunk_size);
        ST7789_WaitForDMA();
        offset += chunk_size;
        bytes_to_send -= chunk_size;
    }
    ST7789_Unselect();
}

void ST7789_FillCircle_DMA(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    if (r <= 0) return;

    ST7789_Select();
    int x = 0;
    int y = r;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;

    ST7789_FillRectangle_DMA(x0 - r, y0, 2 * r + 1, 1, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_FillRectangle_DMA(x0 - x, y0 + y, 2 * x + 1, 1, color);
        ST7789_FillRectangle_DMA(x0 - x, y0 - y, 2 * x + 1, 1, color);
        ST7789_FillRectangle_DMA(x0 - y, y0 + x, 2 * y + 1, 1, color);
        ST7789_FillRectangle_DMA(x0 - y, y0 - x, 2 * y + 1, 1, color);
    }
    ST7789_Unselect();
}
