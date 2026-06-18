#ifndef INC_TOUCH_TEST_PAGE_H_
#define INC_TOUCH_TEST_PAGE_H_

#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint32_t raw_x;
    uint32_t raw_y;
    uint32_t samples;
    uint32_t displayed_samples;
    uint16_t displayed_x;
    uint16_t displayed_y;
    uint32_t displayed_raw_x;
    uint32_t displayed_raw_y;
    int16_t marker_x;
    int16_t marker_y;
    int16_t previous_marker_x;
    int16_t previous_marker_y;
    uint8_t pressed;
    uint8_t displayed_pressed;
    uint8_t text_divider;
    uint8_t renderer_initialized;
} TouchTestPage;

void TouchTestPage_Init(TouchTestPage *page);
void TouchTestPage_Update(TouchTestPage *page);
void TouchTestPage_Render(TouchTestPage *page);

#endif /* INC_TOUCH_TEST_PAGE_H_ */
