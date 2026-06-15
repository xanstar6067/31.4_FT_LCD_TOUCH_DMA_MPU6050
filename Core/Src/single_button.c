#include "single_button.h"

#define SINGLE_BUTTON_DEBOUNCE_MS    30U
#define SINGLE_BUTTON_LONG_PRESS_MS  900U

void SingleButton_Init(SingleButton *button,
                       uint8_t pressed,
                       uint32_t now) {
    button->candidate_since = now;
    button->press_started = now;
    button->stable_pressed = pressed;
    button->candidate_pressed = pressed;
    button->long_press_sent = 0U;
}

SingleButtonEvent SingleButton_Update(SingleButton *button,
                                      uint8_t pressed,
                                      uint32_t now) {
    if (pressed != button->candidate_pressed) {
        button->candidate_pressed = pressed;
        button->candidate_since = now;
    }

    if ((button->candidate_pressed != button->stable_pressed) &&
        ((uint32_t)(now - button->candidate_since) >=
         SINGLE_BUTTON_DEBOUNCE_MS)) {
        button->stable_pressed = button->candidate_pressed;

        if (button->stable_pressed != 0U) {
            button->press_started = now;
            button->long_press_sent = 0U;
        } else if (button->long_press_sent == 0U) {
            return SINGLE_BUTTON_EVENT_SHORT_PRESS;
        }
    }

    if ((button->stable_pressed != 0U) &&
        (button->long_press_sent == 0U) &&
        ((uint32_t)(now - button->press_started) >=
         SINGLE_BUTTON_LONG_PRESS_MS)) {
        button->long_press_sent = 1U;
        return SINGLE_BUTTON_EVENT_LONG_PRESS;
    }

    return SINGLE_BUTTON_EVENT_NONE;
}
