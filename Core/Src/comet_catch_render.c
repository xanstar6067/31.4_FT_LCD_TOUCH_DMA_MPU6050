#include "comet_catch_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define COMET_RENDER_PATCH_WIDTH       160
#define COMET_RENDER_PATCH_HEIGHT       48
#define COMET_STAR_CELL_WIDTH           24
#define COMET_STAR_CELL_HEIGHT          22
#define COMET_STAR_COLUMNS              14U
#define COMET_STAR_ROWS                 10U
#define COMET_CATCHER_Y                220

#define COMET_BACKGROUND_COLOR          DISPLAY_COLOR565(2, 4, 12)
#define COMET_STAR_DIM_COLOR            DISPLAY_COLOR565(65, 78, 118)
#define COMET_STAR_BRIGHT_COLOR         DISPLAY_COLOR565(165, 210, 245)
#define COMET_GOOD_COLOR                DISPLAY_COLOR565(250, 207, 64)
#define COMET_GOOD_EDGE_COLOR           DISPLAY_COLOR565(255, 246, 170)
#define COMET_SPARK_COLOR               DISPLAY_COLOR565(235, 68, 64)
#define COMET_SPARK_CORE_COLOR          DISPLAY_COLOR565(255, 170, 66)
#define COMET_NET_COLOR                 DISPLAY_COLOR565(82, 220, 235)
#define COMET_NET_EDGE_COLOR            DISPLAY_WHITE
#define COMET_CATCHER_COLOR             DISPLAY_COLOR565(54, 170, 220)
#define COMET_CATCHER_DARK_COLOR        DISPLAY_COLOR565(12, 55, 82)
#define COMET_CATCHER_HIT_COLOR         DISPLAY_ORANGE
#define COMET_HEADER_COLOR              DISPLAY_BLACK
#define COMET_BAR_BACK_COLOR            DISPLAY_COLOR565(16, 24, 34)

#if (COMET_RENDER_PATCH_WIDTH * COMET_RENDER_PATCH_HEIGHT * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for comet catch patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} CometRenderRect;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t radius;
    uint8_t type;
    uint8_t active;
} CometObjectSnapshot;

#define comet_patch_buffer render_scratch_buffer
static CometObjectSnapshot previous_objects[COMET_CATCH_MAX_OBJECTS];
static int16_t previous_catcher_x;
static uint8_t previous_pulse_active;
static uint8_t previous_catcher_flash;
static uint8_t comet_renderer_initialized;
static uint32_t displayed_score;
static uint8_t displayed_level;
static uint8_t displayed_combo;
static uint8_t displayed_lives;
static uint8_t displayed_energy;

static int16_t CometRender_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static int16_t CometRender_Abs16(int16_t value) {
    return (value < 0) ? (int16_t)-value : value;
}

static uint32_t CometRender_Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static uint8_t CometRender_CatcherFlash(
    const CometCatchState *game) {
    if ((game->phase == COMET_CATCH_PHASE_PLAYING) &&
        (game->invulnerable_ticks != 0U) &&
        (((game->ticks / 3U) & 1U) != 0U)) {
        return 1U;
    }
    return 0U;
}

static void CometRender_ClipRect(CometRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < COMET_CATCH_PLAYFIELD_TOP) {
        rect->y0 = COMET_CATCH_PLAYFIELD_TOP;
    }
    if (rect->x1 >= COMET_CATCH_SCREEN_WIDTH) {
        rect->x1 = COMET_CATCH_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= COMET_CATCH_SCREEN_HEIGHT) {
        rect->y1 = COMET_CATCH_SCREEN_HEIGHT - 1;
    }
}

static void CometRender_IncludeRect(CometRenderRect *destination,
                                    CometRenderRect source) {
    if (source.x0 < destination->x0) {
        destination->x0 = source.x0;
    }
    if (source.y0 < destination->y0) {
        destination->y0 = source.y0;
    }
    if (source.x1 > destination->x1) {
        destination->x1 = source.x1;
    }
    if (source.y1 > destination->y1) {
        destination->y1 = source.y1;
    }
}

static uint8_t CometRender_RectsOverlap(CometRenderRect first,
                                        CometRenderRect second) {
    return (first.x0 <= second.x1) &&
           (first.x1 >= second.x0) &&
           (first.y0 <= second.y1) &&
           (first.y1 >= second.y0);
}

