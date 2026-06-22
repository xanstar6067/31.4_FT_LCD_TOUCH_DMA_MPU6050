#include "button_renderer.h"

#include <string.h>

#include "display_driver.h"

static uint16_t ButtonRenderer_Width(const VirtualButton *button) {
    return (uint16_t)(button->x2 - button->x1 + 1U);
}

static uint16_t ButtonRenderer_Height(const VirtualButton *button) {
    return (uint16_t)(button->y2 - button->y1 + 1U);
}

static void ButtonRenderer_DrawLabel(const VirtualButton *button) {
    uint16_t width;
    uint16_t height;
    uint16_t text_width;
    uint16_t text_height;
    uint16_t text_x;
    uint16_t text_y;
    FontDef *font;

    if ((button->label == NULL) || (button->label[0] == '\0')) {
        return;
    }

    width = ButtonRenderer_Width(button);
    height = ButtonRenderer_Height(button);
    font = &Font_11x18;
    text_width = (uint16_t)(strlen(button->label) * font->width);
    if ((text_width + 6U) > width) {
        font = &Font_7x10;
        text_width =
            (uint16_t)(strlen(button->label) * font->width);
    }
    text_height = font->height;

    text_x = (uint16_t)(button->x1 +
                        ((width > text_width) ?
                         ((width - text_width) / 2U) : 1U));
    text_y = (uint16_t)(button->y1 +
                        ((height > text_height) ?
                         ((height - text_height) / 2U) : 1U));
    DISPLAY_WriteString_DMA(text_x,
                            text_y,
                            button->label,
                            *font,
                            button->text_color,
                            button->pressed ?
                            button->pressed_color :
                            button->fill_color);
}

void ButtonRenderer_Draw(VirtualButton *button) {
    uint16_t width;
    uint16_t height;
    uint16_t fill_color;

    if ((button == NULL) || (button->visible == 0U)) {
        return;
    }

    width = ButtonRenderer_Width(button);
    height = ButtonRenderer_Height(button);
    fill_color =
        button->pressed ? button->pressed_color : button->fill_color;

    switch (button->type) {
        case BUTTON_TYPE_FILLED:
            DISPLAY_FillRectangle_DMA(button->x1,
                                      button->y1,
                                      width,
                                      height,
                                      fill_color);
            break;

        case BUTTON_TYPE_OUTLINED:
            DISPLAY_FillRectangle_DMA(button->x1,
                                      button->y1,
                                      width,
                                      height,
                                      fill_color);
            DISPLAY_DrawRect(button->pressed ?
                             button->text_color :
                             button->outline_color,
                             button->x1,
                             button->y1,
                             button->x2,
                             button->y2);
            break;

        case BUTTON_TYPE_BITMAP:
            if (button->bitmap != NULL) {
                DISPLAY_DrawImage_DMA(button->x1,
                                      button->y1,
                                      width,
                                      height,
                                      button->bitmap);
            }
            break;

        default:
            break;
    }

    ButtonRenderer_DrawLabel(button);
}

void ButtonRenderer_DrawList(VirtualButtonList *list) {
    if (list == NULL) {
        return;
    }

    for (uint16_t i = 0U; i < list->count; i++) {
        ButtonRenderer_Draw(&list->buttons[i]);
    }
}
