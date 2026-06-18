#include "touch_test_page.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ili9341.h"
#include "ili9341_touch.h"

#define TOUCH_TEST_BACKGROUND       ILI9341_COLOR565(5, 8, 14)
#define TOUCH_TEST_PANEL            ILI9341_COLOR565(12, 20, 29)
#define TOUCH_TEST_FIELD            ILI9341_COLOR565(6, 14, 20)
#define TOUCH_TEST_GRID             ILI9341_COLOR565(25, 54, 66)
#define TOUCH_TEST_BORDER           ILI9341_COLOR565(55, 92, 112)
#define TOUCH_TEST_TARGET           ILI9341_ORANGE
#define TOUCH_TEST_MARKER           ILI9341_GREEN
#define TOUCH_TEST_FIELD_X          8U
#define TOUCH_TEST_FIELD_Y          84U
#define TOUCH_TEST_FIELD_W          216U
#define TOUCH_TEST_FIELD_H          136U
#define TOUCH_TEST_SIDE_X           232U
#define TOUCH_TEST_SIDE_Y           84U
#define TOUCH_TEST_SIDE_W           80U
#define TOUCH_TEST_SIDE_H           136U
#define TOUCH_TEST_PATCH_SIZE       25U
#define TOUCH_TEST_PATCH_HALF       12
#define TOUCH_TEST_TEXT_TICKS       2U

static uint8_t touch_patch_buffer[TOUCH_TEST_PATCH_SIZE *
                                  TOUCH_TEST_PATCH_SIZE * 2U];

static const char *TouchTestPage_OrientationName(void) {
    switch (ili9341_orientation) {
        case ILI9341_ORIENTATION_PORTRAIT:
            return "PORTRAIT";
        case ILI9341_ORIENTATION_PORTRAIT_UPSIDE:
            return "PORTRAIT_UP";
        case ILI9341_ORIENTATION_LANDSCAPE:
            return "LANDSCAPE";
        case ILI9341_ORIENTATION_LANDSCAPE_LEFT:
            return "LANDSCAPE_LEFT";
        default:
            return "UNKNOWN";
    }
}

static uint16_t TouchTestPage_FieldStaticPixel(uint16_t x,
                                               uint16_t y) {
    const uint16_t right = TOUCH_TEST_FIELD_X + TOUCH_TEST_FIELD_W - 1U;
    const uint16_t bottom = TOUCH_TEST_FIELD_Y + TOUCH_TEST_FIELD_H - 1U;
    const uint16_t mid_x = TOUCH_TEST_FIELD_X + (TOUCH_TEST_FIELD_W / 2U);
    const uint16_t mid_y = TOUCH_TEST_FIELD_Y + (TOUCH_TEST_FIELD_H / 2U);
    const uint16_t q1_x = TOUCH_TEST_FIELD_X + (TOUCH_TEST_FIELD_W / 4U);
    const uint16_t q3_x = TOUCH_TEST_FIELD_X + ((TOUCH_TEST_FIELD_W * 3U) / 4U);
    const uint16_t q1_y = TOUCH_TEST_FIELD_Y + (TOUCH_TEST_FIELD_H / 4U);
    const uint16_t q3_y = TOUCH_TEST_FIELD_Y + ((TOUCH_TEST_FIELD_H * 3U) / 4U);
    const uint8_t near_left = (x <= (TOUCH_TEST_FIELD_X + 10U));
    const uint8_t near_right = (x >= (right - 10U));
    const uint8_t near_top = (y <= (TOUCH_TEST_FIELD_Y + 10U));
    const uint8_t near_bottom = (y >= (bottom - 10U));

    if ((x == TOUCH_TEST_FIELD_X) || (x == right) ||
        (y == TOUCH_TEST_FIELD_Y) || (y == bottom)) {
        return TOUCH_TEST_BORDER;
    }
    if (((near_left != 0U) || (near_right != 0U)) &&
        ((near_top != 0U) || (near_bottom != 0U))) {
        return TOUCH_TEST_TARGET;
    }
    if ((x == mid_x) || (y == mid_y) ||
        (x == q1_x) || (x == q3_x) ||
        (y == q1_y) || (y == q3_y)) {
        return TOUCH_TEST_GRID;
    }
    return TOUCH_TEST_FIELD;
}

