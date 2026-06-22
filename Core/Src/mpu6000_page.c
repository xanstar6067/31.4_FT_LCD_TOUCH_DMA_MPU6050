#include "mpu6000_page.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "display_driver.h"

#define MPU_PAGE_BACKGROUND       DISPLAY_COLOR565(3, 8, 14)
#define MPU_PAGE_PANEL            DISPLAY_COLOR565(9, 19, 29)
#define MPU_PAGE_GRID             DISPLAY_COLOR565(25, 49, 66)
#define MPU_PAGE_ACCEL_COLOR      DISPLAY_CYAN
#define MPU_PAGE_GYRO_COLOR       DISPLAY_ORANGE
#define MPU_PAGE_IDENTITY_TICKS   30U
#define MPU_PAGE_TEXT_TICKS       3U
#define MPU_PAGE_FOOTER_TICKS     6U
#define MPU_PAGE_BUBBLE_RADIUS    7
#define MPU_PAGE_BUBBLE_SIZE      15U
#define MPU_PAGE_BAR_WIDTH        78U
#define MPU_PAGE_BAR_HEIGHT       7U

static uint8_t bubble_buffer[MPU_PAGE_BUBBLE_SIZE *
                             MPU_PAGE_BUBBLE_SIZE * 2U];
static uint8_t bar_buffer[MPU_PAGE_BAR_WIDTH *
                          MPU_PAGE_BAR_HEIGHT * 2U];

static int16_t MPU6000Page_Clamp(int32_t value,
                                 int16_t minimum,
                                 int16_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return (int16_t)value;
}

static int16_t MPU6000Page_Scale(int16_t value,
                                 int16_t input_limit,
                                 int16_t output_limit) {
    int32_t scaled = ((int32_t)value * output_limit) / input_limit;

    return MPU6000Page_Clamp(scaled,
                             (int16_t)-output_limit,
                             output_limit);
}

static uint16_t MPU6000Page_TiltStaticPixel(int16_t x, int16_t y) {
    if (((x >= 78) && (x <= 79) && (y >= 76) && (y < 190)) ||
        ((y >= 131) && (y <= 132) && (x >= 14) && (x < 144)) ||
        ((x == 48) && (y >= 80) && (y < 186)) ||
        ((x == 109) && (y >= 80) && (y < 186)) ||
        ((y == 102) && (x >= 18) && (x < 140)) ||
        ((y == 161) && (x >= 18) && (x < 140))) {
        return MPU_PAGE_GRID;
    }
    return MPU_PAGE_PANEL;
}

