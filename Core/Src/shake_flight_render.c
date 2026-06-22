#include "shake_flight_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define SHAKE_RENDER_PATCH_SIZE       36

#define SHAKE_SKY_COLOR               DISPLAY_COLOR565(4, 18, 33)
#define SHAKE_GROUND_COLOR            DISPLAY_COLOR565(70, 120, 45)
#define SHAKE_GROUND_DARK             DISPLAY_COLOR565(40, 80, 36)
#define SHAKE_PIPE_COLOR              DISPLAY_COLOR565(30, 190, 70)
#define SHAKE_BIRD_COLOR              DISPLAY_YELLOW
#define SHAKE_BIRD_WING               DISPLAY_ORANGE

#if (SHAKE_RENDER_PATCH_SIZE * SHAKE_RENDER_PATCH_SIZE * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for shake flight patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} ShakeRenderRect;

#define shake_patch_buffer render_scratch_buffer
static int16_t previous_bird_y;
static int16_t previous_pipe_x[SHAKE_FLIGHT_PIPE_COUNT];
static int16_t previous_pipe_gap[SHAKE_FLIGHT_PIPE_COUNT];
static uint8_t previous_gap_height;
static uint8_t shake_renderer_initialized;

static int16_t ShakeRender_Round(float value) {
    return (int16_t)(
        value + ((value >= 0.0f) ? 0.5f : -0.5f));
}

static uint8_t ShakeRender_PointInCircle(
    int16_t x,
    int16_t y,
    int16_t center_x,
    int16_t center_y,
    int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;

    return ((dx * dx) + (dy * dy)) <=
           ((int32_t)radius * radius);
}

static void ShakeRender_ClipRect(ShakeRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < SHAKE_FLIGHT_PLAYFIELD_TOP) {
        rect->y0 = SHAKE_FLIGHT_PLAYFIELD_TOP;
    }
    if (rect->x1 >= SHAKE_FLIGHT_SCREEN_WIDTH) {
        rect->x1 = SHAKE_FLIGHT_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= SHAKE_FLIGHT_SCREEN_HEIGHT) {
        rect->y1 = SHAKE_FLIGHT_SCREEN_HEIGHT - 1;
    }
}

static uint16_t ShakeRender_StaticPixel(int16_t x, int16_t y) {
    if (y >= SHAKE_FLIGHT_GROUND_Y) {
        if (((y - SHAKE_FLIGHT_GROUND_Y) % 8) == 0) {
            return SHAKE_GROUND_DARK;
        }
        return SHAKE_GROUND_COLOR;
    }
    return SHAKE_SKY_COLOR;
}

static void ShakeRender_PutPixel(
    ShakeRenderRect rect,
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
    shake_patch_buffer[offset] = (uint8_t)(color >> 8);
    shake_patch_buffer[offset + 1U] =
        (uint8_t)(color & 0xFFU);
}

static void ShakeRender_FillRectClipped(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint16_t color) {
    if ((width <= 0) || (height <= 0)) {
        return;
    }
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if ((x >= SHAKE_FLIGHT_SCREEN_WIDTH) ||
        (y >= SHAKE_FLIGHT_SCREEN_HEIGHT) ||
        (width <= 0) ||
        (height <= 0)) {
        return;
    }
    if ((x + width) > SHAKE_FLIGHT_SCREEN_WIDTH) {
        width = SHAKE_FLIGHT_SCREEN_WIDTH - x;
    }
    if ((y + height) > SHAKE_FLIGHT_SCREEN_HEIGHT) {
        height = SHAKE_FLIGHT_SCREEN_HEIGHT - y;
    }
    DISPLAY_FillRectangle_DMA(
        (uint16_t)x, (uint16_t)y,
        (uint16_t)width, (uint16_t)height,
        color);
}

static void ShakeRender_DrawPipePart(
    int16_t x,
    int16_t width,
    int16_t gap_y,
    uint8_t gap_height,
    uint16_t color) {
    int16_t half_gap = gap_height / 2;
    int16_t gap_top = gap_y - half_gap;
    int16_t gap_bottom = gap_y + half_gap;
    int16_t top_height =
        gap_top - SHAKE_FLIGHT_PLAYFIELD_TOP;
    int16_t bottom_height =
        SHAKE_FLIGHT_GROUND_Y - gap_bottom;

    ShakeRender_FillRectClipped(
        x, SHAKE_FLIGHT_PLAYFIELD_TOP,
        width, top_height, color);
    ShakeRender_FillRectClipped(
        x, gap_bottom,
        width, bottom_height, color);
}

