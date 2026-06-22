#include "virtual_buttons.h"

#include <stdlib.h>
#include <string.h>

static uint8_t VirtualButton_HitTest(const VirtualButton *button,
                                     uint16_t touch_x,
                                     uint16_t touch_y) {
    return ((button->enabled != 0U) &&
            (button->visible != 0U) &&
            (touch_x >= button->x1) &&
            (touch_x <= button->x2) &&
            (touch_y >= button->y1) &&
            (touch_y <= button->y2)) ? 1U : 0U;
}

static void VirtualButton_ResetTransient(VirtualButton *button) {
    button->just_pressed = false;
    button->just_released = false;
}

void VirtualButtonList_Init(VirtualButtonList *list,
                            uint16_t capacity) {
    list->buttons =
        (VirtualButton *)malloc(capacity * sizeof(VirtualButton));
    list->count = 0U;
    list->capacity = (list->buttons != NULL) ? capacity : 0U;
    list->owns_storage = (list->buttons != NULL) ? 1U : 0U;
    list->active_id = 0U;
    if (list->buttons != NULL) {
        memset(list->buttons, 0, capacity * sizeof(VirtualButton));
    }
}

void VirtualButtonList_InitWithStorage(VirtualButtonList *list,
                                       VirtualButton *storage,
                                       uint16_t capacity) {
    list->buttons = storage;
    list->count = 0U;
    list->capacity = capacity;
    list->owns_storage = 0U;
    list->active_id = 0U;
    if (storage != NULL) {
        memset(storage, 0, capacity * sizeof(VirtualButton));
    }
}

void VirtualButtonList_Clear(VirtualButtonList *list) {
    if (list->buttons != NULL) {
        memset(list->buttons, 0,
               list->capacity * sizeof(VirtualButton));
    }
    list->count = 0U;
    list->active_id = 0U;
}

VirtualButton *VirtualButtonList_AddEx(VirtualButtonList *list,
                                       uint8_t id,
                                       const char *label,
                                       uint16_t x1,
                                       uint16_t y1,
                                       uint16_t x2,
                                       uint16_t y2,
                                       ButtonType type,
                                       uint16_t fill_color,
                                       uint16_t outline_color,
                                       uint16_t pressed_color,
                                       uint16_t text_color,
                                       const uint16_t *bitmap) {
    VirtualButton *button;

    if ((list->buttons == NULL) ||
        (list->count >= list->capacity)) {
        return NULL;
    }

    button = &list->buttons[list->count++];
    memset(button, 0, sizeof(*button));
    button->x1 = x1;
    button->y1 = y1;
    button->x2 = x2;
    button->y2 = y2;
    button->type = type;
    button->fill_color = fill_color;
    button->outline_color = outline_color;
    button->pressed_color = pressed_color;
    button->text_color = text_color;
    button->bitmap = bitmap;
    button->label = label;
    button->id = id;
    button->enabled = 1U;
    button->visible = 1U;
    return button;
}

void VirtualButtonList_Add(VirtualButtonList *list,
                           uint16_t x1,
                           uint16_t y1,
                           uint16_t x2,
                           uint16_t y2,
                           ButtonType type,
                           uint16_t fill_color,
                           uint16_t outline_color,
                           const uint16_t *bitmap) {
    (void)VirtualButtonList_AddEx(
        list,
        (uint8_t)(list->count + 1U),
        NULL,
        x1,
        y1,
        x2,
        y2,
        type,
        fill_color,
        outline_color,
        fill_color,
        outline_color,
        bitmap);
}

