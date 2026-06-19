#include "gravity_pong_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define PONG_RENDER_PATCH_WIDTH      96
#define PONG_RENDER_PATCH_HEIGHT     32
#define PONG_RENDER_CORE_GLOW        14

#define PONG_BACKGROUND_COLOR        ILI9341_COLOR565(3, 8, 18)
#define PONG_CENTER_LINE_COLOR       ILI9341_COLOR565(18, 35, 52)
#define PONG_CORE_OUTER_COLOR        ILI9341_PURPLE
#define PONG_CORE_INNER_COLOR        ILI9341_DARK_BLUE
#define PONG_PLAYER_COLOR            ILI9341_GREEN
#define PONG_AI_COLOR                ILI9341_ORANGE
#define PONG_BALL_COLOR              ILI9341_CYAN

#if (PONG_RENDER_PATCH_WIDTH * PONG_RENDER_PATCH_HEIGHT * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for gravity pong patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} PongRenderRect;

#define pong_patch_buffer render_scratch_buffer
static int16_t previous_ball_x;
static int16_t previous_ball_y;
static int16_t previous_player_x;
static int16_t previous_ai_x;
static uint8_t pong_renderer_initialized;

static int16_t PongRender_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static uint8_t PongRender_PointInCircle(int16_t x,
                                        int16_t y,
                                        int16_t center_x,
                                        int16_t center_y,
                                        int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;
    return ((dx * dx) + (dy * dy)) <= ((int32_t)radius * radius);
}

static uint8_t PongRender_PointInPaddle(int16_t x,
                                        int16_t y,
                                        int16_t paddle_x,
                                        int16_t paddle_y,
                                        uint16_t paddle_width) {
    return (x >= paddle_x) &&
           (x < (paddle_x + (int16_t)paddle_width)) &&
           (y >= paddle_y) &&
           (y < (paddle_y + GRAVITY_PONG_PADDLE_HEIGHT));
}

static uint16_t PongRender_StaticPixel(int16_t x, int16_t y) {
    if (PongRender_PointInCircle(x, y,
                                 GRAVITY_PONG_CORE_X,
                                 GRAVITY_PONG_CORE_Y,
                                 PONG_RENDER_CORE_GLOW) != 0U) {
        if (PongRender_PointInCircle(x, y,
                                     GRAVITY_PONG_CORE_X,
                                     GRAVITY_PONG_CORE_Y,
                                     4) != 0U) {
            return ILI9341_WHITE;
        }
        if (PongRender_PointInCircle(x, y,
                                     GRAVITY_PONG_CORE_X,
                                     GRAVITY_PONG_CORE_Y,
                                     GRAVITY_PONG_CORE_RADIUS) != 0U) {
            return PONG_CORE_INNER_COLOR;
        }
        return PONG_CORE_OUTER_COLOR;
    }

    if ((x >= (GRAVITY_PONG_SCREEN_WIDTH / 2 - 1)) &&
        (x <= (GRAVITY_PONG_SCREEN_WIDTH / 2)) &&
        ((((y - GRAVITY_PONG_PLAYFIELD_TOP) / 6) & 1) == 0)) {
        return PONG_CENTER_LINE_COLOR;
    }

    return PONG_BACKGROUND_COLOR;
}

static uint16_t PongRender_ComposedPixel(const GravityPongState *game,
                                         int16_t ball_x,
                                         int16_t ball_y,
                                         int16_t player_x,
                                         int16_t ai_x,
                                         int16_t x,
                                         int16_t y) {
    if (PongRender_PointInCircle(x, y, ball_x, ball_y,
                                 GRAVITY_PONG_BALL_RADIUS) != 0U) {
        if (PongRender_PointInCircle(x, y,
                                     ball_x - 2, ball_y - 2, 1) != 0U) {
            return ILI9341_WHITE;
        }
        return PONG_BALL_COLOR;
    }

    if (PongRender_PointInPaddle(x, y,
                                 player_x,
                                 GRAVITY_PONG_PLAYER_Y,
                                 game->player.width) != 0U) {
        return PONG_PLAYER_COLOR;
    }

    if (PongRender_PointInPaddle(x, y,
                                 ai_x,
                                 GRAVITY_PONG_AI_Y,
                                 game->ai.width) != 0U) {
        return PONG_AI_COLOR;
    }

    return PongRender_StaticPixel(x, y);
}

