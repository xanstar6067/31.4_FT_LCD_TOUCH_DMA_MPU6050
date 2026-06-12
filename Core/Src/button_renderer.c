/*
 * button_renderer.c
 *
 *  Created on: Mar 19, 2025
 *      Author: Grok3
 */
#include "button_renderer.h"
#include "ili9341.h"

void ButtonRenderer_Draw(VirtualButton* button) {
    uint16_t width = button->x2 - button->x1 + 1;
    uint16_t height = button->y2 - button->y1 + 1;

    switch (button->type) {
        case BUTTON_TYPE_FILLED:
            ILI9341_FillRectangle_DMA(button->x1, button->y1, width, height, button->fill_color);
            break;

        case BUTTON_TYPE_OUTLINED:
            ILI9341_FillRectangle_DMA(button->x1, button->y1, width, height, button->fill_color);
            ILI9341_DrawRect(button->outline_color, button->x1, button->y1, button->x2, button->y2);
            break;

        case BUTTON_TYPE_BITMAP:
            if (button->bitmap) {
                // Проверяем, чтобы размеры кнопки не превышали изображение
                // Предполагаем, что bitmap — это плоский массив uint16_t размером 240x240
                ILI9341_DrawImage_DMA(button->x1, button->y1, width, height, button->bitmap);
            	//ILI9341_DrawImage_DMA(button->x1, button->y1, width, height, button->bitmap, 240);
                button->x2 = button->x1 + width - 1;  // Обновляем координаты
                button->y2 = button->y1 + height - 1;
            }
            break;
    }
}
/*
 void ButtonRenderer_Draw(VirtualButton* button) {
    uint16_t width = button->x2 - button->x1 + 1;
    uint16_t height = button->y2 - button->y1 + 1;

    switch (button->type) {
        case BUTTON_TYPE_FILLED:
            ILI9341_FillRectangle_DMA(button->x1, button->y1, width, height, button->fill_color);
            break;

        case BUTTON_TYPE_OUTLINED:
            ILI9341_FillRectangle_DMA(button->x1, button->y1, width, height, button->fill_color);
            ILI9341_DrawRect(button->outline_color, button->x1, button->y1, button->x2, button->y2);
            break;

        case BUTTON_TYPE_BITMAP:
            if (button->bitmap) {
                // Предполагаем, что битмап имеет фиксированный размер, например, 320x224
                // Размер кнопки подстраивается под битмап
                ILI9341_DrawImage_DMA(button->x1, button->y1, width, height, button->bitmap);
                button->x2 = button->x1 + width - 1;
                button->y2 = button->y1 + height - 1;
            }
            break;
    }
}
*/
