#ifndef INC_SINGLE_BUTTON_H_
#define INC_SINGLE_BUTTON_H_

#include <stdint.h>

typedef enum {
    SINGLE_BUTTON_EVENT_NONE = 0,
    SINGLE_BUTTON_EVENT_SHORT_PRESS,
    SINGLE_BUTTON_EVENT_LONG_PRESS
} SingleButtonEvent;

typedef struct {
    uint32_t candidate_since;
    uint32_t press_started;
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint8_t long_press_sent;
} SingleButton;

void SingleButton_Init(SingleButton *button,
                       uint8_t pressed,
                       uint32_t now);
SingleButtonEvent SingleButton_Update(SingleButton *button,
                                      uint8_t pressed,
                                      uint32_t now);

#endif /* INC_SINGLE_BUTTON_H_ */
