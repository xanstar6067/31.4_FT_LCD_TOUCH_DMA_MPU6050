//Modified by Grok3

#include "stm32f4xx_hal.h"
#include "ili9341.h"
#include <stdlib.h>

// Глобальные переменные
uint8_t ili9341_orientation = ILI9341_ORIENTATION_LANDSCAPE_LEFT; // Начальная ориентация
uint16_t ILI9341_SCREEN_WIDTH = 320;
uint16_t ILI9341_SCREEN_HEIGHT = 240;
uint16_t ILI9341_TOUCH_SCALE_X = 320;
uint16_t ILI9341_TOUCH_SCALE_Y = 240;

// Флаг для отслеживания завершения DMA
volatile uint8_t dma_transfer_complete = 1; // 1 - передача завершена, 0 - в процессе

// Вспомогательные функции
static void ILI9341_Select(void) {
    HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_RESET);
}

void ILI9341_Unselect(void) {
    HAL_GPIO_WritePin(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin, GPIO_PIN_SET);
}

static void ILI9341_Reset(void) {
    HAL_GPIO_WritePin(ILI9341_RES_GPIO_Port, ILI9341_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(ILI9341_RES_GPIO_Port, ILI9341_RES_Pin, GPIO_PIN_SET);
}

static void ILI9341_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&ILI9341_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ILI9341_WriteData(uint8_t* buff, size_t buff_size) {
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    while (buff_size > 0) {
        uint16_t chunk_size = buff_size > 32768 ? 32768 : buff_size;
        HAL_SPI_Transmit(&ILI9341_SPI_PORT, buff, chunk_size, HAL_MAX_DELAY);
        buff += chunk_size;
        buff_size -= chunk_size;
    }
}

static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ILI9341_WriteCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }
    ILI9341_WriteCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }
    ILI9341_WriteCommand(0x2C); // RAMWR
}

static void ILI9341_WaitForDMA(void) {
    while (!dma_transfer_complete) {
        // Ждём завершения передачи
    }
}

// Callback для завершения передачи DMA
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &ILI9341_SPI_PORT) {
        dma_transfer_complete = 1;
    }
}