static void ShakeRender_ErasePipePart(
    int16_t x,
    int16_t width,
    int16_t gap_y,
    uint8_t gap_height) {
    int16_t half_gap = gap_height / 2;
    int16_t gap_top = gap_y - half_gap;
    int16_t gap_bottom = gap_y + half_gap;

    ShakeRender_FillRectClipped(
        x, SHAKE_FLIGHT_PLAYFIELD_TOP,
        width, gap_top - SHAKE_FLIGHT_PLAYFIELD_TOP,
        SHAKE_SKY_COLOR);
    ShakeRender_FillRectClipped(
        x, gap_bottom,
        width, SHAKE_FLIGHT_GROUND_Y - gap_bottom,
        SHAKE_SKY_COLOR);
}

static void ShakeRender_DrawPipeFull(
    int16_t x,
    int16_t gap_y,
    uint8_t gap_height,
    uint16_t color) {
    ShakeRender_DrawPipePart(
        x, SHAKE_FLIGHT_PIPE_WIDTH,
        gap_y, gap_height, color);
}

static void ShakeRender_DrawPipesToPatch(
    const ShakeFlightState *game,
    ShakeRenderRect rect,
    uint16_t width,
    uint16_t height) {
    uint8_t gap_height = ShakeFlight_GapHeight(game);

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        int16_t x = ShakeRender_Round(game->pipes[i].x);
        int16_t half_gap = gap_height / 2;
        int16_t gap_top = game->pipes[i].gap_y - half_gap;
        int16_t gap_bottom = game->pipes[i].gap_y + half_gap;
        int16_t pipe_left = x;
        int16_t pipe_right = x + SHAKE_FLIGHT_PIPE_WIDTH - 1;

        if ((pipe_right < rect.x0) || (pipe_left > rect.x1)) {
            continue;
        }

        for (int16_t py = rect.y0; py <= rect.y1; py++) {
            if ((py >= gap_top) && (py <= gap_bottom)) {
                continue;
            }
            if ((py < SHAKE_FLIGHT_PLAYFIELD_TOP) ||
                (py >= SHAKE_FLIGHT_GROUND_Y)) {
                continue;
            }
            for (int16_t px = pipe_left;
                 px <= pipe_right;
                 px++) {
                ShakeRender_PutPixel(
                    rect, width, height,
                    px, py, SHAKE_PIPE_COLOR);
            }
        }
    }
}

static void ShakeRender_DrawBirdToPatch(
    const ShakeFlightState *game,
    ShakeRenderRect rect,
    uint16_t width,
    uint16_t height) {
    int16_t bird_y = ShakeRender_Round(game->bird_y);

    for (int16_t y = bird_y - SHAKE_FLIGHT_BIRD_RADIUS - 2;
         y <= bird_y + SHAKE_FLIGHT_BIRD_RADIUS + 2;
         y++) {
        for (int16_t x = SHAKE_FLIGHT_BIRD_X -
                         SHAKE_FLIGHT_BIRD_RADIUS - 3;
             x <= SHAKE_FLIGHT_BIRD_X +
                  SHAKE_FLIGHT_BIRD_RADIUS + 5;
             x++) {
            int16_t dx = x - SHAKE_FLIGHT_BIRD_X;
            int16_t dy = y - bird_y;
            uint16_t color;

            if ((dx >= 5) &&
                (dx <= 11) &&
                (dy >= -3) &&
                (dy <= 3) &&
                ((dx + dy) <= 12) &&
                ((dx - dy) <= 12)) {
                ShakeRender_PutPixel(
                    rect, width, height,
                    x, y, SHAKE_BIRD_WING);
                continue;
            }
            if (ShakeRender_PointInCircle(
                    x, y,
                    SHAKE_FLIGHT_BIRD_X,
                    bird_y,
                    SHAKE_FLIGHT_BIRD_RADIUS) == 0U) {
                continue;
            }

            color = SHAKE_BIRD_COLOR;
            if (ShakeRender_PointInCircle(
                    x, y,
                    SHAKE_FLIGHT_BIRD_X - 3,
                    bird_y + 2, 3) != 0U) {
                color = SHAKE_BIRD_WING;
            }
            if (ShakeRender_PointInCircle(
                    x, y,
                    SHAKE_FLIGHT_BIRD_X + 3,
                    bird_y - 3, 1) != 0U) {
                color = DISPLAY_WHITE;
            }
            ShakeRender_PutPixel(
                rect, width, height, x, y, color);
        }
    }
}