static void CometRender_PutPixel(CometRenderRect rect,
                                 uint16_t width,
                                 uint16_t height,
                                 int16_t x,
                                 int16_t y,
                                 uint16_t color) {
    uint32_t offset;

    if ((x < rect.x0) || (x > rect.x1) ||
        (y < rect.y0) || (y > rect.y1)) {
        return;
    }

    offset =
        (((uint32_t)(y - rect.y0) * width) +
         (uint32_t)(x - rect.x0)) * 2U;
    if (offset >= ((uint32_t)width * height * 2U)) {
        return;
    }
    comet_patch_buffer[offset] = (uint8_t)(color >> 8);
    comet_patch_buffer[offset + 1U] = (uint8_t)(color & 0xFFU);
}

static void CometRender_FillPatch(uint16_t width,
                                  uint16_t height,
                                  uint16_t color) {
    uint32_t pixel_count = (uint32_t)width * height;
    uint8_t high = (uint8_t)(color >> 8);
    uint8_t low = (uint8_t)(color & 0xFFU);

    for (uint32_t i = 0U; i < pixel_count; i++) {
        comet_patch_buffer[i * 2U] = high;
        comet_patch_buffer[(i * 2U) + 1U] = low;
    }
}

static void CometRender_FillRectToPatch(CometRenderRect rect,
                                        uint16_t width,
                                        uint16_t height,
                                        int16_t x,
                                        int16_t y,
                                        int16_t draw_width,
                                        int16_t draw_height,
                                        uint16_t color) {
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = (int16_t)(x + draw_width - 1);
    int16_t y1 = (int16_t)(y + draw_height - 1);

    if ((draw_width <= 0) || (draw_height <= 0)) {
        return;
    }
    if (x0 < rect.x0) {
        x0 = rect.x0;
    }
    if (y0 < rect.y0) {
        y0 = rect.y0;
    }
    if (x1 > rect.x1) {
        x1 = rect.x1;
    }
    if (y1 > rect.y1) {
        y1 = rect.y1;
    }

    for (int16_t py = y0; py <= y1; py++) {
        for (int16_t px = x0; px <= x1; px++) {
            CometRender_PutPixel(
                rect, width, height, px, py, color);
        }
    }
}

static uint8_t CometRender_PointInCircle(int16_t x,
                                         int16_t y,
                                         int16_t center_x,
                                         int16_t center_y,
                                         int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;

    return ((dx * dx) + (dy * dy)) <=
           ((int32_t)radius * radius);
}

static void CometRender_DrawCircleToPatch(CometRenderRect rect,
                                          uint16_t width,
                                          uint16_t height,
                                          int16_t center_x,
                                          int16_t center_y,
                                          int16_t radius,
                                          uint16_t color) {
    int16_t x0 = (int16_t)(center_x - radius);
    int16_t x1 = (int16_t)(center_x + radius);
    int16_t y0 = (int16_t)(center_y - radius);
    int16_t y1 = (int16_t)(center_y + radius);

    if (x0 < rect.x0) {
        x0 = rect.x0;
    }
    if (x1 > rect.x1) {
        x1 = rect.x1;
    }
    if (y0 < rect.y0) {
        y0 = rect.y0;
    }
    if (y1 > rect.y1) {
        y1 = rect.y1;
    }

    for (int16_t py = y0; py <= y1; py++) {
        for (int16_t px = x0; px <= x1; px++) {
            if (CometRender_PointInCircle(
                    px, py, center_x, center_y,
                    radius) != 0U) {
                CometRender_PutPixel(
                    rect, width, height, px, py, color);
            }
        }
    }
}

static void CometRender_StarPosition(
    const CometCatchState *game,
    uint8_t column,
    uint8_t row,
    int16_t *x,
    int16_t *y,
    uint8_t *bright) {
    uint32_t hash = CometRender_Hash(
        game->background_seed +
        ((uint32_t)row * 0x9E3779B9U) +
        ((uint32_t)column * 0x85EBCA6BU));

    *x = (int16_t)(
        (column * COMET_STAR_CELL_WIDTH) +
        2U + (hash % 20U));
    *y = (int16_t)(
        COMET_CATCH_PLAYFIELD_TOP +
        (row * COMET_STAR_CELL_HEIGHT) +
        3U + ((hash >> 8) % 16U));
    *bright = (uint8_t)((hash >> 20) & 0x03U);
}

