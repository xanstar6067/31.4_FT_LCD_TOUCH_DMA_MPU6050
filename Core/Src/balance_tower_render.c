#include "balance_tower_render.h"

#include <stdio.h>
#include "ili9341.h"
#include "render_scratch.h"

#define TOWER_RENDER_PATCH_WIDTH       160
#define TOWER_RENDER_PATCH_HEIGHT      48

#define TOWER_BACKGROUND_COLOR         ILI9341_COLOR565(5, 10, 16)
#define TOWER_GRID_COLOR               ILI9341_COLOR565(15, 30, 40)
#define TOWER_CENTER_COLOR             ILI9341_COLOR565(30, 65, 72)
#define TOWER_PLATFORM_COLOR           ILI9341_COLOR565(65, 205, 190)
#define TOWER_PLATFORM_EDGE_COLOR      ILI9341_WHITE
#define TOWER_BASE_COLOR               ILI9341_COLOR565(45, 75, 82)
#define TOWER_BLOCK_BORDER_COLOR       ILI9341_COLOR565(235, 225, 190)

#if (TOWER_RENDER_PATCH_WIDTH * TOWER_RENDER_PATCH_HEIGHT * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for balance tower patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} BalanceTowerRect;

static const uint16_t tower_block_colors[] = {
    ILI9341_ORANGE,
    ILI9341_CYAN,
    ILI9341_GREEN,
    ILI9341_MAGENTA,
    ILI9341_YELLOW
};

#define tower_patch_buffer render_scratch_buffer
static int16_t previous_platform_angle;
static int16_t previous_block_x[BALANCE_TOWER_MAX_BLOCKS];
static uint8_t tower_renderer_initialized;

static int16_t BalanceTowerRender_Round(float value) {
    return (int16_t)(
        value + ((value >= 0.0f) ? 0.5f : -0.5f));
}

static int16_t BalanceTowerRender_AngleTenths(
    const BalanceTowerState *game) {
    return BalanceTowerRender_Round(game->platform_angle * 10.0f);
}

static int16_t BalanceTowerRender_PlatformDelta(
    int16_t angle_tenths) {
    return (int16_t)(
        ((BALANCE_TOWER_PLATFORM_WIDTH / 2) * angle_tenths) /
        573);
}

static void BalanceTowerRender_PlatformBounds(
    int16_t angle_tenths,
    BalanceTowerRect *rect) {
    int16_t delta =
        BalanceTowerRender_PlatformDelta(angle_tenths);
    int16_t left_y = BALANCE_TOWER_PLATFORM_Y - delta;
    int16_t right_y = BALANCE_TOWER_PLATFORM_Y + delta;

    rect->x0 =
        BALANCE_TOWER_PLATFORM_CENTER_X -
        (BALANCE_TOWER_PLATFORM_WIDTH / 2) - 2;
    rect->x1 =
        BALANCE_TOWER_PLATFORM_CENTER_X +
        (BALANCE_TOWER_PLATFORM_WIDTH / 2) + 2;
    rect->y0 =
        ((left_y < right_y) ? left_y : right_y) -
        BALANCE_TOWER_PLATFORM_THICKNESS - 1;
    rect->y1 =
        ((left_y > right_y) ? left_y : right_y) +
        BALANCE_TOWER_PLATFORM_THICKNESS + 1;
}

static void BalanceTowerRender_ClipRect(
    BalanceTowerRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < BALANCE_TOWER_PLAYFIELD_TOP) {
        rect->y0 = BALANCE_TOWER_PLAYFIELD_TOP;
    }
    if (rect->x1 >= BALANCE_TOWER_SCREEN_WIDTH) {
        rect->x1 = BALANCE_TOWER_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= BALANCE_TOWER_SCREEN_HEIGHT) {
        rect->y1 = BALANCE_TOWER_SCREEN_HEIGHT - 1;
    }
}

