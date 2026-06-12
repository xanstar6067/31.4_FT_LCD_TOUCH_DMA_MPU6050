/*
 * virtual_buttons.c
 *
 *  Created on: Mar 19, 2025
 *      Author: Grok3
 */
#include "virtual_buttons.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>

// Инициализация списка кнопок
void VirtualButtonList_Init(VirtualButtonList* list, uint16_t capacity) {
    list->buttons = (VirtualButton*)malloc(capacity * sizeof(VirtualButton));
    list->count = 0;
    list->capacity = capacity;
}

// Добавление кнопки в список
void VirtualButtonList_Add(VirtualButtonList* list, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                           ButtonType type, uint16_t fill_color, uint16_t outline_color, const uint16_t* bitmap) {
    if (list->count >= list->capacity) return; // Нет места

    VirtualButton* btn = &list->buttons[list->count];
    btn->x1 = x1;
    btn->y1 = y1;
    btn->x2 = x2;
    btn->y2 = y2;
    btn->type = type;
    btn->fill_color = fill_color;
    btn->outline_color = outline_color;
    btn->bitmap = bitmap;
    btn->pressed = false;

    list->count++;
}

// Проверка касания с фильтрацией дребезга
bool VirtualButtonList_CheckTouch(VirtualButtonList* list, uint16_t touch_x, uint16_t touch_y) {
    static uint32_t last_touch_time = 0;
    uint32_t current_time = HAL_GetTick();

    // Фильтр дребезга: игнорируем касания чаще, чем раз в 50 мс
    if (current_time - last_touch_time < 50) {
        return false;
    }

    for (uint16_t i = 0; i < list->count; i++) {
        VirtualButton* btn = &list->buttons[i];
        if (touch_x >= btn->x1 && touch_x <= btn->x2 && touch_y >= btn->y1 && touch_y <= btn->y2) {
            btn->pressed = true;
            last_touch_time = current_time;
            return true;
        }
    }
    return false;
}

// Освобождение памяти
void VirtualButtonList_Free(VirtualButtonList* list) {
    free(list->buttons);
    list->buttons = NULL;
    list->count = 0;
    list->capacity = 0;
}