static void CometRender_DrawStarsToPatch(
    const CometCatchState *game,
    CometRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t row = 0U; row < COMET_STAR_ROWS; row++) {
        for (uint8_t column = 0U;
             column < COMET_STAR_COLUMNS;
             column++) {
            int16_t x;
            int16_t y;
            uint8_t bright;
            uint16_t color;

            CometRender_StarPosition(game, column, row,
                                     &x, &y, &bright);
            if ((x < (rect.x0 - 1)) || (x > (rect.x1 + 1)) ||
                (y < (rect.y0 - 1)) || (y > (rect.y1 + 1))) {
                continue;
            }

            color = (bright >= 2U) ?
                    COMET_STAR_BRIGHT_COLOR :
                    COMET_STAR_DIM_COLOR;
            if (bright == 3U) {
                CometRender_PutPixel(
                    rect, width, height, x - 1, y,
                    COMET_STAR_DIM_COLOR);
                CometRender_PutPixel(
                    rect, width, height, x + 1, y,
                    COMET_STAR_DIM_COLOR);
                CometRender_PutPixel(
                    rect, width, height, x, y - 1,
                    COMET_STAR_DIM_COLOR);
                CometRender_PutPixel(
                    rect, width, height, x, y + 1,
                    COMET_STAR_DIM_COLOR);
            }
            CometRender_PutPixel(
                rect, width, height, x, y, color);
        }
    }
}

static void CometRender_DrawStarToPatch(CometRenderRect rect,
                                        uint16_t width,
                                        uint16_t height,
                                        int16_t x,
                                        int16_t y,
                                        uint8_t radius) {
    CometRender_DrawCircleToPatch(
        rect, width, height, x, y, radius,
        COMET_GOOD_COLOR);
    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x - radius - 2), y,
        (int16_t)((radius * 2U) + 5U), 1,
        COMET_GOOD_EDGE_COLOR);
    CometRender_FillRectToPatch(
        rect, width, height,
        x, (int16_t)(y - radius - 2),
        1, (int16_t)((radius * 2U) + 5U),
        COMET_GOOD_EDGE_COLOR);
    CometRender_DrawCircleToPatch(
        rect, width, height, x, y, 2,
        DISPLAY_WHITE);
}

static void CometRender_DrawSparkToPatch(CometRenderRect rect,
                                         uint16_t width,
                                         uint16_t height,
                                         int16_t x,
                                         int16_t y,
                                         uint8_t radius) {
    int16_t extent = (int16_t)(radius + 2U);

    for (int16_t py = (int16_t)(y - extent);
         py <= (int16_t)(y + extent);
         py++) {
        for (int16_t px = (int16_t)(x - extent);
             px <= (int16_t)(x + extent);
             px++) {
            int16_t distance =
                (int16_t)(CometRender_Abs16((int16_t)(px - x)) +
                          CometRender_Abs16((int16_t)(py - y)));

            if (distance <= extent) {
                uint16_t color =
                    (distance < (radius / 2)) ?
                    COMET_SPARK_CORE_COLOR :
                    COMET_SPARK_COLOR;
                CometRender_PutPixel(
                    rect, width, height, px, py, color);
            }
        }
    }
    CometRender_PutPixel(
        rect, width, height, x, y, DISPLAY_WHITE);
}

static void CometRender_ObjectBounds(
    int16_t x,
    int16_t y,
    uint8_t radius,
    CometRenderRect *rect) {
    rect->x0 = (int16_t)(x - radius - 3);
    rect->y0 = (int16_t)(y - radius - 3);
    rect->x1 = (int16_t)(x + radius + 3);
    rect->y1 = (int16_t)(y + radius + 3);
}

static void CometRender_CatcherBounds(int16_t x,
                                      uint8_t pulse_active,
                                      CometRenderRect *rect) {
    int16_t half_width =
        (pulse_active != 0U) ? 39 : 29;
    int16_t top =
        (pulse_active != 0U) ?
        (COMET_CATCHER_Y - 27) :
        (COMET_CATCHER_Y - 8);

    rect->x0 = (int16_t)(x - half_width);
    rect->y0 = top;
    rect->x1 = (int16_t)(x + half_width);
    rect->y1 = COMET_CATCHER_Y + 13;
}

