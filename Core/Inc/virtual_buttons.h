#ifndef INC_VIRTUAL_BUTTONS_H_
#define INC_VIRTUAL_BUTTONS_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_TYPE_FILLED = 0,
    BUTTON_TYPE_OUTLINED,
    BUTTON_TYPE_BITMAP
} ButtonType;

typedef struct {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    ButtonType type;
    uint16_t fill_color;
    uint16_t outline_color;
    uint16_t pressed_color;
    uint16_t text_color;
    const uint16_t *bitmap;
    const char *label;
    uint8_t id;
    uint8_t enabled;
    uint8_t visible;
    bool pressed;
    bool just_pressed;
    bool just_released;
    uint16_t hold_ticks;
} VirtualButton;

typedef struct {
    VirtualButton *buttons;
    uint16_t count;
    uint16_t capacity;
    uint8_t owns_storage;
    uint8_t active_id;
} VirtualButtonList;

void VirtualButtonList_Init(VirtualButtonList *list, uint16_t capacity);
void VirtualButtonList_InitWithStorage(VirtualButtonList *list,
                                       VirtualButton *storage,
                                       uint16_t capacity);
void VirtualButtonList_Clear(VirtualButtonList *list);
void VirtualButtonList_Add(VirtualButtonList *list,
                           uint16_t x1,
                           uint16_t y1,
                           uint16_t x2,
                           uint16_t y2,
                           ButtonType type,
                           uint16_t fill_color,
                           uint16_t outline_color,
                           const uint16_t *bitmap);
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
                                       const uint16_t *bitmap);
bool VirtualButtonList_UpdateTouch(VirtualButtonList *list,
                                   uint8_t touch_pressed,
                                   uint16_t touch_x,
                                   uint16_t touch_y);
bool VirtualButtonList_CheckTouch(VirtualButtonList *list,
                                  uint16_t touch_x,
                                  uint16_t touch_y);
VirtualButton *VirtualButtonList_Find(VirtualButtonList *list,
                                      uint8_t id);
const VirtualButton *VirtualButtonList_FindConst(
    const VirtualButtonList *list,
    uint8_t id);
bool VirtualButtonList_WasPressed(const VirtualButtonList *list,
                                  uint8_t id);
bool VirtualButtonList_WasReleased(const VirtualButtonList *list,
                                   uint8_t id);
bool VirtualButtonList_IsDown(const VirtualButtonList *list,
                              uint8_t id);
bool VirtualButtonList_ShouldFire(const VirtualButtonList *list,
                                  uint8_t id,
                                  uint16_t first_repeat_ticks,
                                  uint16_t repeat_ticks);
void VirtualButtonList_SetEnabled(VirtualButtonList *list,
                                  uint8_t id,
                                  uint8_t enabled);
void VirtualButtonList_SetVisible(VirtualButtonList *list,
                                  uint8_t id,
                                  uint8_t visible);
void VirtualButtonList_Free(VirtualButtonList *list);

#endif /* INC_VIRTUAL_BUTTONS_H_ */
