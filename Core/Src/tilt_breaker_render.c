#include "tilt_breaker_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define BREAKER_RENDER_PATCH_WIDTH      RENDER_SCRATCH_PATCH_WIDTH
#define BREAKER_RENDER_PATCH_HEIGHT     RENDER_SCRATCH_PATCH_HEIGHT

#define BREAKER_BACKGROUND_COLOR        DISPLAY_COLOR565(3, 9, 14)
#define BREAKER_PADDLE_COLOR            DISPLAY_GREEN
#define BREAKER_BALL_COLOR              DISPLAY_CYAN
#define BREAKER_BRICK_ONE_COLOR         DISPLAY_BLUE
#define BREAKER_BRICK_TWO_COLOR         DISPLAY_ORANGE
#define BREAKER_BRICK_THREE_COLOR       DISPLAY_MAGENTA
#define BREAKER_BONUS_WIDE_COLOR        DISPLAY_GREEN
#define BREAKER_BONUS_SLOW_COLOR        DISPLAY_CYAN
#define BREAKER_BONUS_LIFE_COLOR        DISPLAY_PINK

#if (BREAKER_RENDER_PATCH_WIDTH * BREAKER_RENDER_PATCH_HEIGHT * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for tilt breaker patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} BreakerRenderRect;

#define breaker_patch_buffer render_scratch_buffer
static int16_t previous_ball_x;
static int16_t previous_ball_y;
static int16_t previous_paddle_x;
static uint16_t previous_paddle_width;
static int16_t previous_bonus_x;
static int16_t previous_bonus_y;
static uint8_t previous_bonus_active;
static uint8_t breaker_renderer_initialized;

static int16_t BreakerRender_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static uint8_t BreakerRender_PointInCircle(int16_t x,
                                           int16_t y,
                                           int16_t center_x,
                                           int16_t center_y,
                                           int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;
    return ((dx * dx) + (dy * dy)) <= ((int32_t)radius * radius);
}

static uint16_t BreakerRender_BrickColor(uint8_t strength) {
    if (strength >= 3U) {
        return BREAKER_BRICK_THREE_COLOR;
    }
    if (strength == 2U) {
        return BREAKER_BRICK_TWO_COLOR;
    }
    return BREAKER_BRICK_ONE_COLOR;
}

static uint16_t BreakerRender_BonusColor(TiltBreakerBonusType type) {
    switch (type) {
        case TILT_BREAKER_BONUS_WIDE:
            return BREAKER_BONUS_WIDE_COLOR;
        case TILT_BREAKER_BONUS_SLOW:
            return BREAKER_BONUS_SLOW_COLOR;
        case TILT_BREAKER_BONUS_LIFE:
            return BREAKER_BONUS_LIFE_COLOR;
        default:
            return DISPLAY_WHITE;
    }
}

static const char *BreakerRender_BonusLabel(TiltBreakerBonusType type) {
    switch (type) {
        case TILT_BREAKER_BONUS_WIDE:
            return "W";
        case TILT_BREAKER_BONUS_SLOW:
            return "S";
        case TILT_BREAKER_BONUS_LIFE:
            return "+";
        default:
            return "?";
    }
}

static uint16_t BreakerRender_StaticPixel(const TiltBreakerState *game,
                                          int16_t x,
                                          int16_t y) {
    if ((y >= TILT_BREAKER_BRICK_TOP) &&
        (y < (TILT_BREAKER_BRICK_TOP +
              TILT_BREAKER_BRICK_ROWS *
              (TILT_BREAKER_BRICK_HEIGHT +
               TILT_BREAKER_BRICK_GAP_Y)))) {
        for (uint8_t i = 0U; i < TILT_BREAKER_BRICK_COUNT; i++) {
            int16_t brick_x;
            int16_t brick_y;
            int16_t width;
            int16_t height;

            if (game->bricks[i] == 0U) {
                continue;
            }

            TiltBreaker_GetBrickRect(i,
                                     &brick_x,
                                     &brick_y,
                                     &width,
                                     &height);
            if ((x > brick_x) &&
                (x < (brick_x + width - 1)) &&
                (y > brick_y) &&
                (y < (brick_y + height - 1))) {
                return BreakerRender_BrickColor(game->bricks[i]);
            }
        }
    }
    return BREAKER_BACKGROUND_COLOR;
}