static uint16_t TouchTestPage_MarkerPixel(uint16_t x,
                                          uint16_t y,
                                          uint16_t center_x,
                                          uint16_t center_y,
                                          uint8_t draw_marker) {
    int16_t dx;
    int16_t dy;
    uint16_t color = TouchTestPage_FieldStaticPixel(x, y);

    if (draw_marker == 0U) {
        return color;
    }

    dx = (int16_t)x - (int16_t)center_x;
    dy = (int16_t)y - (int16_t)center_y;
    if (dx < 0) {
        dx = (int16_t)-dx;
    }
    if (dy < 0) {
        dy = (int16_t)-dy;
    }

    if (((dx <= 9) && (dy <= 1)) ||
        ((dy <= 9) && (dx <= 1))) {
        color = TOUCH_TEST_MARKER;
    }
    if (((dx * dx) + (dy * dy)) <= 9) {
        color = ILI9341_WHITE;
    }
    return color;
}

static void TouchTestPage_DrawMarkerPatch(uint16_t center_x,
                                          uint16_t center_y,
                                          uint8_t draw_marker) {
    int16_t left = (int16_t)center_x - TOUCH_TEST_PATCH_HALF;
    int16_t top = (int16_t)center_y - TOUCH_TEST_PATCH_HALF;
    const int16_t min_left = (int16_t)TOUCH_TEST_FIELD_X;
    const int16_t min_top = (int16_t)TOUCH_TEST_FIELD_Y;
    const int16_t max_left =
        (int16_t)(TOUCH_TEST_FIELD_X + TOUCH_TEST_FIELD_W -
                  TOUCH_TEST_PATCH_SIZE);
    const int16_t max_top =
        (int16_t)(TOUCH_TEST_FIELD_Y + TOUCH_TEST_FIELD_H -
                  TOUCH_TEST_PATCH_SIZE);
    uint32_t offset = 0U;

    if (left < min_left) {
        left = min_left;
    }
    if (top < min_top) {
        top = min_top;
    }
    if (left > max_left) {
        left = max_left;
    }
    if (top > max_top) {
        top = max_top;
    }

    for (uint16_t row = 0U; row < TOUCH_TEST_PATCH_SIZE; row++) {
        for (uint16_t column = 0U;
             column < TOUCH_TEST_PATCH_SIZE;
             column++) {
            uint16_t x = (uint16_t)left + column;
            uint16_t y = (uint16_t)top + row;
            uint16_t color =
                TouchTestPage_MarkerPixel(x, y,
                                          center_x, center_y,
                                          draw_marker);

            touch_patch_buffer[offset++] = (uint8_t)(color >> 8);
            touch_patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    ILI9341_DrawImage_DMA_1D((uint16_t)left,
                             (uint16_t)top,
                             TOUCH_TEST_PATCH_SIZE,
                             TOUCH_TEST_PATCH_SIZE,
                             touch_patch_buffer);
}

static uint16_t TouchTestPage_MapToFieldX(uint16_t x) {
    uint32_t max_x = (ILI9341_TOUCH_SCALE_X > 1U) ?
                     (ILI9341_TOUCH_SCALE_X - 1U) : 1U;

    if (x >= ILI9341_TOUCH_SCALE_X) {
        x = (uint16_t)max_x;
    }
    return (uint16_t)(TOUCH_TEST_FIELD_X +
        (((uint32_t)x * (TOUCH_TEST_FIELD_W - 1U)) / max_x));
}

static uint16_t TouchTestPage_MapToFieldY(uint16_t y) {
    uint32_t max_y = (ILI9341_TOUCH_SCALE_Y > 1U) ?
                     (ILI9341_TOUCH_SCALE_Y - 1U) : 1U;

    if (y >= ILI9341_TOUCH_SCALE_Y) {
        y = (uint16_t)max_y;
    }
    return (uint16_t)(TOUCH_TEST_FIELD_Y +
        (((uint32_t)y * (TOUCH_TEST_FIELD_H - 1U)) / max_y));
}

static void TouchTestPage_DrawFieldStatic(void) {
    const uint16_t right = TOUCH_TEST_FIELD_X + TOUCH_TEST_FIELD_W - 1U;
    const uint16_t bottom = TOUCH_TEST_FIELD_Y + TOUCH_TEST_FIELD_H - 1U;
    const uint16_t mid_x = TOUCH_TEST_FIELD_X + (TOUCH_TEST_FIELD_W / 2U);
    const uint16_t mid_y = TOUCH_TEST_FIELD_Y + (TOUCH_TEST_FIELD_H / 2U);
    const uint16_t q1_x = TOUCH_TEST_FIELD_X + (TOUCH_TEST_FIELD_W / 4U);
    const uint16_t q3_x = TOUCH_TEST_FIELD_X + ((TOUCH_TEST_FIELD_W * 3U) / 4U);
    const uint16_t q1_y = TOUCH_TEST_FIELD_Y + (TOUCH_TEST_FIELD_H / 4U);
    const uint16_t q3_y = TOUCH_TEST_FIELD_Y + ((TOUCH_TEST_FIELD_H * 3U) / 4U);

    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              TOUCH_TEST_FIELD_Y,
                              TOUCH_TEST_FIELD_W,
                              TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_FIELD);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              TOUCH_TEST_FIELD_Y,
                              TOUCH_TEST_FIELD_W,
                              1U,
                              TOUCH_TEST_BORDER);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              bottom,
                              TOUCH_TEST_FIELD_W,
                              1U,
                              TOUCH_TEST_BORDER);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              TOUCH_TEST_FIELD_Y,
                              1U,
                              TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_BORDER);
    ILI9341_FillRectangle_DMA(right,
                              TOUCH_TEST_FIELD_Y,
                              1U,
                              TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_BORDER);
    ILI9341_FillRectangle_DMA(q1_x, TOUCH_TEST_FIELD_Y,
                              1U, TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_GRID);
    ILI9341_FillRectangle_DMA(mid_x, TOUCH_TEST_FIELD_Y,
                              1U, TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_GRID);
    ILI9341_FillRectangle_DMA(q3_x, TOUCH_TEST_FIELD_Y,
                              1U, TOUCH_TEST_FIELD_H,
                              TOUCH_TEST_GRID);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X, q1_y,
                              TOUCH_TEST_FIELD_W, 1U,
                              TOUCH_TEST_GRID);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X, mid_y,
                              TOUCH_TEST_FIELD_W, 1U,
                              TOUCH_TEST_GRID);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X, q3_y,
                              TOUCH_TEST_FIELD_W, 1U,
                              TOUCH_TEST_GRID);

    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              TOUCH_TEST_FIELD_Y,
                              11U, 11U,
                              TOUCH_TEST_TARGET);
    ILI9341_FillRectangle_DMA(right - 10U,
                              TOUCH_TEST_FIELD_Y,
                              11U, 11U,
                              TOUCH_TEST_TARGET);
    ILI9341_FillRectangle_DMA(TOUCH_TEST_FIELD_X,
                              bottom - 10U,
                              11U, 11U,
                              TOUCH_TEST_TARGET);
    ILI9341_FillRectangle_DMA(right - 10U,
                              bottom - 10U,
                              11U, 11U,
                              TOUCH_TEST_TARGET);

    ILI9341_WriteString_DMA(16, 110, "TL", Font_7x10,
                            TOUCH_TEST_TARGET, TOUCH_TEST_FIELD);
    ILI9341_WriteString_DMA(199, 110, "TR", Font_7x10,
                            TOUCH_TEST_TARGET, TOUCH_TEST_FIELD);
    ILI9341_WriteString_DMA(16, 192, "BL", Font_7x10,
                            TOUCH_TEST_TARGET, TOUCH_TEST_FIELD);
    ILI9341_WriteString_DMA(199, 192, "BR", Font_7x10,
                            TOUCH_TEST_TARGET, TOUCH_TEST_FIELD);
}