static void PongRender_ClipRect(PongRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < GRAVITY_PONG_PLAYFIELD_TOP) {
        rect->y0 = GRAVITY_PONG_PLAYFIELD_TOP;
    }
    if (rect->x1 >= GRAVITY_PONG_SCREEN_WIDTH) {
        rect->x1 = GRAVITY_PONG_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= GRAVITY_PONG_SCREEN_HEIGHT) {
        rect->y1 = GRAVITY_PONG_SCREEN_HEIGHT - 1;
    }
}

static uint8_t PongRender_DrawPatch(const GravityPongState *game,
                                    PongRenderRect rect,
                                    int16_t ball_x,
                                    int16_t ball_y,
                                    int16_t player_x,
                                    int16_t ai_x) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    PongRender_ClipRect(&rect);
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);

    if ((width > PONG_RENDER_PATCH_WIDTH) ||
        (height > PONG_RENDER_PATCH_HEIGHT)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                PongRender_ComposedPixel(game,
                                         ball_x,
                                         ball_y,
                                         player_x,
                                         ai_x,
                                         x,
                                         y);
            pong_patch_buffer[offset++] = (uint8_t)(color >> 8);
            pong_patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    ILI9341_DrawImage_DMA_1D((uint16_t)rect.x0,
                             (uint16_t)rect.y0,
                             width,
                             height,
                             pong_patch_buffer);
    return 1U;
}

static void PongRender_DrawHud(const GravityPongState *game) {
    char score[16];

    ILI9341_FillRectangle_DMA(0, 0,
                              GRAVITY_PONG_SCREEN_WIDTH,
                              GRAVITY_PONG_PLAYFIELD_TOP,
                              ILI9341_BLACK);
    ILI9341_FillRectangle_DMA(0,
                              GRAVITY_PONG_PLAYFIELD_TOP - 1,
                              GRAVITY_PONG_SCREEN_WIDTH,
                              1,
                              ILI9341_GRAY);
    ILI9341_WriteString_DMA(4, 4, "GRAVITY PONG", Font_11x18,
                            ILI9341_WHITE, ILI9341_BLACK);

    snprintf(score, sizeof(score), "YOU %u:%u CPU",
             (unsigned int)game->player_score,
             (unsigned int)game->ai_score);
    ILI9341_WriteString_DMA(188, 4, score, Font_11x18,
                            ILI9341_YELLOW, ILI9341_BLACK);
}

static void PongRender_DrawStaticField(void) {
    ILI9341_FillScreen_DMA(PONG_BACKGROUND_COLOR);

    for (uint16_t y = GRAVITY_PONG_PLAYFIELD_TOP;
         y < GRAVITY_PONG_SCREEN_HEIGHT;
         y += 12U) {
        ILI9341_FillRectangle_DMA(
            GRAVITY_PONG_SCREEN_WIDTH / 2 - 1,
            y,
            2,
            6,
            PONG_CENTER_LINE_COLOR);
    }

    ILI9341_FillCircle_DMA(GRAVITY_PONG_CORE_X,
                           GRAVITY_PONG_CORE_Y,
                           PONG_RENDER_CORE_GLOW,
                           PONG_CORE_OUTER_COLOR);
    ILI9341_FillCircle_DMA(GRAVITY_PONG_CORE_X,
                           GRAVITY_PONG_CORE_Y,
                           GRAVITY_PONG_CORE_RADIUS,
                           PONG_CORE_INNER_COLOR);
    ILI9341_FillCircle_DMA(GRAVITY_PONG_CORE_X,
                           GRAVITY_PONG_CORE_Y,
                           4,
                           ILI9341_WHITE);
}

static void PongRender_DrawPauseMessage(const GravityPongState *game) {
    const char *message = "READY";
    uint16_t x = 120U;
    uint16_t color = ILI9341_CYAN;

    if (game->phase == GRAVITY_PONG_PHASE_MATCH_PAUSE) {
        if (game->match_winner == 1U) {
            message = "YOU WIN";
            x = 104U;
            color = ILI9341_GREEN;
        } else {
            message = "CPU WINS";
            x = 96U;
            color = ILI9341_ORANGE;
        }
    }

    ILI9341_WriteString_DMA(x, 82, message, Font_16x26,
                            color, PONG_BACKGROUND_COLOR);
}