static uint8_t ShakeRender_DrawBirdPatch(
    const ShakeFlightState *game,
    ShakeRenderRect rect) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    ShakeRender_ClipRect(&rect);
    if ((rect.x1 < rect.x0) || (rect.y1 < rect.y0)) {
        return 1U;
    }
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);
    if ((width > SHAKE_RENDER_PATCH_SIZE) ||
        (height > SHAKE_RENDER_PATCH_SIZE)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color = ShakeRender_StaticPixel(x, y);

            shake_patch_buffer[offset++] =
                (uint8_t)(color >> 8);
            shake_patch_buffer[offset++] =
                (uint8_t)(color & 0xFFU);
        }
    }

    ShakeRender_DrawPipesToPatch(game, rect, width, height);
    ShakeRender_DrawBirdToPatch(game, rect, width, height);
    DISPLAY_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        shake_patch_buffer);
    return 1U;
}

static void ShakeRender_DrawHud(const ShakeFlightState *game) {
    char text[24];

    DISPLAY_FillRectangle_DMA(
        0, 0, SHAKE_FLIGHT_SCREEN_WIDTH,
        SHAKE_FLIGHT_PLAYFIELD_TOP, DISPLAY_BLACK);
    DISPLAY_FillRectangle_DMA(
        0, SHAKE_FLIGHT_PLAYFIELD_TOP - 1,
        SHAKE_FLIGHT_SCREEN_WIDTH, 1, DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(
        4, 4, "SHAKE FLIGHT", Font_11x18,
        DISPLAY_WHITE, DISPLAY_BLACK);
    snprintf(text, sizeof(text), "L%u %04lu",
             (unsigned int)game->level,
             (unsigned long)game->score);
    DISPLAY_WriteString_DMA(
        180, 9, text, Font_7x10,
        DISPLAY_CYAN, DISPLAY_BLACK);
    DISPLAY_WriteString_DMA(
        268, 9, "Z SHAKE", Font_7x10,
        DISPLAY_YELLOW, DISPLAY_BLACK);
}

static void ShakeRender_DrawStaticField(void) {
    DISPLAY_FillScreen_DMA(SHAKE_SKY_COLOR);
    DISPLAY_FillRectangle_DMA(
        0, SHAKE_FLIGHT_GROUND_Y,
        SHAKE_FLIGHT_SCREEN_WIDTH,
        SHAKE_FLIGHT_SCREEN_HEIGHT - SHAKE_FLIGHT_GROUND_Y,
        SHAKE_GROUND_COLOR);
    DISPLAY_FillRectangle_DMA(
        0, SHAKE_FLIGHT_GROUND_Y,
        SHAKE_FLIGHT_SCREEN_WIDTH, 2,
        SHAKE_GROUND_DARK);
}

static void ShakeRender_SaveSnapshots(
    const ShakeFlightState *game) {
    previous_bird_y = ShakeRender_Round(game->bird_y);
    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        previous_pipe_x[i] =
            ShakeRender_Round(game->pipes[i].x);
        previous_pipe_gap[i] = game->pipes[i].gap_y;
    }
    previous_gap_height = ShakeFlight_GapHeight(game);
}

static void ShakeRender_DrawAllPipes(
    const ShakeFlightState *game) {
    uint8_t gap_height = ShakeFlight_GapHeight(game);

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        ShakeRender_DrawPipeFull(
            ShakeRender_Round(game->pipes[i].x),
            game->pipes[i].gap_y,
            gap_height,
            SHAKE_PIPE_COLOR);
    }
}