static void MPU6000Page_DrawBubblePatch(int16_t center_x,
                                        int16_t center_y,
                                        uint8_t draw_bubble,
                                        uint16_t bubble_color) {
    int16_t left = center_x - MPU_PAGE_BUBBLE_RADIUS;
    int16_t top = center_y - MPU_PAGE_BUBBLE_RADIUS;
    uint32_t offset = 0U;

    for (int16_t y = 0; y < (int16_t)MPU_PAGE_BUBBLE_SIZE; y++) {
        for (int16_t x = 0; x < (int16_t)MPU_PAGE_BUBBLE_SIZE; x++) {
            int16_t absolute_x = left + x;
            int16_t absolute_y = top + y;
            int16_t dx = x - MPU_PAGE_BUBBLE_RADIUS;
            int16_t dy = y - MPU_PAGE_BUBBLE_RADIUS;
            uint16_t color =
                MPU6000Page_TiltStaticPixel(absolute_x, absolute_y);

            if ((draw_bubble != 0U) &&
                (((dx * dx) + (dy * dy)) <=
                 (MPU_PAGE_BUBBLE_RADIUS *
                  MPU_PAGE_BUBBLE_RADIUS))) {
                color = bubble_color;
                if (((dx + 2) * (dx + 2) +
                     (dy + 2) * (dy + 2)) <= 2) {
                    color = DISPLAY_WHITE;
                }
            }

            bubble_buffer[offset++] = (uint8_t)(color >> 8);
            bubble_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    DISPLAY_DrawImage_DMA_1D((uint16_t)left,
                             (uint16_t)top,
                             MPU_PAGE_BUBBLE_SIZE,
                             MPU_PAGE_BUBBLE_SIZE,
                             bubble_buffer);
}

static void MPU6000Page_UpdateBubble(MPU6000Page *page) {
    int16_t bubble_x =
        79 + MPU6000Page_Scale(page->data.accel_x, 16384, 55);
    int16_t bubble_y =
        132 + MPU6000Page_Scale(page->data.accel_y, 16384, 48);
    uint8_t color_changed =
        page->displayed_online != page->online;

    if ((bubble_x == page->previous_bubble_x) &&
        (bubble_y == page->previous_bubble_y) &&
        (color_changed == 0U)) {
        return;
    }

    if (page->previous_bubble_x != INT16_MIN) {
        MPU6000Page_DrawBubblePatch(page->previous_bubble_x,
                                    page->previous_bubble_y,
                                    0U,
                                    MPU_PAGE_PANEL);
    }
    MPU6000Page_DrawBubblePatch(
        bubble_x,
        bubble_y,
        1U,
        page->online ? DISPLAY_GREEN : DISPLAY_RED);
    page->previous_bubble_x = bubble_x;
    page->previous_bubble_y = bubble_y;
}

static void MPU6000Page_DrawBar(uint16_t x,
                                uint16_t y,
                                int16_t amount,
                                uint16_t color) {
    int16_t center = MPU_PAGE_BAR_WIDTH / 2;
    uint32_t offset = 0U;

    for (uint16_t row = 0U; row < MPU_PAGE_BAR_HEIGHT; row++) {
        for (int16_t column = 0;
             column < (int16_t)MPU_PAGE_BAR_WIDTH;
             column++) {
            uint16_t pixel = MPU_PAGE_GRID;

            if (column == center) {
                pixel = DISPLAY_GRAY;
            }
            if (((amount > 0) &&
                 (column >= center) &&
                 (column < (center + amount))) ||
                ((amount < 0) &&
                 (column >= (center + amount)) &&
                 (column < center))) {
                pixel = color;
            }

            bar_buffer[offset++] = (uint8_t)(pixel >> 8);
            bar_buffer[offset++] = (uint8_t)(pixel & 0xFFU);
        }
    }

    DISPLAY_DrawImage_DMA_1D(x, y,
                             MPU_PAGE_BAR_WIDTH,
                             MPU_PAGE_BAR_HEIGHT,
                             bar_buffer);
}

static void MPU6000Page_UpdateBars(MPU6000Page *page) {
    const int16_t values[6] = {
        page->data.accel_x,
        page->data.accel_y,
        page->data.accel_z,
        page->data.gyro_x,
        page->data.gyro_y,
        page->data.gyro_z
    };
    const uint16_t positions_y[6] = {
        90U, 105U, 120U, 155U, 170U, 185U
    };

    for (uint8_t index = 0U; index < 6U; index++) {
        int16_t limit = (index < 3U) ? 16384 : 8000;
        int16_t amount =
            MPU6000Page_Scale(values[index], limit,
                              MPU_PAGE_BAR_WIDTH / 2);

        if (amount == page->previous_bars[index]) {
            continue;
        }
        MPU6000Page_DrawBar(
            225U,
            positions_y[index],
            amount,
            (index < 3U) ?
            MPU_PAGE_ACCEL_COLOR : MPU_PAGE_GYRO_COLOR);
        page->previous_bars[index] = amount;
    }
}

static void MPU6000Page_DrawRawValue(uint16_t y,
                                     char axis,
                                     int16_t value) {
    char text[12];

    snprintf(text, sizeof(text), "%c:%6d", axis, (int)value);
    DISPLAY_WriteString_DMA(164, y, text, Font_7x10,
                            DISPLAY_WHITE, MPU_PAGE_PANEL);
}

static void MPU6000Page_UpdateText(MPU6000Page *page,
                                   uint8_t force) {
    if ((force == 0U) &&
        (page->text_divider < MPU_PAGE_TEXT_TICKS)) {
        return;
    }
    page->text_divider = 0U;

    if ((force != 0U) ||
        (page->displayed_data.accel_x != page->data.accel_x)) {
        MPU6000Page_DrawRawValue(88U, 'X', page->data.accel_x);
    }
    if ((force != 0U) ||
        (page->displayed_data.accel_y != page->data.accel_y)) {
        MPU6000Page_DrawRawValue(103U, 'Y', page->data.accel_y);
    }
    if ((force != 0U) ||
        (page->displayed_data.accel_z != page->data.accel_z)) {
        MPU6000Page_DrawRawValue(118U, 'Z', page->data.accel_z);
    }
    if ((force != 0U) ||
        (page->displayed_data.gyro_x != page->data.gyro_x)) {
        MPU6000Page_DrawRawValue(153U, 'X', page->data.gyro_x);
    }
    if ((force != 0U) ||
        (page->displayed_data.gyro_y != page->data.gyro_y)) {
        MPU6000Page_DrawRawValue(168U, 'Y', page->data.gyro_y);
    }
    if ((force != 0U) ||
        (page->displayed_data.gyro_z != page->data.gyro_z)) {
        MPU6000Page_DrawRawValue(183U, 'Z', page->data.gyro_z);
    }
    page->displayed_data = page->data;
}

static void MPU6000Page_UpdateStatus(MPU6000Page *page,
                                     uint8_t force) {
    char text[48];
    uint16_t color = page->online ? DISPLAY_GREEN : DISPLAY_RED;

    if ((force == 0U) &&
        (page->displayed_online == page->online) &&
        (page->displayed_who_am_i == page->who_am_i) &&
        (page->displayed_errors == page->read_errors)) {
        return;
    }

    DISPLAY_FillRectangle_DMA(8, 44, 304, 20, MPU_PAGE_PANEL);
    snprintf(text, sizeof(text),
             "%s %s WHO:%02X ERR:%lu",
             page->online ? "ONLINE" : "OFFLINE",
             MPU6000_GetDeviceNameFromWhoAmI(page->who_am_i),
             page->who_am_i,
             (unsigned long)page->read_errors);
    DISPLAY_WriteString_DMA(13, 49, text, Font_7x10,
                            color, MPU_PAGE_PANEL);
    page->displayed_who_am_i = page->who_am_i;
    page->displayed_errors = page->read_errors;
}

static void MPU6000Page_UpdateFooter(MPU6000Page *page,
                                     uint8_t force) {
    char text[40];
    int32_t temperature_centi =
        MPU6000_TemperatureCenti(&page->data);

    if ((force == 0U) &&
        (page->footer_divider < MPU_PAGE_FOOTER_TICKS)) {
        return;
    }
    page->footer_divider = 0U;

    if ((force == 0U) &&
        (page->displayed_temperature == page->data.temperature) &&
        (page->displayed_reads == page->successful_reads) &&
        (page->displayed_online == page->online)) {
        return;
    }

    DISPLAY_FillRectangle_DMA(8, 203, 304, 12,
                              MPU_PAGE_BACKGROUND);
    if (page->online != 0U) {
        snprintf(text, sizeof(text),
                 "TEMP:%ld.%02ldC  SAMPLES:%lu",
                 (long)(temperature_centi / 100),
                 (long)(temperature_centi < 0 ?
                        -(temperature_centi % 100) :
                        (temperature_centi % 100)),
                 (unsigned long)page->successful_reads);
    } else {
        snprintf(text, sizeof(text),
                 "TEMP:--.--C  SAMPLES:%lu",
                 (unsigned long)page->successful_reads);
    }
    DISPLAY_WriteString_DMA(8, 205, text, Font_7x10,
                            DISPLAY_GRAY,
                            MPU_PAGE_BACKGROUND);
    page->displayed_temperature = page->data.temperature;
    page->displayed_reads = page->successful_reads;
}

static void MPU6000Page_DrawStatic(void) {
    DISPLAY_FillScreen_DMA(MPU_PAGE_BACKGROUND);
    DISPLAY_FillRectangle_DMA(0, 0, 320, 38,
                              DISPLAY_DARK_BLUE);
    DISPLAY_WriteString_DMA(8, 6, "MPU6000 MONITOR",
                            Font_16x26,
                            DISPLAY_WHITE,
                            DISPLAY_DARK_BLUE);
    DISPLAY_WriteString_DMA(270, 14, "PAGE 1",
                            Font_7x10,
                            DISPLAY_CYAN,
                            DISPLAY_DARK_BLUE);

    DISPLAY_FillRectangle_DMA(8, 44, 304, 20, MPU_PAGE_PANEL);
    DISPLAY_FillRectangle_DMA(8, 70, 142, 126, MPU_PAGE_PANEL);
    DISPLAY_FillRectangle_DMA(157, 70, 155, 126, MPU_PAGE_PANEL);
    DISPLAY_FillRectangle_DMA(78, 76, 2, 114, MPU_PAGE_GRID);
    DISPLAY_FillRectangle_DMA(14, 131, 130, 2, MPU_PAGE_GRID);
    DISPLAY_FillRectangle_DMA(48, 80, 1, 106, MPU_PAGE_GRID);
    DISPLAY_FillRectangle_DMA(109, 80, 1, 106, MPU_PAGE_GRID);
    DISPLAY_FillRectangle_DMA(18, 102, 122, 1, MPU_PAGE_GRID);
    DISPLAY_FillRectangle_DMA(18, 161, 122, 1, MPU_PAGE_GRID);

    DISPLAY_WriteString_DMA(164, 74, "ACCEL RAW", Font_7x10,
                            MPU_PAGE_ACCEL_COLOR, MPU_PAGE_PANEL);
    DISPLAY_WriteString_DMA(164, 139, "GYRO RAW", Font_7x10,
                            MPU_PAGE_GYRO_COLOR, MPU_PAGE_PANEL);
    DISPLAY_WriteString_DMA(
        8, 220, "TILT:BUBBLE  BARS:SIGNED RAW  LONG:RETRY",
        Font_7x10, DISPLAY_WHITE, MPU_PAGE_BACKGROUND);
}

void MPU6000Page_Init(MPU6000Page *page) {
    memset(page, 0, sizeof(*page));
    page->who_am_i = MPU6000_Check();
    page->online = MPU6000_IsKnownWhoAmI(page->who_am_i);
    page->previous_bubble_x = INT16_MIN;
    page->previous_bubble_y = INT16_MIN;
    page->displayed_temperature = INT16_MIN;
    for (uint8_t index = 0U; index < 6U; index++) {
        page->previous_bars[index] = INT16_MIN;
    }
    page->displayed_online = 0xFFU;
    page->displayed_who_am_i = 0xFFU;
    page->displayed_errors = UINT32_MAX;
    page->displayed_reads = UINT32_MAX;
}

void MPU6000Page_Update(MPU6000Page *page,
                        const MPU6000_Data_t *data,
                        HAL_StatusTypeDef read_status) {
    if (read_status == HAL_OK) {
        page->data = *data;
        page->successful_reads++;
        page->online = 1U;
    } else {
        page->read_errors++;
        page->online = 0U;
    }

    page->identity_divider++;
    if (page->identity_divider >= MPU_PAGE_IDENTITY_TICKS) {
        page->identity_divider = 0U;
        page->who_am_i = MPU6000_Check();
        page->online =
            (MPU6000_IsKnownWhoAmI(page->who_am_i) != 0U) &&
            (read_status == HAL_OK);
    }

    page->text_divider++;
    page->footer_divider++;
    MPU6000Page_Render(page);
}

void MPU6000Page_Render(MPU6000Page *page) {
    uint8_t force = 0U;

    if (page->renderer_initialized == 0U) {
        MPU6000Page_DrawStatic();
        page->previous_bubble_x = INT16_MIN;
        page->previous_bubble_y = INT16_MIN;
        for (uint8_t index = 0U; index < 6U; index++) {
            page->previous_bars[index] = INT16_MIN;
        }
        page->displayed_online = 0xFFU;
        page->displayed_who_am_i = 0xFFU;
        page->displayed_errors = UINT32_MAX;
        page->displayed_reads = UINT32_MAX;
        page->displayed_temperature = INT16_MIN;
        page->renderer_initialized = 1U;
        force = 1U;
    }

    MPU6000Page_UpdateBubble(page);
    MPU6000Page_UpdateBars(page);
    MPU6000Page_UpdateText(page, force);
    MPU6000Page_UpdateStatus(page, force);
    MPU6000Page_UpdateFooter(page, force);
    page->displayed_online = page->online;
}

