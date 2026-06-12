#include "game_render.h"

#include <stdio.h>
#include "ili9341.h"

#define RENDER_PATCH_MAX_WIDTH   64
#define RENDER_PATCH_MAX_HEIGHT  64
#define RENDER_HUD_HEIGHT        GAME_PLAYFIELD_TOP
#define RENDER_HUD_LINE_Y        (GAME_PLAYFIELD_TOP - 1)

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} RenderRect;

static const uint16_t background_colors[] = {
    ILI9341_COLOR565(5, 12, 24),
    ILI9341_COLOR565(18, 7, 25),
    ILI9341_COLOR565(4, 22, 18),
    ILI9341_COLOR565(24, 12, 4),
    ILI9341_COLOR565(8, 8, 30),
    ILI9341_COLOR565(22, 5, 12)
};

static uint8_t patch_buffer[RENDER_PATCH_MAX_WIDTH *
                            RENDER_PATCH_MAX_HEIGHT * 2U];
static int16_t previous_ball_x;
static int16_t previous_ball_y;
static uint8_t previous_active_mask;
static uint8_t renderer_initialized;

static int16_t Render_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static uint8_t Render_ActiveMask(const GameState *game) {
    uint8_t mask = 0U;

    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        if (game->level.targets[i].active != 0U) {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}

static uint16_t Render_BackgroundColor(const GameState *game) {
    uint8_t index = game->level.theme %
                    (sizeof(background_colors) /
                     sizeof(background_colors[0]));
    return background_colors[index];
}

static void Render_IncludeCircle(RenderRect *rect,
                                 int16_t x,
                                 int16_t y,
                                 int16_t radius) {
    int16_t x0 = x - radius;
    int16_t y0 = y - radius;
    int16_t x1 = x + radius;
    int16_t y1 = y + radius;

    if (x0 < rect->x0) {
        rect->x0 = x0;
    }
    if (y0 < rect->y0) {
        rect->y0 = y0;
    }
    if (x1 > rect->x1) {
        rect->x1 = x1;
    }
    if (y1 > rect->y1) {
        rect->y1 = y1;
    }
}

static void Render_ClipRect(RenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < GAME_PLAYFIELD_TOP) {
        rect->y0 = GAME_PLAYFIELD_TOP;
    }
    if (rect->x1 >= GAME_SCREEN_WIDTH) {
        rect->x1 = GAME_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= GAME_SCREEN_HEIGHT) {
        rect->y1 = GAME_SCREEN_HEIGHT - 1;
    }
}

static uint8_t Render_PointInCircle(int16_t x,
                                    int16_t y,
                                    int16_t center_x,
                                    int16_t center_y,
                                    int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;
    return ((dx * dx) + (dy * dy)) <= ((int32_t)radius * radius);
}

static uint16_t Render_StaticPixel(const GameState *game,
                                   int16_t x,
                                   int16_t y) {
    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        const GameTarget *target = &game->level.targets[i];

        if (target->active == 0U) {
            continue;
        }
        if (Render_PointInCircle(x, y, target->x, target->y,
                                 GAME_TARGET_RADIUS) != 0U) {
            if (Render_PointInCircle(x, y, target->x, target->y, 3) != 0U) {
                return ILI9341_WHITE;
            }
            return ILI9341_YELLOW;
        }
    }
    return Render_BackgroundColor(game);
}

static uint16_t Render_ComposedPixel(const GameState *game,
                                     int16_t ball_x,
                                     int16_t ball_y,
                                     int16_t x,
                                     int16_t y) {
    if (Render_PointInCircle(x, y, ball_x, ball_y,
                             BALL_RADIUS) != 0U) {
        if (Render_PointInCircle(x, y,
                                 ball_x - 5, ball_y - 5, 4) != 0U) {
            return ILI9341_WHITE;
        }
        return ILI9341_RED;
    }
    return Render_StaticPixel(game, x, y);
}