static void TouchTestPage_DrawStatic(void) {
    char text[48];

    ILI9341_FillScreen_DMA(TOUCH_TEST_BACKGROUND);
    ILI9341_FillRectangle_DMA(0, 0, 320, 38,
                              ILI9341_DARK_BLUE);
    ILI9341_WriteString_DMA(8, 6, "TOUCH TEST",
                            Font_16x26,
                            ILI9341_WHITE,
                            ILI9341_DARK_BLUE);
    ILI9341_WriteString_DMA(270, 14, "PAGE 2",
                            Font_7x10,
                            ILI9341_CYAN,
                            ILI9341_DARK_BLUE);

    ILI9341_FillRectangle_DMA(8, 44, 304, 32,
                              TOUCH_TEST_PANEL);
    ILI9341_WriteString_DMA(14, 49,
                            "CTRL:XPT2046/ADS7846  SPI2 PB9/PB1",
                            Font_7x10,
                            ILI9341_CYAN,
                            TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text),
             "ORI:%s  LCD:%ux%u",
             TouchTestPage_OrientationName(),
             (unsigned int)ILI9341_SCREEN_WIDTH,
             (unsigned int)ILI9341_SCREEN_HEIGHT);
    ILI9341_WriteString_DMA(14, 63, text,
                            Font_7x10,
                            ILI9341_WHITE,
                            TOUCH_TEST_PANEL);

    TouchTestPage_DrawFieldStatic();

    ILI9341_FillRectangle_DMA(TOUCH_TEST_SIDE_X,
                              TOUCH_TEST_SIDE_Y,
                              TOUCH_TEST_SIDE_W,
                              TOUCH_TEST_SIDE_H,
                              TOUCH_TEST_PANEL);
    ILI9341_WriteString_DMA(240, 92, "LIVE",
                            Font_7x10,
                            ILI9341_YELLOW,
                            TOUCH_TEST_PANEL);

    ILI9341_WriteString_DMA(
        8, 224, "CORNERS: MARKER FOLLOWS FINGER  LONG:CLEAR",
        Font_7x10, ILI9341_WHITE, TOUCH_TEST_BACKGROUND);
}