static void CometRender_DrawObjectsToPatch(
    const CometCatchState *game,
    CometRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < COMET_CATCH_MAX_OBJECTS; i++) {
        const CometCatchObject *object = &game->objects[i];
        int16_t x;
        int16_t y;
        CometRenderRect bounds;

        if (object->active == 0U) {
            continue;
        }

        x = CometRender_Round(object->x);
        y = CometRender_Round(object->y);
        CometRender_ObjectBounds(x, y, object->radius, &bounds);
        if (CometRender_RectsOverlap(rect, bounds) == 0U) {
            continue;
        }

        if (object->type == COMET_CATCH_OBJECT_SPARK) {
            CometRender_DrawSparkToPatch(
                rect, width, height, x, y, object->radius);
        } else {
            CometRender_DrawStarToPatch(
                rect, width, height, x, y, object->radius);
        }
    }
}

static void CometRender_DrawCatcherToPatch(
    const CometCatchState *game,
    CometRenderRect rect,
    uint16_t width,
    uint16_t height) {
    int16_t x = CometRender_Round(game->catcher_x);
    uint16_t body_color =
        (CometRender_CatcherFlash(game) != 0U) ?
        COMET_CATCHER_HIT_COLOR :
        COMET_CATCHER_COLOR;
    uint16_t rim_color =
        (game->phase == COMET_CATCH_PHASE_RESTART_PAUSE) ?
        DISPLAY_ORANGE : DISPLAY_WHITE;

    if ((game->pulse_active != 0U) &&
        (game->phase == COMET_CATCH_PHASE_PLAYING)) {
        CometRender_FillRectToPatch(
            rect, width, height,
            (int16_t)(x - 35), COMET_CATCHER_Y - 25,
            71, 2, COMET_NET_EDGE_COLOR);
        CometRender_FillRectToPatch(
            rect, width, height,
            (int16_t)(x - 30), COMET_CATCHER_Y - 18,
            61, 2, COMET_NET_COLOR);
        CometRender_FillRectToPatch(
            rect, width, height,
            (int16_t)(x - 24), COMET_CATCHER_Y - 11,
            49, 2, COMET_NET_COLOR);
        CometRender_FillRectToPatch(
            rect, width, height,
            (int16_t)(x - 25), COMET_CATCHER_Y - 25,
            1, 16, COMET_NET_COLOR);
        CometRender_FillRectToPatch(
            rect, width, height,
            x, COMET_CATCHER_Y - 25,
            1, 16, COMET_NET_EDGE_COLOR);
        CometRender_FillRectToPatch(
            rect, width, height,
            (int16_t)(x + 25), COMET_CATCHER_Y - 25,
            1, 16, COMET_NET_COLOR);
    }

    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x - 28), COMET_CATCHER_Y - 3,
        57, 4, rim_color);
    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x - 24), COMET_CATCHER_Y + 1,
        49, 8, COMET_CATCHER_DARK_COLOR);
    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x - 19), COMET_CATCHER_Y + 6,
        39, 6, body_color);
    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x - 24), COMET_CATCHER_Y + 4,
        4, 8, body_color);
    CometRender_FillRectToPatch(
        rect, width, height,
        (int16_t)(x + 21), COMET_CATCHER_Y + 4,
        4, 8, body_color);
}

static uint8_t CometRender_DrawPatch(
    const CometCatchState *game,
    CometRenderRect rect) {
    uint16_t width;
    uint16_t height;

    CometRender_ClipRect(&rect);
    if ((rect.x1 < rect.x0) || (rect.y1 < rect.y0)) {
        return 1U;
    }

    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);
    if ((width > COMET_RENDER_PATCH_WIDTH) ||
        (height > COMET_RENDER_PATCH_HEIGHT)) {
        return 0U;
    }

    CometRender_FillPatch(width, height, COMET_BACKGROUND_COLOR);
    CometRender_DrawStarsToPatch(game, rect, width, height);
    CometRender_DrawObjectsToPatch(game, rect, width, height);
    CometRender_DrawCatcherToPatch(game, rect, width, height);

    DISPLAY_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        comet_patch_buffer);
    return 1U;
}