// Управление и инициализация
void ILI9341_Init(void) {
    ILI9341_Select();
    ILI9341_Reset();

    ILI9341_WriteCommand(0x01); // SOFTWARE RESET
    HAL_Delay(1000);
        
    ILI9341_WriteCommand(0xCB); // POWER CONTROL A
    { uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xCF); // POWER CONTROL B
    { uint8_t data[] = { 0x00, 0xC1, 0x30 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xE8); // DRIVER TIMING CONTROL A
    { uint8_t data[] = { 0x85, 0x00, 0x78 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xEA); // DRIVER TIMING CONTROL B
    { uint8_t data[] = { 0x00, 0x00 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xED); // POWER ON SEQUENCE CONTROL
    { uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xF7); // PUMP RATIO CONTROL
    { uint8_t data[] = { 0x20 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xC0); // POWER CONTROL, VRH[5:0]
    { uint8_t data[] = { 0x23 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xC1); // POWER CONTROL, SAP[2:0]; BT[3:0]
    { uint8_t data[] = { 0x10 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xC5); // VCM CONTROL
    { uint8_t data[] = { 0x3E, 0x28 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xC7); // VCM CONTROL 2
    { uint8_t data[] = { 0x86 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0x3A); // PIXEL FORMAT
    { uint8_t data[] = { 0x55 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xB1); // FRAME RATIO CONTROL, STANDARD RGB COLOR
    { uint8_t data[] = { 0x00, 0x18 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xB6); // DISPLAY FUNCTION CONTROL
    { uint8_t data[] = { 0x08, 0x82, 0x27 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xF2); // 3GAMMA FUNCTION DISABLE
    { uint8_t data[] = { 0x00 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0x26); // GAMMA CURVE SELECTED
    { uint8_t data[] = { 0x01 }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xE0); // POSITIVE GAMMA CORRECTION
    { uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                         0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
      ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0xE1); // NEGATIVE GAMMA CORRECTION
    { uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                         0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
      ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_WriteCommand(0x11); // EXIT SLEEP
    HAL_Delay(120);
    ILI9341_WriteCommand(0x29); // TURN ON DISPLAY

    ILI9341_SetOrientation(ili9341_orientation);
}

void ILI9341_SetOrientation(uint8_t orientation) {
    ILI9341_Select();
    ILI9341_WriteCommand(0x36); // MADCTL
    { uint8_t data[] = { orientation }; ILI9341_WriteData(data, sizeof(data)); }
    ILI9341_Unselect();

    ili9341_orientation = orientation;
    if (orientation == ILI9341_ORIENTATION_PORTRAIT || orientation == ILI9341_ORIENTATION_PORTRAIT_UPSIDE) {
        ILI9341_SCREEN_WIDTH = 240;
        ILI9341_SCREEN_HEIGHT = 320;
        ILI9341_TOUCH_SCALE_X = 240;
        ILI9341_TOUCH_SCALE_Y = 320;
    } else {
        ILI9341_SCREEN_WIDTH = 320;
        ILI9341_SCREEN_HEIGHT = 240;
        ILI9341_TOUCH_SCALE_X = 320;
        ILI9341_TOUCH_SCALE_Y = 240;
    }
}

void ILI9341_InvertColors(bool invert) {
    ILI9341_Select();
    ILI9341_WriteCommand(invert ? 0x21 /* INVON */ : 0x20 /* INVOFF */);
    ILI9341_Unselect();
}

// SPI функции рисования
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + 1, y + 1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ILI9341_WriteData(data, sizeof(data));
    ILI9341_Unselect();
}

static void ILI9341_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, b, j;
    ILI9341_SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);

    for (i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++) {
            if ((b << j) & 0x8000) {
                uint8_t data[] = { color >> 8, color & 0xFF };
                ILI9341_WriteData(data, sizeof(data));
            } else {
                uint8_t data[] = { bgcolor >> 8, bgcolor & 0xFF };
                ILI9341_WriteData(data, sizeof(data));
            }
        }
    }
}

void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ILI9341_Select();
    while (*str) {
        if (x + font.width >= ILI9341_SCREEN_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ILI9341_SCREEN_HEIGHT) break;
            if (*str == ' ') { str++; continue; }
        }
        ILI9341_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
    ILI9341_Unselect();
}

void ILI9341_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_SCREEN_WIDTH) w = ILI9341_SCREEN_WIDTH - x;
    if ((y + h - 1) >= ILI9341_SCREEN_HEIGHT) h = ILI9341_SCREEN_HEIGHT - y;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for (y = h; y > 0; y--) {
        for (x = w; x > 0; x--) {
            HAL_SPI_Transmit(&ILI9341_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
        }
    }
    ILI9341_Unselect();
}

void ILI9341_FillScreen(uint16_t color) {
    ILI9341_FillRectangle(0, 0, ILI9341_SCREEN_WIDTH, ILI9341_SCREEN_HEIGHT, color);
}

void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ILI9341_SCREEN_HEIGHT) return;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ILI9341_WriteData((uint8_t*)data, sizeof(uint16_t) * w * h);
    ILI9341_Unselect();
}

void ILI9341_DrawLine(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    int steep = abs(y2 - y1) > abs(x2 - x1);
    if (steep) { swap(x1, y1); swap(x2, y2); }
    if (x1 > x2) { swap(x1, x2); swap(y1, y2); }
    int dx = x2 - x1;
    int dy = abs(y2 - y1);
    int err = dx / 2;
    int ystep = (y1 < y2) ? 1 : -1;
    for (; x1 <= x2; x1++) {
        if (steep) ILI9341_DrawPixel(y1, x1, color);
        else ILI9341_DrawPixel(x1, y1, color);
        err -= dy;
        if (err < 0) { y1 += ystep; err += dx; }
    }
}

void ILI9341_DrawRect(uint16_t color, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    ILI9341_DrawLine(color, x1, y1, x2, y1);
    ILI9341_DrawLine(color, x2, y1, x2, y2);
    ILI9341_DrawLine(color, x1, y1, x1, y2);
    ILI9341_DrawLine(color, x1, y2, x2, y2);
}

void ILI9341_DrawCircle(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;
    ILI9341_DrawPixel(x0, y0 + r, color);
    ILI9341_DrawPixel(x0, y0 - r, color);
    ILI9341_DrawPixel(x0 + r, y0, color);
    ILI9341_DrawPixel(x0 - r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++;
        ddF_x += 2;
        f += ddF_x;
        ILI9341_DrawPixel(x0 + x, y0 + y, color);
        ILI9341_DrawPixel(x0 - x, y0 + y, color);
        ILI9341_DrawPixel(x0 + x, y0 - y, color);
        ILI9341_DrawPixel(x0 - x, y0 - y, color);
        ILI9341_DrawPixel(x0 + y, y0 + x, color);
        ILI9341_DrawPixel(x0 - y, y0 + x, color);
        ILI9341_DrawPixel(x0 + y, y0 - x, color);
        ILI9341_DrawPixel(x0 - y, y0 - x, color);
    }
}

void ILI9341_FillCircle(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    if (r <= 0) return;

    ILI9341_Select();
    int x = 0;
    int y = r;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;

    // Рисуем центральную горизонтальную линию
    ILI9341_FillRectangle(x0 - r, y0, 2 * r + 1, 1, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        // Заполняем горизонтальные линии для каждой пары координат
        ILI9341_FillRectangle(x0 - x, y0 + y, 2 * x + 1, 1, color);
        ILI9341_FillRectangle(x0 - x, y0 - y, 2 * x + 1, 1, color);
        ILI9341_FillRectangle(x0 - y, y0 + x, 2 * y + 1, 1, color);
        ILI9341_FillRectangle(x0 - y, y0 - x, 2 * y + 1, 1, color);
    }
    ILI9341_Unselect();
}

// DMA SPI функции рисования
void ILI9341_FillScreen_DMA(uint16_t color) {
    ILI9341_Select();
    ILI9341_SetAddressWindow(0, 0, ILI9341_SCREEN_WIDTH - 1, ILI9341_SCREEN_HEIGHT - 1);

    uint8_t color_data[2] = { color >> 8, color & 0xFF };
    static uint8_t fill_buffer[320 * 2]; // Максимальная ширина 320 пикселей * 2 байта
    for (int i = 0; i < 320 * 2; i += 2) {
        fill_buffer[i] = color_data[0];
        fill_buffer[i + 1] = color_data[1];
    }

    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for (uint16_t y = 0; y < ILI9341_SCREEN_HEIGHT; y++) {
        dma_transfer_complete = 0;
        HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, fill_buffer, ILI9341_SCREEN_WIDTH * 2);
        ILI9341_WaitForDMA();
    }
    ILI9341_Unselect();
}

void ILI9341_FillRectangle_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_SCREEN_WIDTH) w = ILI9341_SCREEN_WIDTH - x;
    if ((y + h - 1) >= ILI9341_SCREEN_HEIGHT) h = ILI9341_SCREEN_HEIGHT - y;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint8_t color_data[2] = { color >> 8, color & 0xFF };
    static uint8_t fill_buffer[320 * 2]; // Максимальная ширина 320 пикселей * 2 байта
    for (int i = 0; i < w * 2; i += 2) {
        fill_buffer[i] = color_data[0];
        fill_buffer[i + 1] = color_data[1];
    }

    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for (uint16_t row = 0; row < h; row++) {
        dma_transfer_complete = 0;
        HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, fill_buffer, w * 2);
        ILI9341_WaitForDMA();
    }
    ILI9341_Unselect();
}

void ILI9341_WriteString_DMA(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ILI9341_Select();
    while (*str) {
        if (x + font.width >= ILI9341_SCREEN_WIDTH) {
            x = 0;
            y += font.height;
            if (y + font.height >= ILI9341_SCREEN_HEIGHT) break;
            if (*str == ' ') { str++; continue; }
        }

        uint32_t b, j;
        static uint8_t char_buffer[32 * 32 * 2]; // Максимальный размер шрифта 32x32
        uint16_t char_width = font.width;
        uint16_t char_height = font.height;
        uint16_t buf_index = 0;

        for (uint16_t i = 0; i < char_height; i++) {
            b = font.data[(*str - 32) * char_height + i];
            for (j = 0; j < char_width; j++) {
                if ((b << j) & 0x8000) {
                    char_buffer[buf_index++] = color >> 8;
                    char_buffer[buf_index++] = color & 0xFF;
                } else {
                    char_buffer[buf_index++] = bgcolor >> 8;
                    char_buffer[buf_index++] = bgcolor & 0xFF;
                }
            }
        }

        ILI9341_SetAddressWindow(x, y, x + char_width - 1, y + char_height - 1);
        HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
        dma_transfer_complete = 0;
        HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, char_buffer, char_width * char_height * 2);
        ILI9341_WaitForDMA();

        x += font.width;
        str++;
    }
    ILI9341_Unselect();
}