bool VirtualButtonList_UpdateTouch(VirtualButtonList *list,
                                   uint8_t touch_pressed,
                                   uint16_t touch_x,
                                   uint16_t touch_y) {
    uint8_t next_active_id = 0U;
    bool changed = false;

    for (uint16_t i = 0U; i < list->count; i++) {
        VirtualButton_ResetTransient(&list->buttons[i]);
    }

    if (touch_pressed != 0U) {
        for (uint16_t i = 0U; i < list->count; i++) {
            VirtualButton *button = &list->buttons[i];

            if (VirtualButton_HitTest(button, touch_x, touch_y) != 0U) {
                next_active_id = button->id;
                break;
            }
        }
    }

    for (uint16_t i = 0U; i < list->count; i++) {
        VirtualButton *button = &list->buttons[i];
        bool should_press = (button->id == next_active_id);

        if (should_press) {
            if (!button->pressed) {
                button->just_pressed = true;
                button->hold_ticks = 0U;
                changed = true;
            } else if (button->hold_ticks < 0xFFFFU) {
                button->hold_ticks++;
            }
            button->pressed = true;
        } else {
            if (button->pressed) {
                button->just_released = true;
                changed = true;
            }
            button->pressed = false;
            button->hold_ticks = 0U;
        }
    }

    if (list->active_id != next_active_id) {
        changed = true;
    }
    list->active_id = next_active_id;
    return changed;
}

bool VirtualButtonList_CheckTouch(VirtualButtonList *list,
                                  uint16_t touch_x,
                                  uint16_t touch_y) {
    (void)VirtualButtonList_UpdateTouch(list, 1U, touch_x, touch_y);
    return (list->active_id != 0U);
}

VirtualButton *VirtualButtonList_Find(VirtualButtonList *list,
                                      uint8_t id) {
    for (uint16_t i = 0U; i < list->count; i++) {
        if (list->buttons[i].id == id) {
            return &list->buttons[i];
        }
    }
    return NULL;
}

const VirtualButton *VirtualButtonList_FindConst(
    const VirtualButtonList *list,
    uint8_t id) {
    for (uint16_t i = 0U; i < list->count; i++) {
        if (list->buttons[i].id == id) {
            return &list->buttons[i];
        }
    }
    return NULL;
}

bool VirtualButtonList_WasPressed(const VirtualButtonList *list,
                                  uint8_t id) {
    const VirtualButton *button =
        VirtualButtonList_FindConst(list, id);

    return (button != NULL) ? button->just_pressed : false;
}

bool VirtualButtonList_WasReleased(const VirtualButtonList *list,
                                   uint8_t id) {
    const VirtualButton *button =
        VirtualButtonList_FindConst(list, id);

    return (button != NULL) ? button->just_released : false;
}

bool VirtualButtonList_IsDown(const VirtualButtonList *list,
                              uint8_t id) {
    const VirtualButton *button =
        VirtualButtonList_FindConst(list, id);

    return (button != NULL) ? button->pressed : false;
}

bool VirtualButtonList_ShouldFire(const VirtualButtonList *list,
                                  uint8_t id,
                                  uint16_t first_repeat_ticks,
                                  uint16_t repeat_ticks) {
    const VirtualButton *button =
        VirtualButtonList_FindConst(list, id);
    uint16_t repeat_age;

    if ((button == NULL) || (!button->pressed)) {
        return false;
    }
    if (button->just_pressed) {
        return true;
    }
    if ((repeat_ticks == 0U) ||
        (button->hold_ticks < first_repeat_ticks)) {
        return false;
    }

    repeat_age =
        (uint16_t)(button->hold_ticks - first_repeat_ticks);
    return ((repeat_age % repeat_ticks) == 0U);
}

void VirtualButtonList_SetEnabled(VirtualButtonList *list,
                                  uint8_t id,
                                  uint8_t enabled) {
    VirtualButton *button = VirtualButtonList_Find(list, id);

    if (button != NULL) {
        button->enabled = (enabled != 0U) ? 1U : 0U;
        if (button->enabled == 0U) {
            button->pressed = false;
            button->just_pressed = false;
            button->just_released = false;
            button->hold_ticks = 0U;
        }
    }
}

void VirtualButtonList_SetVisible(VirtualButtonList *list,
                                  uint8_t id,
                                  uint8_t visible) {
    VirtualButton *button = VirtualButtonList_Find(list, id);

    if (button != NULL) {
        button->visible = (visible != 0U) ? 1U : 0U;
    }
}

void VirtualButtonList_Free(VirtualButtonList *list) {
    if ((list->owns_storage != 0U) &&
        (list->buttons != NULL)) {
        free(list->buttons);
    }
    list->buttons = NULL;
    list->count = 0U;
    list->capacity = 0U;
    list->owns_storage = 0U;
    list->active_id = 0U;
}