static void BalanceTowerRender_IncludeRect(
    BalanceTowerRect *destination,
    BalanceTowerRect source) {
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

static uint8_t BalanceTowerRender_RectsOverlap(
    BalanceTowerRect first,
    BalanceTowerRect second) {
    return (first.x0 <= second.x1) &&
           (first.x1 >= second.x0) &&
           (first.y0 <= second.y1) &&
           (first.y1 >= second.y0);
}

static uint16_t BalanceTowerRender_StaticPixel(
    int16_t x,
    int16_t y) {
    if ((y >= 216) && (x >= 153) && (x <= 167)) {
        int16_t half_width = (y - 213) / 2;

        if ((x >= (160 - half_width)) &&
            (x <= (160 + half_width))) {
            return TOWER_BASE_COLOR;
        }
    }
    if ((x == BALANCE_TOWER_PLATFORM_CENTER_X) &&
        (y < BALANCE_TOWER_PLATFORM_Y)) {
        return TOWER_CENTER_COLOR;
    }
    if ((((x - 20) % 40) == 0) ||
        (((y - BALANCE_TOWER_PLAYFIELD_TOP) % 32) == 0)) {
        return TOWER_GRID_COLOR;
    }
    return TOWER_BACKGROUND_COLOR;
}

static void BalanceTowerRender_PutPixel(
    BalanceTowerRect rect,
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
    tower_patch_buffer[offset] = (uint8_t)(color >> 8);
    tower_patch_buffer[offset + 1U] =
        (uint8_t)(color & 0xFFU);
}

static void BalanceTowerRender_DrawPlatformToPatch(
    const BalanceTowerState *game,
    BalanceTowerRect rect,
    uint16_t width,
    uint16_t height) {
    int16_t angle_tenths =
        BalanceTowerRender_AngleTenths(game);
    int16_t delta =
        BalanceTowerRender_PlatformDelta(angle_tenths);
    int16_t left =
        BALANCE_TOWER_PLATFORM_CENTER_X -
        (BALANCE_TOWER_PLATFORM_WIDTH / 2);
    int16_t right =
        BALANCE_TOWER_PLATFORM_CENTER_X +
        (BALANCE_TOWER_PLATFORM_WIDTH / 2);
    BalanceTowerRect bounds;

    BalanceTowerRender_PlatformBounds(angle_tenths, &bounds);
    if (BalanceTowerRender_RectsOverlap(rect, bounds) == 0U) {
        return;
    }

    for (int16_t x = left; x <= right; x++) {
        int16_t line_y =
            BALANCE_TOWER_PLATFORM_Y - delta +
            (int16_t)(
                ((int32_t)(x - left) * (2 * delta)) /
                BALANCE_TOWER_PLATFORM_WIDTH);

        for (int16_t thickness =
                 -(BALANCE_TOWER_PLATFORM_THICKNESS / 2);
             thickness <=
                 (BALANCE_TOWER_PLATFORM_THICKNESS / 2);
             thickness++) {
            uint16_t color =
                (thickness ==
                 -(BALANCE_TOWER_PLATFORM_THICKNESS / 2)) ?
                TOWER_PLATFORM_EDGE_COLOR :
                TOWER_PLATFORM_COLOR;

            BalanceTowerRender_PutPixel(
                rect, width, height,
                x, line_y + thickness, color);
        }
    }
}

static void BalanceTowerRender_DrawBlocksToPatch(
    const BalanceTowerState *game,
    BalanceTowerRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < game->block_count; i++) {
        int16_t x;
        int16_t y;
        int16_t block_width;
        int16_t block_height;
        BalanceTowerRect bounds;
        uint16_t fill_color =
            tower_block_colors[
                game->block_styles[i] %
                (sizeof(tower_block_colors) /
                 sizeof(tower_block_colors[0]))];

        BalanceTower_GetBlockRect(
            game, i, &x, &y, &block_width, &block_height);
        bounds.x0 = x;
        bounds.y0 = y;
        bounds.x1 = x + block_width - 1;
        bounds.y1 = y + block_height - 1;
        if (BalanceTowerRender_RectsOverlap(rect, bounds) == 0U) {
            continue;
        }

        for (int16_t py = bounds.y0; py <= bounds.y1; py++) {
            for (int16_t px = bounds.x0; px <= bounds.x1; px++) {
                uint16_t color =
                    ((px == bounds.x0) ||
                     (px == bounds.x1) ||
                     (py == bounds.y0) ||
                     (py == bounds.y1)) ?
                    TOWER_BLOCK_BORDER_COLOR : fill_color;

                BalanceTowerRender_PutPixel(
                    rect, width, height, px, py, color);
            }
        }
    }
}