static void TouchTestPage_UpdateMarker(TouchTestPage *page) {
    if ((page->pressed != 0U) &&
        (page->previous_marker_x == page->marker_x) &&
        (page->previous_marker_y == page->marker_y)) {
        return;
    }

    if (page->previous_marker_x != INT16_MIN) {
        TouchTestPage_DrawMarkerPatch(
            (uint16_t)page->previous_marker_x,
            (uint16_t)page->previous_marker_y,
            0U);
        page->previous_marker_x = INT16_MIN;
        page->previous_marker_y = INT16_MIN;
    }

    if (page->pressed == 0U) {
        return;
    }

    TouchTestPage_DrawMarkerPatch((uint16_t)page->marker_x,
                                  (uint16_t)page->marker_y,
                                  1U);
    page->previous_marker_x = page->marker_x;
    page->previous_marker_y = page->marker_y;
}

static void TouchTestPage_UpdateText(TouchTestPage *page,
                                     uint8_t force) {
    char text[16];
    uint16_t status_color =
        (page->pressed != 0U) ? ILI9341_GREEN : ILI9341_GRAY;

    if ((force == 0U) &&
        (page->text_divider < TOUCH_TEST_TEXT_TICKS) &&
        (page->displayed_pressed == page->pressed) &&
        (page->displayed_x == page->x) &&
        (page->displayed_y == page->y) &&
        (page->displayed_raw_x == page->raw_x) &&
        (page->displayed_raw_y == page->raw_y) &&
        (page->displayed_samples == page->samples)) {
        return;
    }
    page->text_divider = 0U;

    ILI9341_FillRectangle_DMA(238, 108, 66, 104,
                              TOUCH_TEST_PANEL);
    ILI9341_WriteString_DMA(240, 108,
                            (page->pressed != 0U) ?
                            "PRESSED" : "RELEASE",
                            Font_7x10,
                            status_color,
                            TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text), "X:%3u",
             (unsigned int)page->x);
    ILI9341_WriteString_DMA(240, 126, text, Font_7x10,
                            ILI9341_WHITE, TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text), "Y:%3u",
             (unsigned int)page->y);
    ILI9341_WriteString_DMA(240, 140, text, Font_7x10,
                            ILI9341_WHITE, TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text), "RX:%5lu",
             (unsigned long)page->raw_x);
    ILI9341_WriteString_DMA(240, 158, text, Font_7x10,
                            ILI9341_CYAN, TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text), "RY:%5lu",
             (unsigned long)page->raw_y);
    ILI9341_WriteString_DMA(240, 172, text, Font_7x10,
                            ILI9341_CYAN, TOUCH_TEST_PANEL);
    snprintf(text, sizeof(text), "S:%5lu",
             (unsigned long)page->samples);
    ILI9341_WriteString_DMA(240, 194, text, Font_7x10,
                            ILI9341_GRAY, TOUCH_TEST_PANEL);

    page->displayed_pressed = page->pressed;
    page->displayed_x = page->x;
    page->displayed_y = page->y;
    page->displayed_raw_x = page->raw_x;
    page->displayed_raw_y = page->raw_y;
    page->displayed_samples = page->samples;
}