static uint8_t Render_DrawPatch(const GameState *game,
                                int16_t ball_x,
                                int16_t ball_y,
                                RenderRect rect) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    Render_ClipRect(&rect);
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);

    if ((width > RENDER_PATCH_MAX_WIDTH) ||
        (height > RENDER_PATCH_MAX_HEIGHT)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                Render_ComposedPixel(game, ball_x, ball_y, x, y);
            patch_buffer[offset++] = (uint8_t)(color >> 8);
            patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    ILI9341_DrawImage_DMA_1D((uint16_t)rect.x0,
                             (uint16_t)rect.y0,
                             width,
                             height,
                             patch_buffer);
    return 1U;
}

static void Render_DrawHud(const GameState *game) {
    char text[20];

    ILI9341_FillRectangle_DMA(0, 0, GAME_SCREEN_WIDTH,
                              RENDER_HUD_HEIGHT, ILI9341_BLACK);
    ILI9341_FillRectangle_DMA(0, RENDER_HUD_LINE_Y,
                              GAME_SCREEN_WIDTH, 1, ILI9341_GRAY);

    snprintf(text, sizeof(text), "LEVEL %u",
             (unsigned int)game->level.number);
    ILI9341_WriteString_DMA(4, 4, text, Font_11x18,
                            ILI9341_WHITE, ILI9341_BLACK);

    snprintf(text, sizeof(text), "LEFT %u",
             (unsigned int)Game_TargetsRemaining(game));
    ILI9341_WriteString_DMA(210, 4, text, Font_11x18,
                            ILI9341_YELLOW, ILI9341_BLACK);
}

static void Render_DrawWholeLevel(const GameState *game) {
    int16_t ball_x = Render_Round(game->ball.x);
    int16_t ball_y = Render_Round(game->ball.y);
    RenderRect ball_rect = {
        ball_x - BALL_RADIUS,
        ball_y - BALL_RADIUS,
        ball_x + BALL_RADIUS,
        ball_y + BALL_RADIUS
    };

    ILI9341_FillScreen_DMA(Render_BackgroundColor(game));

    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        const GameTarget *target = &game->level.targets[i];

        if (target->active != 0U) {
            ILI9341_FillCircle_DMA((uint16_t)target->x,
                                   (uint16_t)target->y,
                                   GAME_TARGET_RADIUS,
                                   ILI9341_YELLOW);
            ILI9341_FillCircle_DMA((uint16_t)target->x,
                                   (uint16_t)target->y,
                                   3,
                                   ILI9341_WHITE);
        }
    }

    Render_DrawHud(game);
    Render_DrawPatch(game, ball_x, ball_y, ball_rect);

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_active_mask = Render_ActiveMask(game);
    renderer_initialized = 1U;
}

void Render_Init(const GameState *game) {
    renderer_initialized = 0U;
    Render_DrawWholeLevel(game);
}

void Render_Frame(const GameState *game, GameEvent event) {
    int16_t ball_x;
    int16_t ball_y;
    uint8_t active_mask;
    uint8_t changed_targets;
    RenderRect dirty;

    if ((renderer_initialized == 0U) ||
        ((event & GAME_EVENT_LEVEL_STARTED) != 0U)) {
        Render_DrawWholeLevel(game);
        return;
    }

    ball_x = Render_Round(game->ball.x);
    ball_y = Render_Round(game->ball.y);
    active_mask = Render_ActiveMask(game);
    changed_targets = previous_active_mask ^ active_mask;

    if ((ball_x == previous_ball_x) &&
        (ball_y == previous_ball_y) &&
        (changed_targets == 0U)) {
        return;
    }

    dirty.x0 = previous_ball_x - BALL_RADIUS;
    dirty.y0 = previous_ball_y - BALL_RADIUS;
    dirty.x1 = previous_ball_x + BALL_RADIUS;
    dirty.y1 = previous_ball_y + BALL_RADIUS;
    Render_IncludeCircle(&dirty, ball_x, ball_y, BALL_RADIUS);

    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        if ((changed_targets & (uint8_t)(1U << i)) != 0U) {
            Render_IncludeCircle(&dirty,
                                 game->level.targets[i].x,
                                 game->level.targets[i].y,
                                 GAME_TARGET_RADIUS);
        }
    }

    if (Render_DrawPatch(game, ball_x, ball_y, dirty) == 0U) {
        Render_DrawWholeLevel(game);
        return;
    }

    if ((event & GAME_EVENT_TARGET_COLLECTED) != 0U) {
        Render_DrawHud(game);
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_active_mask = active_mask;
}