static uint8_t BalanceTowerRender_DrawPatch(
    const BalanceTowerState *game,
    BalanceTowerRect rect) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    BalanceTowerRender_ClipRect(&rect);
    if ((rect.x1 < rect.x0) || (rect.y1 < rect.y0)) {
        return 1U;
    }

    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);
    if ((width > TOWER_RENDER_PATCH_WIDTH) ||
        (height > TOWER_RENDER_PATCH_HEIGHT)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                BalanceTowerRender_StaticPixel(x, y);

            tower_patch_buffer[offset++] =
                (uint8_t)(color >> 8);
            tower_patch_buffer[offset++] =
                (uint8_t)(color & 0xFFU);
        }
    }

    BalanceTowerRender_DrawPlatformToPatch(
        game, rect, width, height);
    BalanceTowerRender_DrawBlocksToPatch(
        game, rect, width, height);
    ILI9341_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        tower_patch_buffer);
    return 1U;
}

static void BalanceTowerRender_DrawHud(
    const BalanceTowerState *game) {
    char text[24];

    ILI9341_FillRectangle_DMA(
        0, 0, BALANCE_TOWER_SCREEN_WIDTH,
        BALANCE_TOWER_PLAYFIELD_TOP, ILI9341_BLACK);
    ILI9341_FillRectangle_DMA(
        0, BALANCE_TOWER_PLAYFIELD_TOP - 1,
        BALANCE_TOWER_SCREEN_WIDTH, 1, ILI9341_GRAY);
    ILI9341_WriteString_DMA(
        4, 4, "BALANCE TOWER", Font_11x18,
        ILI9341_WHITE, ILI9341_BLACK);
    snprintf(text, sizeof(text), "L%u %04lu",
             (unsigned int)game->level,
             (unsigned long)game->score);
    ILI9341_WriteString_DMA(
        180, 9, text, Font_7x10,
        ILI9341_CYAN, ILI9341_BLACK);
    ILI9341_WriteString_DMA(
        280, 9, "GZ", Font_7x10,
        ILI9341_GREEN, ILI9341_BLACK);
}

static void BalanceTowerRender_DrawStaticField(void) {
    ILI9341_FillScreen_DMA(TOWER_BACKGROUND_COLOR);

    for (uint16_t x = 20U;
         x < BALANCE_TOWER_SCREEN_WIDTH;
         x += 40U) {
        ILI9341_FillRectangle_DMA(
            x, BALANCE_TOWER_PLAYFIELD_TOP,
            1, BALANCE_TOWER_PLATFORM_Y -
               BALANCE_TOWER_PLAYFIELD_TOP,
            TOWER_GRID_COLOR);
    }
    for (uint16_t y = BALANCE_TOWER_PLAYFIELD_TOP;
         y < BALANCE_TOWER_PLATFORM_Y;
         y += 32U) {
        ILI9341_FillRectangle_DMA(
            0, y, BALANCE_TOWER_SCREEN_WIDTH,
            1, TOWER_GRID_COLOR);
    }
    ILI9341_FillRectangle_DMA(
        BALANCE_TOWER_PLATFORM_CENTER_X,
        BALANCE_TOWER_PLAYFIELD_TOP,
        1, BALANCE_TOWER_PLATFORM_Y -
           BALANCE_TOWER_PLAYFIELD_TOP,
        TOWER_CENTER_COLOR);
    for (uint16_t y = 216U;
         y < BALANCE_TOWER_SCREEN_HEIGHT;
         y++) {
        uint16_t half_width = (y - 213U) / 2U;

        ILI9341_FillRectangle_DMA(
            BALANCE_TOWER_PLATFORM_CENTER_X - half_width,
            y, (2U * half_width) + 1U, 1,
            TOWER_BASE_COLOR);
    }
}

static void BalanceTowerRender_SaveSnapshots(
    const BalanceTowerState *game) {
    previous_platform_angle =
        BalanceTowerRender_AngleTenths(game);
    for (uint8_t i = 0U; i < BALANCE_TOWER_MAX_BLOCKS; i++) {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;

        BalanceTower_GetBlockRect(
            game, i, &x, &y, &width, &height);
        previous_block_x[i] = x;
    }
}