static uint16_t BreakerRender_ComposedPixel(
    const TiltBreakerState *game,
    int16_t ball_x,
    int16_t ball_y,
    int16_t paddle_x,
    int16_t x,
    int16_t y) {
    if (BreakerRender_PointInCircle(x, y,
                                    ball_x, ball_y,
                                    TILT_BREAKER_BALL_RADIUS) != 0U) {
        if (BreakerRender_PointInCircle(x, y,
                                        ball_x - 2,
                                        ball_y - 2,
                                        1) != 0U) {
            return DISPLAY_WHITE;
        }
        return BREAKER_BALL_COLOR;
    }

    if ((game->bonus.active != 0U) &&
        (x >= (int16_t)game->bonus.x) &&
        (x < ((int16_t)game->bonus.x + TILT_BREAKER_BONUS_WIDTH)) &&
        (y >= (int16_t)game->bonus.y) &&
        (y < ((int16_t)game->bonus.y +
              TILT_BREAKER_BONUS_HEIGHT))) {
        return BreakerRender_BonusColor(game->bonus.type);
    }

    if ((x >= paddle_x) &&
        (x < (paddle_x + (int16_t)game->paddle.width)) &&
        (y >= TILT_BREAKER_PADDLE_Y) &&
        (y < (TILT_BREAKER_PADDLE_Y +
              TILT_BREAKER_PADDLE_HEIGHT))) {
        return BREAKER_PADDLE_COLOR;
    }

    return BreakerRender_StaticPixel(game, x, y);
}

static void BreakerRender_ClipRect(BreakerRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < TILT_BREAKER_PLAYFIELD_TOP) {
        rect->y0 = TILT_BREAKER_PLAYFIELD_TOP;
    }
    if (rect->x1 >= TILT_BREAKER_SCREEN_WIDTH) {
        rect->x1 = TILT_BREAKER_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= TILT_BREAKER_SCREEN_HEIGHT) {
        rect->y1 = TILT_BREAKER_SCREEN_HEIGHT - 1;
    }
}

static uint8_t BreakerRender_DrawPatch(const TiltBreakerState *game,
                                       BreakerRenderRect rect,
                                       int16_t ball_x,
                                       int16_t ball_y,
                                       int16_t paddle_x) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    BreakerRender_ClipRect(&rect);
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);

    if ((width > BREAKER_RENDER_PATCH_WIDTH) ||
        (height > BREAKER_RENDER_PATCH_HEIGHT)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                BreakerRender_ComposedPixel(game,
                                            ball_x,
                                            ball_y,
                                            paddle_x,
                                            x,
                                            y);
            breaker_patch_buffer[offset++] = (uint8_t)(color >> 8);
            breaker_patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    DISPLAY_DrawImage_DMA_1D((uint16_t)rect.x0,
                             (uint16_t)rect.y0,
                             width,
                             height,
                             breaker_patch_buffer);
    return 1U;
}

static void BreakerRender_DrawBonusLabel(const TiltBreakerState *game) {
    if (game->bonus.active == 0U) {
        return;
    }

    DISPLAY_WriteString_DMA((uint16_t)game->bonus.x + 4U,
                            (uint16_t)game->bonus.y - 1U,
                            BreakerRender_BonusLabel(game->bonus.type),
                            Font_7x10,
                            DISPLAY_BLACK,
                            BreakerRender_BonusColor(game->bonus.type));
}

