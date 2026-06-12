/*
 * virtual_buttons.h
 *
 *  Created on: Mar 19, 2025
 *      Author: Grok3
 */

#ifndef INC_VIRTUAL_BUTTONS_H_
#define INC_VIRTUAL_BUTTONS_H_

#include <stdint.h>
#include <stdbool.h>

// Типы кнопок
typedef enum {
    BUTTON_TYPE_FILLED,    // Сплошная заливка
    BUTTON_TYPE_OUTLINED,  // С контуром
    BUTTON_TYPE_BITMAP     // С битмапом
} ButtonType;

// Структура для виртуальной кнопки
typedef struct {
    uint16_t x1, y1;       // Левый верхний угол
    uint16_t x2, y2;       // Правый нижний угол
    ButtonType type;       // Тип кнопки
    uint16_t fill_color;   // Цвет заливки (для FILLED и OUTLINED)
    uint16_t outline_color;// Цвет контура (для OUTLINED)
    const uint16_t* bitmap;// Указатель на битмап (для BITMAP)
    bool pressed;          // Флаг нажатия
} VirtualButton;

// Структура для списка кнопок
typedef struct {
    VirtualButton* buttons; // Массив кнопок
    uint16_t count;         // Текущее количество кнопок
    uint16_t capacity;      // Максимальная вместимость
} VirtualButtonList;

// Функции управления кнопками
void VirtualButtonList_Init(VirtualButtonList* list, uint16_t capacity);
void VirtualButtonList_Add(VirtualButtonList* list, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                           ButtonType type, uint16_t fill_color, uint16_t outline_color, const uint16_t* bitmap);
bool VirtualButtonList_CheckTouch(VirtualButtonList* list, uint16_t touch_x, uint16_t touch_y);
void VirtualButtonList_Free(VirtualButtonList* list);

#endif /* INC_VIRTUAL_BUTTONS_H_ */