void TouchTestPage_Init(TouchTestPage *page) {
    memset(page, 0, sizeof(*page));
    page->previous_marker_x = INT16_MIN;
    page->previous_marker_y = INT16_MIN;
    page->displayed_pressed = 0xFFU;
    page->displayed_x = UINT16_MAX;
    page->displayed_y = UINT16_MAX;
    page->displayed_raw_x = UINT32_MAX;
    page->displayed_raw_y = UINT32_MAX;
    page->displayed_samples = UINT32_MAX;
}

void TouchTestPage_Update(TouchTestPage *page) {
    uint16_t x = page->x;
    uint16_t y = page->y;

    page->pressed = ILI9341_TouchPressed() ? 1U : 0U;
    if (page->pressed != 0U) {
        if (ILI9341_TouchGetCoordinates(&x, &y)) {
            page->x = x;
            page->y = y;
            page->raw_x = raw_x;
            page->raw_y = raw_y;
            page->samples++;
            page->marker_x =
                (int16_t)TouchTestPage_MapToFieldX(page->x);
            page->marker_y =
                (int16_t)TouchTestPage_MapToFieldY(page->y);
        }
    }

    page->text_divider++;
    TouchTestPage_Render(page);
}

void TouchTestPage_Render(TouchTestPage *page) {
    uint8_t force = 0U;

    if (page->renderer_initialized == 0U) {
        TouchTestPage_DrawStatic();
        page->previous_marker_x = INT16_MIN;
        page->previous_marker_y = INT16_MIN;
        page->displayed_pressed = 0xFFU;
        page->displayed_x = UINT16_MAX;
        page->displayed_y = UINT16_MAX;
        page->displayed_raw_x = UINT32_MAX;
        page->displayed_raw_y = UINT32_MAX;
        page->displayed_samples = UINT32_MAX;
        page->renderer_initialized = 1U;
        force = 1U;
    }

    TouchTestPage_UpdateMarker(page);
    TouchTestPage_UpdateText(page, force);
}