static void BreakerRender_DrawHud(const TiltBreakerState *game) {
    char status[24];

    DISPLAY_FillRectangle_DMA(0, 0,
                              TILT_BREAKER_SCREEN_WIDTH,
                              TILT_BREAKER_PLAYFIELD_TOP,
                              DISPLAY_BLACK);
    DISPLAY_FillRectangle_DMA(0,
                              TILT_BREAKER_PLAYFIELD_TOP - 1,
                              TILT_BREAKER_SCREEN_WIDTH,
                              1,
                              DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(4, 4, "TILT BREAKER", Font_11x18,
                            DISPLAY_WHITE, DISPLAY_BLACK);

    snprintf(status, sizeof(status), "L%u B%u X%u",
             (unsigned int)game->level,
             (unsigned int)game->bricks_remaining,
             (unsigned int)game->lives);
    DISPLAY_WriteString_DMA(210, 9, status, Font_7x10,
                            DISPLAY_YELLOW, DISPLAY_BLACK);
}

static void BreakerRender_DrawBricks(const TiltBreakerState *game) {
    for (uint8_t i = 0U; i < TILT_BREAKER_BRICK_COUNT; i++) {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;

        if (game->bricks[i] == 0U) {
            continue;
        }

        TiltBreaker_GetBrickRect(i, &x, &y, &width, &height);
        DISPLAY_FillRectangle_DMA((uint16_t)(x + 1),
                                  (uint16_t)(y + 1),
                                  (uint16_t)(width - 2),
                                  (uint16_t)(height - 2),
                                  BreakerRender_BrickColor(
                                      game->bricks[i]));
    }
}

static void BreakerRender_DrawPauseMessage(const TiltBreakerState *game) {
    const char *message = "READY";
    uint16_t x = 120U;
    uint16_t color = DISPLAY_CYAN;

    if (game->phase == TILT_BREAKER_PHASE_LEVEL_PAUSE) {
        message = "NEW LEVEL";
        x = 88U;
        color = DISPLAY_GREEN;
    } else if (game->phase == TILT_BREAKER_PHASE_GAME_OVER_PAUSE) {
        message = "GAME OVER";
        x = 88U;
        color = DISPLAY_ORANGE;
    }

    DISPLAY_WriteString_DMA(x, 174, message, Font_16x26,
                            color, BREAKER_BACKGROUND_COLOR);
}

static void BreakerRender_DrawWholeField(const TiltBreakerState *game) {
    int16_t ball_x = BreakerRender_Round(game->ball.x);
    int16_t ball_y = BreakerRender_Round(game->ball.y);
    int16_t paddle_x = BreakerRender_Round(game->paddle.x);

    DISPLAY_FillScreen_DMA(BREAKER_BACKGROUND_COLOR);
    BreakerRender_DrawBricks(game);
    BreakerRender_DrawHud(game);

    DISPLAY_FillRectangle_DMA((uint16_t)paddle_x,
                              TILT_BREAKER_PADDLE_Y,
                              game->paddle.width,
                              TILT_BREAKER_PADDLE_HEIGHT,
                              BREAKER_PADDLE_COLOR);
    if (game->bonus.active != 0U) {
        DISPLAY_FillRectangle_DMA((uint16_t)game->bonus.x,
                                  (uint16_t)game->bonus.y,
                                  TILT_BREAKER_BONUS_WIDTH,
                                  TILT_BREAKER_BONUS_HEIGHT,
                                  BreakerRender_BonusColor(
                                      game->bonus.type));
        BreakerRender_DrawBonusLabel(game);
    }
    DISPLAY_FillCircle_DMA((uint16_t)ball_x,
                           (uint16_t)ball_y,
                           TILT_BREAKER_BALL_RADIUS,
                           BREAKER_BALL_COLOR);

    if (game->phase != TILT_BREAKER_PHASE_PLAYING) {
        BreakerRender_DrawPauseMessage(game);
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_paddle_x = paddle_x;
    previous_paddle_width = game->paddle.width;
    previous_bonus_x = (int16_t)game->bonus.x;
    previous_bonus_y = (int16_t)game->bonus.y;
    previous_bonus_active = game->bonus.active;
    breaker_renderer_initialized = 1U;
}

void TiltBreakerRender_Init(const TiltBreakerState *game) {
    breaker_renderer_initialized = 0U;
    BreakerRender_DrawWholeField(game);
}

void TiltBreakerRender_Frame(const TiltBreakerState *game,
                             TiltBreakerEvent event) {
    int16_t ball_x;
    int16_t ball_y;
    int16_t paddle_x;
    BreakerRenderRect rect;

    if ((breaker_renderer_initialized == 0U) ||
        ((event & (TILT_BREAKER_EVENT_ROUND_STARTED |
                   TILT_BREAKER_EVENT_LEVEL_STARTED |
                   TILT_BREAKER_EVENT_GAME_STARTED)) != 0U) ||
        (((event & TILT_BREAKER_EVENT_LIFE_CHANGED) != 0U) &&
         (game->phase != TILT_BREAKER_PHASE_PLAYING))) {
        BreakerRender_DrawWholeField(game);
        return;
    }

    ball_x = BreakerRender_Round(game->ball.x);
    ball_y = BreakerRender_Round(game->ball.y);
    paddle_x = BreakerRender_Round(game->paddle.x);

    if ((event & TILT_BREAKER_EVENT_BRICK_CHANGED) != 0U) {
        int16_t width;
        int16_t height;

        TiltBreaker_GetBrickRect(game->changed_brick,
                                 &rect.x0,
                                 &rect.y0,
                                 &width,
                                 &height);
        rect.x0--;
        rect.y0--;
        rect.x1 = rect.x0 + width + 1;
        rect.y1 = rect.y0 + height + 1;
        if (BreakerRender_DrawPatch(game, rect,
                                    ball_x, ball_y,
                                    paddle_x) == 0U) {
            BreakerRender_DrawWholeField(game);
            return;
        }
        BreakerRender_DrawHud(game);
    }

    if ((paddle_x != previous_paddle_x) ||
        (game->paddle.width != previous_paddle_width)) {
        rect.x0 = (paddle_x < previous_paddle_x) ?
                  paddle_x - 1 : previous_paddle_x - 1;
        rect.x1 =
            ((paddle_x + (int16_t)game->paddle.width) >
             (previous_paddle_x + (int16_t)previous_paddle_width)) ?
            (paddle_x + (int16_t)game->paddle.width) :
            (previous_paddle_x + (int16_t)previous_paddle_width);
        rect.y0 = TILT_BREAKER_PADDLE_Y - 1;
        rect.y1 = TILT_BREAKER_PADDLE_Y +
                  TILT_BREAKER_PADDLE_HEIGHT;
        if (BreakerRender_DrawPatch(game, rect,
                                    ball_x, ball_y,
                                    paddle_x) == 0U) {
            BreakerRender_DrawWholeField(game);
            return;
        }
    }

    if ((event & TILT_BREAKER_EVENT_BONUS_CHANGED) != 0U) {
        int16_t bonus_x = (int16_t)game->bonus.x;
        int16_t bonus_y = (int16_t)game->bonus.y;

        if ((previous_bonus_active != 0U) ||
            (game->bonus.active != 0U)) {
            if (previous_bonus_active != 0U) {
                rect.x0 = previous_bonus_x;
                rect.y0 = previous_bonus_y;
                rect.x1 = previous_bonus_x + TILT_BREAKER_BONUS_WIDTH;
                rect.y1 = previous_bonus_y + TILT_BREAKER_BONUS_HEIGHT;
            } else {
                rect.x0 = bonus_x;
                rect.y0 = bonus_y;
                rect.x1 = bonus_x + TILT_BREAKER_BONUS_WIDTH;
                rect.y1 = bonus_y + TILT_BREAKER_BONUS_HEIGHT;
            }

            if ((game->bonus.active != 0U) &&
                (previous_bonus_active != 0U)) {
                if (bonus_x < rect.x0) {
                    rect.x0 = bonus_x;
                }
                if (bonus_y < rect.y0) {
                    rect.y0 = bonus_y;
                }
                if ((bonus_x + TILT_BREAKER_BONUS_WIDTH) > rect.x1) {
                    rect.x1 = bonus_x + TILT_BREAKER_BONUS_WIDTH;
                }
                if ((bonus_y + TILT_BREAKER_BONUS_HEIGHT) > rect.y1) {
                    rect.y1 = bonus_y + TILT_BREAKER_BONUS_HEIGHT;
                }
            }

            rect.x0--;
            rect.y0--;
            rect.x1++;
            rect.y1++;
            if (BreakerRender_DrawPatch(game, rect,
                                        ball_x, ball_y,
                                        paddle_x) == 0U) {
                BreakerRender_DrawWholeField(game);
                return;
            }
            BreakerRender_DrawBonusLabel(game);
        }
    }

    if ((event & TILT_BREAKER_EVENT_LIFE_CHANGED) != 0U) {
        BreakerRender_DrawHud(game);
    }

    if ((ball_x != previous_ball_x) || (ball_y != previous_ball_y)) {
        rect.x0 = ((ball_x < previous_ball_x) ?
                  ball_x : previous_ball_x) -
                  TILT_BREAKER_BALL_RADIUS - 1;
        rect.x1 = ((ball_x > previous_ball_x) ?
                  ball_x : previous_ball_x) +
                  TILT_BREAKER_BALL_RADIUS + 1;
        rect.y0 = ((ball_y < previous_ball_y) ?
                  ball_y : previous_ball_y) -
                  TILT_BREAKER_BALL_RADIUS - 1;
        rect.y1 = ((ball_y > previous_ball_y) ?
                  ball_y : previous_ball_y) +
                  TILT_BREAKER_BALL_RADIUS + 1;
        if (BreakerRender_DrawPatch(game, rect,
                                    ball_x, ball_y,
                                    paddle_x) == 0U) {
            BreakerRender_DrawWholeField(game);
            return;
        }
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_paddle_x = paddle_x;
    previous_paddle_width = game->paddle.width;
    previous_bonus_x = (int16_t)game->bonus.x;
    previous_bonus_y = (int16_t)game->bonus.y;
    previous_bonus_active = game->bonus.active;
}