static void PongRender_DrawWholeField(const GravityPongState *game) {
    int16_t ball_x = PongRender_Round(game->ball.x);
    int16_t ball_y = PongRender_Round(game->ball.y);
    int16_t player_x = PongRender_Round(game->player.x);
    int16_t ai_x = PongRender_Round(game->ai.x);

    PongRender_DrawStaticField();
    PongRender_DrawHud(game);

    ILI9341_FillRectangle_DMA((uint16_t)player_x,
                              GRAVITY_PONG_PLAYER_Y,
                              game->player.width,
                              GRAVITY_PONG_PADDLE_HEIGHT,
                              PONG_PLAYER_COLOR);
    ILI9341_FillRectangle_DMA((uint16_t)ai_x,
                              GRAVITY_PONG_AI_Y,
                              game->ai.width,
                              GRAVITY_PONG_PADDLE_HEIGHT,
                              PONG_AI_COLOR);
    ILI9341_FillCircle_DMA((uint16_t)ball_x,
                           (uint16_t)ball_y,
                           GRAVITY_PONG_BALL_RADIUS,
                           PONG_BALL_COLOR);

    if (game->phase != GRAVITY_PONG_PHASE_PLAYING) {
        PongRender_DrawPauseMessage(game);
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_player_x = player_x;
    previous_ai_x = ai_x;
    pong_renderer_initialized = 1U;
}

void GravityPongRender_Init(const GravityPongState *game) {
    pong_renderer_initialized = 0U;
    PongRender_DrawWholeField(game);
}

void GravityPongRender_Frame(const GravityPongState *game,
                             GravityPongEvent event) {
    int16_t ball_x;
    int16_t ball_y;
    int16_t player_x;
    int16_t ai_x;
    PongRenderRect rect;

    if ((pong_renderer_initialized == 0U) ||
        ((event & (GRAVITY_PONG_EVENT_SCORE_CHANGED |
                   GRAVITY_PONG_EVENT_ROUND_STARTED |
                   GRAVITY_PONG_EVENT_MATCH_STARTED)) != 0U)) {
        PongRender_DrawWholeField(game);
        return;
    }

    if (game->phase != GRAVITY_PONG_PHASE_PLAYING) {
        return;
    }

    ball_x = PongRender_Round(game->ball.x);
    ball_y = PongRender_Round(game->ball.y);
    player_x = PongRender_Round(game->player.x);
    ai_x = PongRender_Round(game->ai.x);

    if (player_x != previous_player_x) {
        rect.x0 = (player_x < previous_player_x) ?
                  player_x - 1 : previous_player_x - 1;
        rect.x1 = ((player_x > previous_player_x) ?
                  player_x : previous_player_x) +
                  (int16_t)game->player.width;
        rect.y0 = GRAVITY_PONG_PLAYER_Y - 1;
        rect.y1 = GRAVITY_PONG_PLAYER_Y +
                  GRAVITY_PONG_PADDLE_HEIGHT;
        if (PongRender_DrawPatch(game, rect,
                                 ball_x, ball_y,
                                 player_x, ai_x) == 0U) {
            PongRender_DrawWholeField(game);
            return;
        }
    }

    if (ai_x != previous_ai_x) {
        rect.x0 = (ai_x < previous_ai_x) ? ai_x - 1 : previous_ai_x - 1;
        rect.x1 = ((ai_x > previous_ai_x) ? ai_x : previous_ai_x) +
                  (int16_t)game->ai.width;
        rect.y0 = GRAVITY_PONG_AI_Y - 1;
        rect.y1 = GRAVITY_PONG_AI_Y + GRAVITY_PONG_PADDLE_HEIGHT;
        if (PongRender_DrawPatch(game, rect,
                                 ball_x, ball_y,
                                 player_x, ai_x) == 0U) {
            PongRender_DrawWholeField(game);
            return;
        }
    }

    if ((ball_x != previous_ball_x) || (ball_y != previous_ball_y)) {
        rect.x0 = ((ball_x < previous_ball_x) ?
                  ball_x : previous_ball_x) -
                  GRAVITY_PONG_BALL_RADIUS - 1;
        rect.x1 = ((ball_x > previous_ball_x) ?
                  ball_x : previous_ball_x) +
                  GRAVITY_PONG_BALL_RADIUS + 1;
        rect.y0 = ((ball_y < previous_ball_y) ?
                  ball_y : previous_ball_y) -
                  GRAVITY_PONG_BALL_RADIUS - 1;
        rect.y1 = ((ball_y > previous_ball_y) ?
                  ball_y : previous_ball_y) +
                  GRAVITY_PONG_BALL_RADIUS + 1;
        if (PongRender_DrawPatch(game, rect,
                                 ball_x, ball_y,
                                 player_x, ai_x) == 0U) {
            PongRender_DrawWholeField(game);
            return;
        }
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    previous_player_x = player_x;
    previous_ai_x = ai_x;
}