static void BalanceTowerRender_DrawDynamic(
    const BalanceTowerState *game) {
    BalanceTowerRect rect;

    BalanceTowerRender_PlatformBounds(
        BalanceTowerRender_AngleTenths(game), &rect);
    BalanceTowerRender_DrawPatch(game, rect);

    for (uint8_t i = 0U; i < game->block_count; i++) {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;

        BalanceTower_GetBlockRect(
            game, i, &x, &y, &width, &height);
        rect.x0 = x - 1;
        rect.y0 = y - 1;
        rect.x1 = x + width;
        rect.y1 = y + height;
        BalanceTowerRender_DrawPatch(game, rect);
    }
}

static void BalanceTowerRender_DrawWholeField(
    const BalanceTowerState *game) {
    BalanceTowerRender_DrawStaticField();
    BalanceTowerRender_DrawHud(game);

    if (game->phase == BALANCE_TOWER_PHASE_CALIBRATING) {
        ILI9341_WriteString_DMA(
            72, 84, "HOLD STILL", Font_16x26,
            ILI9341_YELLOW, TOWER_BACKGROUND_COLOR);
        ILI9341_WriteString_DMA(
            90, 121, "GYRO CALIBRATION", Font_7x10,
            ILI9341_CYAN, TOWER_BACKGROUND_COLOR);
    } else {
        BalanceTowerRender_DrawDynamic(game);
        if (game->phase == BALANCE_TOWER_PHASE_RECOVERY) {
            ILI9341_WriteString_DMA(
                80, 90, "AUTO LEVEL", Font_16x26,
                ILI9341_ORANGE, TOWER_BACKGROUND_COLOR);
        }
    }

    BalanceTowerRender_SaveSnapshots(game);
    tower_renderer_initialized = 1U;
}

void BalanceTowerRender_Init(const BalanceTowerState *game) {
    tower_renderer_initialized = 0U;
    BalanceTowerRender_DrawWholeField(game);
}

void BalanceTowerRender_Frame(const BalanceTowerState *game,
                              BalanceTowerEvent event) {
    int16_t angle_tenths;
    BalanceTowerRect platform_dirty;

    if ((tower_renderer_initialized == 0U) ||
        ((event & (BALANCE_TOWER_EVENT_ROUND_STARTED |
                   BALANCE_TOWER_EVENT_LEVEL_STARTED |
                   BALANCE_TOWER_EVENT_RECOVERY_STARTED)) != 0U)) {
        BalanceTowerRender_DrawWholeField(game);
        return;
    }
    if (game->phase != BALANCE_TOWER_PHASE_PLAYING) {
        return;
    }

    angle_tenths = BalanceTowerRender_AngleTenths(game);
    if (angle_tenths != previous_platform_angle) {
        BalanceTowerRect current;

        BalanceTowerRender_PlatformBounds(
            previous_platform_angle, &platform_dirty);
        BalanceTowerRender_PlatformBounds(
            angle_tenths, &current);
        BalanceTowerRender_IncludeRect(&platform_dirty, current);
        if (BalanceTowerRender_DrawPatch(
                game, platform_dirty) == 0U) {
            BalanceTowerRender_DrawWholeField(game);
            return;
        }
    }

    for (uint8_t i = 0U; i < game->block_count; i++) {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;

        BalanceTower_GetBlockRect(
            game, i, &x, &y, &width, &height);
        if (x != previous_block_x[i]) {
            BalanceTowerRect dirty = {
                (x < previous_block_x[i]) ?
                    x - 1 : previous_block_x[i] - 1,
                y - 1,
                ((x > previous_block_x[i]) ?
                    x : previous_block_x[i]) + width,
                y + height
            };

            if (BalanceTowerRender_DrawPatch(
                    game, dirty) == 0U) {
                BalanceTowerRender_DrawWholeField(game);
                return;
            }
        }
    }

    if ((event & BALANCE_TOWER_EVENT_HUD_CHANGED) != 0U) {
        BalanceTowerRender_DrawHud(game);
    }
    BalanceTowerRender_SaveSnapshots(game);
}