void ILI9341_DrawImage_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ILI9341_SCREEN_HEIGHT) return;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for (uint16_t row = 0; row < h; row++) {
        dma_transfer_complete = 0;
        HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, (uint8_t*)&data[row * w], w * 2);
        ILI9341_WaitForDMA();
    }
    ILI9341_Unselect();
}

void ILI9341_DrawImage_DMA_1D(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data) {
    if ((x >= ILI9341_SCREEN_WIDTH) || (y >= ILI9341_SCREEN_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_SCREEN_WIDTH) return;
    if ((y + h - 1) >= ILI9341_SCREEN_HEIGHT) return;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for (uint16_t row = 0; row < h; row++) {
        uint32_t offset = row * w * 2;
        uint16_t bytes_to_send = w * 2;
        while (bytes_to_send > 0) {
            uint16_t chunk_size = (bytes_to_send > 65535) ? 65535 : bytes_to_send;
            dma_transfer_complete = 0;
            HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, &data[offset], chunk_size);
            ILI9341_WaitForDMA();
            offset += chunk_size;
            bytes_to_send -= chunk_size;
        }
    }
    ILI9341_Unselect();
}

void ILI9341_FillCircle_DMA(uint16_t x0, uint16_t y0, int r, uint16_t color) {
    if (r <= 0) return;

    ILI9341_Select();
    int x = 0;
    int y = r;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;

    // Рисуем центральную горизонтальную линию с DMA
    ILI9341_FillRectangle_DMA(x0 - r, y0, 2 * r + 1, 1, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        // Заполняем горизонтальные линии с DMA
        ILI9341_FillRectangle_DMA(x0 - x, y0 + y, 2 * x + 1, 1, color);
        ILI9341_FillRectangle_DMA(x0 - x, y0 - y, 2 * x + 1, 1, color);
        ILI9341_FillRectangle_DMA(x0 - y, y0 + x, 2 * y + 1, 1, color);
        ILI9341_FillRectangle_DMA(x0 - y, y0 - x, 2 * y + 1, 1, color);
    }
    ILI9341_Unselect();
}