static void ShakeRender_DrawWholeField(
    const ShakeFlightState *game) {
    int16_t bird_y = ShakeRender_Round(game->bird_y);
    ShakeRenderRect bird_rect = {
        SHAKE_FLIGHT_BIRD_X - SHAKE_FLIGHT_BIRD_RADIUS - 4,
        bird_y - SHAKE_FLIGHT_BIRD_RADIUS - 3,
        SHAKE_FLIGHT_BIRD_X + SHAKE_FLIGHT_BIRD_RADIUS + 6,
        bird_y + SHAKE_FLIGHT_BIRD_RADIUS + 3
    };

    ShakeRender_DrawStaticField();
    ShakeRender_DrawAllPipes(game);
    ShakeRender_DrawHud(game);
    ShakeRender_DrawBirdPatch(game, bird_rect);

    if (game->phase == SHAKE_FLIGHT_PHASE_RECOVERY) {
        DISPLAY_WriteString_DMA(
            94, 92, "CRASH", Font_16x26,
            DISPLAY_ORANGE, SHAKE_SKY_COLOR);
        DISPLAY_WriteString_DMA(
            96, 126, "AUTO RESTART", Font_7x10,
            DISPLAY_CYAN, SHAKE_SKY_COLOR);
    }

    ShakeRender_SaveSnapshots(game);
    shake_renderer_initialized = 1U;
}

void ShakeFlightRender_Init(const ShakeFlightState *game) {
    shake_renderer_initialized = 0U;
    ShakeRender_DrawWholeField(game);
}

void ShakeFlightRender_Frame(const ShakeFlightState *game,
                             ShakeFlightEvent event) {
    int16_t bird_y;
    uint8_t pipes_changed = 0U;
    uint8_t gap_height = ShakeFlight_GapHeight(game);
    ShakeRenderRect bird_dirty;

    if ((shake_renderer_initialized == 0U) ||
        ((event & (SHAKE_FLIGHT_EVENT_ROUND_STARTED |
                   SHAKE_FLIGHT_EVENT_CRASH)) != 0U)) {
        ShakeRender_DrawWholeField(game);
        return;
    }
    if (game->phase != SHAKE_FLIGHT_PHASE_PLAYING) {
        return;
    }
    if (gap_height != previous_gap_height) {
        ShakeRender_DrawWholeField(game);
        return;
    }

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        int16_t pipe_x = ShakeRender_Round(game->pipes[i].x);
        int16_t delta = previous_pipe_x[i] - pipe_x;

        if ((pipe_x == previous_pipe_x[i]) &&
            (game->pipes[i].gap_y == previous_pipe_gap[i])) {
            continue;
        }

        pipes_changed = 1U;
        if ((delta <= 0) ||
            (delta > 8) ||
            (game->pipes[i].gap_y != previous_pipe_gap[i])) {
            ShakeRender_ErasePipePart(
                previous_pipe_x[i],
                SHAKE_FLIGHT_PIPE_WIDTH,
                previous_pipe_gap[i],
                gap_height);
            ShakeRender_DrawPipeFull(
                pipe_x,
                game->pipes[i].gap_y,
                gap_height,
                SHAKE_PIPE_COLOR);
        } else {
            ShakeRender_ErasePipePart(
                pipe_x + SHAKE_FLIGHT_PIPE_WIDTH,
                delta,
                previous_pipe_gap[i],
                gap_height);
            ShakeRender_DrawPipePart(
                pipe_x,
                delta,
                game->pipes[i].gap_y,
                gap_height,
                SHAKE_PIPE_COLOR);
        }
    }

    bird_y = ShakeRender_Round(game->bird_y);
    if ((bird_y != previous_bird_y) || (pipes_changed != 0U)) {
        bird_dirty.x0 =
            SHAKE_FLIGHT_BIRD_X -
            SHAKE_FLIGHT_BIRD_RADIUS - 4;
        bird_dirty.x1 =
            SHAKE_FLIGHT_BIRD_X +
            SHAKE_FLIGHT_BIRD_RADIUS + 6;
        bird_dirty.y0 =
            ((bird_y < previous_bird_y) ?
             bird_y : previous_bird_y) -
            SHAKE_FLIGHT_BIRD_RADIUS - 3;
        bird_dirty.y1 =
            ((bird_y > previous_bird_y) ?
             bird_y : previous_bird_y) +
            SHAKE_FLIGHT_BIRD_RADIUS + 3;
        if (ShakeRender_DrawBirdPatch(
                game, bird_dirty) == 0U) {
            ShakeRender_DrawWholeField(game);
            return;
        }
    }

    if ((event & SHAKE_FLIGHT_EVENT_HUD_CHANGED) != 0U) {
        ShakeRender_DrawHud(game);
    }
    ShakeRender_SaveSnapshots(game);
}