static void CometRender_DrawHudStatic(void) {
    DISPLAY_FillRectangle_DMA(
        0, 0, COMET_CATCH_SCREEN_WIDTH,
        COMET_CATCH_PLAYFIELD_TOP, COMET_HEADER_COLOR);
    DISPLAY_FillRectangle_DMA(
        0, COMET_CATCH_PLAYFIELD_TOP - 1,
        COMET_CATCH_SCREEN_WIDTH, 1, DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(
        4, 4, "COMET CATCH", Font_11x18,
        DISPLAY_WHITE, COMET_HEADER_COLOR);
}

static void CometRender_DrawEnergyBar(
    const CometCatchState *game) {
    uint16_t bar_x = 142U;
    uint16_t bar_y = 18U;
    uint16_t bar_w = 122U;
    uint16_t energy_w =
        (uint16_t)(((uint32_t)game->pulse_energy *
                    (bar_w - 2U)) / COMET_CATCH_ENERGY_MAX);
    uint16_t color =
        (game->pulse_active != 0U) ?
        DISPLAY_CYAN : DISPLAY_GREEN;

    DISPLAY_FillRectangle_DMA(
        bar_x, bar_y, bar_w, 7U, DISPLAY_GRAY);
    DISPLAY_FillRectangle_DMA(
        bar_x + 1U, bar_y + 1U,
        bar_w - 2U, 5U, COMET_BAR_BACK_COLOR);
    if (energy_w > 0U) {
        DISPLAY_FillRectangle_DMA(
            bar_x + 1U, bar_y + 1U,
            energy_w, 5U, color);
    }
}

static void CometRender_DrawHudStats(
    const CometCatchState *game,
    uint8_t force) {
    char text[18];

    if ((force != 0U) ||
        (displayed_score != game->score) ||
        (displayed_level != game->level)) {
        DISPLAY_FillRectangle_DMA(
            140, 4, 74, 10, COMET_HEADER_COLOR);
        snprintf(text, sizeof(text), "L%u %05lu",
                 (unsigned int)game->level,
                 (unsigned long)game->score);
        DISPLAY_WriteString_DMA(
            140, 4, text, Font_7x10,
            DISPLAY_CYAN, COMET_HEADER_COLOR);
        displayed_score = game->score;
        displayed_level = game->level;
    }

    if ((force != 0U) ||
        (displayed_combo != game->combo)) {
        DISPLAY_FillRectangle_DMA(
            220, 4, 50, 10, COMET_HEADER_COLOR);
        snprintf(text, sizeof(text), "C%u",
                 (unsigned int)game->combo);
        DISPLAY_WriteString_DMA(
            220, 4, text, Font_7x10,
            (game->combo >= 8U) ? DISPLAY_YELLOW : DISPLAY_GREEN,
            COMET_HEADER_COLOR);
        displayed_combo = game->combo;
    }

    if ((force != 0U) ||
        (displayed_lives != game->lives)) {
        DISPLAY_FillRectangle_DMA(
            288, 4, 30, 10, COMET_HEADER_COLOR);
        snprintf(text, sizeof(text), "H%u",
                 (unsigned int)game->lives);
        DISPLAY_WriteString_DMA(
            288, 4, text, Font_7x10,
            (game->lives > 1U) ? DISPLAY_GREEN : DISPLAY_ORANGE,
            COMET_HEADER_COLOR);
        displayed_lives = game->lives;
    }

    if ((force != 0U) ||
        (displayed_energy != game->pulse_energy)) {
        CometRender_DrawEnergyBar(game);
        displayed_energy = game->pulse_energy;
    }
}

static void CometRender_ResetHudCache(void) {
    displayed_score = 0xFFFFFFFFU;
    displayed_level = 0xFFU;
    displayed_combo = 0xFFU;
    displayed_lives = 0xFFU;
    displayed_energy = 0xFFU;
}

static void CometRender_SaveSnapshots(
    const CometCatchState *game) {
    previous_catcher_x = CometRender_Round(game->catcher_x);
    previous_pulse_active = game->pulse_active;
    previous_catcher_flash = CometRender_CatcherFlash(game);

    for (uint8_t i = 0U; i < COMET_CATCH_MAX_OBJECTS; i++) {
        previous_objects[i].x =
            CometRender_Round(game->objects[i].x);
        previous_objects[i].y =
            CometRender_Round(game->objects[i].y);
        previous_objects[i].radius = game->objects[i].radius;
        previous_objects[i].type = game->objects[i].type;
        previous_objects[i].active = game->objects[i].active;
    }
}

static void CometRender_DrawWholeField(
    const CometCatchState *game) {
    CometRenderRect rect;

    CometRender_ResetHudCache();
    CometRender_DrawHudStatic();
    CometRender_DrawHudStats(game, 1U);

    for (int16_t y = COMET_CATCH_PLAYFIELD_TOP;
         y < COMET_CATCH_SCREEN_HEIGHT;
         y = (int16_t)(y + COMET_RENDER_PATCH_HEIGHT)) {
        for (int16_t x = 0;
             x < COMET_CATCH_SCREEN_WIDTH;
             x = (int16_t)(x + COMET_RENDER_PATCH_WIDTH)) {
            rect.x0 = x;
            rect.y0 = y;
            rect.x1 =
                (int16_t)(x + COMET_RENDER_PATCH_WIDTH - 1);
            rect.y1 =
                (int16_t)(y + COMET_RENDER_PATCH_HEIGHT - 1);
            (void)CometRender_DrawPatch(game, rect);
        }
    }

    if (game->phase == COMET_CATCH_PHASE_RESTART_PAUSE) {
        DISPLAY_WriteString_DMA(
            86, 92, "NET BROKE", Font_16x26,
            DISPLAY_ORANGE, COMET_BACKGROUND_COLOR);
        DISPLAY_WriteString_DMA(
            111, 126, "AUTO RESTART", Font_7x10,
            DISPLAY_CYAN, COMET_BACKGROUND_COLOR);
    }

    CometRender_SaveSnapshots(game);
    comet_renderer_initialized = 1U;
}

void CometCatchRender_Init(const CometCatchState *game) {
    comet_renderer_initialized = 0U;
    CometRender_DrawWholeField(game);
}

void CometCatchRender_Frame(const CometCatchState *game,
                            CometCatchEvent event) {
    int16_t catcher_x;
    uint8_t catcher_flash;

    if ((comet_renderer_initialized == 0U) ||
        ((event & (COMET_CATCH_EVENT_GAME_STARTED |
                   COMET_CATCH_EVENT_RESTART_PAUSE)) != 0U)) {
        CometRender_DrawWholeField(game);
        return;
    }

    if ((event & (COMET_CATCH_EVENT_OBJECTS |
                  COMET_CATCH_EVENT_PULSE_CHANGED)) != 0U) {
        for (uint8_t i = 0U; i < COMET_CATCH_MAX_OBJECTS; i++) {
            const CometCatchObject *object = &game->objects[i];
            const CometObjectSnapshot *previous =
                &previous_objects[i];
            int16_t current_x = CometRender_Round(object->x);
            int16_t current_y = CometRender_Round(object->y);
            uint8_t current_radius = object->radius;
            CometRenderRect dirty;

            if ((object->active == 0U) &&
                (previous->active == 0U)) {
                continue;
            }
            if ((object->active == previous->active) &&
                (current_x == previous->x) &&
                (current_y == previous->y) &&
                (current_radius == previous->radius) &&
                (object->type == previous->type)) {
                continue;
            }

            if (previous->active != 0U) {
                CometRender_ObjectBounds(
                    previous->x, previous->y,
                    previous->radius, &dirty);
            } else {
                CometRender_ObjectBounds(
                    current_x, current_y,
                    current_radius, &dirty);
            }
            if (object->active != 0U) {
                CometRenderRect current;

                CometRender_ObjectBounds(
                    current_x, current_y,
                    current_radius, &current);
                CometRender_IncludeRect(&dirty, current);
            }
            if (CometRender_DrawPatch(game, dirty) == 0U) {
                CometRender_DrawWholeField(game);
                return;
            }
        }

        catcher_x = CometRender_Round(game->catcher_x);
        catcher_flash = CometRender_CatcherFlash(game);
        if ((catcher_x != previous_catcher_x) ||
            (game->pulse_active != previous_pulse_active) ||
            (catcher_flash != previous_catcher_flash)) {
            CometRenderRect dirty;
            CometRenderRect current;

            CometRender_CatcherBounds(
                previous_catcher_x,
                previous_pulse_active,
                &dirty);
            CometRender_CatcherBounds(
                catcher_x,
                game->pulse_active,
                &current);
            CometRender_IncludeRect(&dirty, current);
            if (CometRender_DrawPatch(game, dirty) == 0U) {
                CometRender_DrawWholeField(game);
                return;
            }
        }
    }

    if ((event & COMET_CATCH_EVENT_HUD) != 0U) {
        CometRender_DrawHudStats(game, 0U);
    }

    CometRender_SaveSnapshots(game);
}
