#include "tilt_breaker_logic.h"

#include <math.h>

#define BREAKER_INPUT_FILTER          0.22f
#define BREAKER_INPUT_DEAD_ZONE       280.0f
#define BREAKER_PADDLE_ACCEL_SCALE    0.00045f
#define BREAKER_PADDLE_FRICTION       0.82f
#define BREAKER_PADDLE_MAX_SPEED      7.2f

#define BREAKER_INITIAL_BALL_SPEED    3.8f
#define BREAKER_MAX_BALL_SPEED        8.2f
#define BREAKER_MIN_VERTICAL_SPEED    2.4f
#define BREAKER_PADDLE_SPIN           0.38f
#define BREAKER_HIT_ANGLE             2.7f
#define BREAKER_HIT_ACCELERATION      1.018f

#define BREAKER_SERVE_PAUSE_TICKS     28U
#define BREAKER_LEVEL_PAUSE_TICKS     45U
#define BREAKER_GAME_OVER_TICKS       65U

static uint32_t TiltBreaker_Random(TiltBreakerState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0x7F4A7C15U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float TiltBreaker_Clamp(float value,
                               float minimum,
                               float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float TiltBreaker_DeadZone(float value) {
    if ((value > -BREAKER_INPUT_DEAD_ZONE) &&
        (value < BREAKER_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - BREAKER_INPUT_DEAD_ZONE :
           value + BREAKER_INPUT_DEAD_ZONE;
}

void TiltBreaker_GetBrickRect(uint8_t index,
                              int16_t *x,
                              int16_t *y,
                              int16_t *width,
                              int16_t *height) {
    uint8_t column = index % TILT_BREAKER_BRICK_COLUMNS;
    uint8_t row = index / TILT_BREAKER_BRICK_COLUMNS;

    *x = TILT_BREAKER_BRICK_LEFT +
         (column * (TILT_BREAKER_BRICK_WIDTH +
                    TILT_BREAKER_BRICK_GAP_X));
    *y = TILT_BREAKER_BRICK_TOP +
         (row * (TILT_BREAKER_BRICK_HEIGHT +
                 TILT_BREAKER_BRICK_GAP_Y));
    *width = TILT_BREAKER_BRICK_WIDTH;
    *height = TILT_BREAKER_BRICK_HEIGHT;
}

static void TiltBreaker_ResetPaddle(TiltBreakerState *game) {
    game->paddle.x =
        (TILT_BREAKER_SCREEN_WIDTH - TILT_BREAKER_PADDLE_WIDTH) / 2.0f;
    game->paddle.vx = 0.0f;
}

static void TiltBreaker_ParkBall(TiltBreakerState *game) {
    game->ball.x =
        game->paddle.x + (TILT_BREAKER_PADDLE_WIDTH / 2.0f);
    game->ball.y =
        TILT_BREAKER_PADDLE_Y - TILT_BREAKER_BALL_RADIUS - 2.0f;
    game->ball.vx = 0.0f;
    game->ball.vy = 0.0f;
}

static void TiltBreaker_NormalizeBallSpeed(TiltBreakerState *game,
                                           float speed) {
    float current =
        sqrtf((game->ball.vx * game->ball.vx) +
              (game->ball.vy * game->ball.vy));

    speed = TiltBreaker_Clamp(speed,
                              BREAKER_INITIAL_BALL_SPEED,
                              BREAKER_MAX_BALL_SPEED);
    if (current > 0.001f) {
        game->ball.vx = (game->ball.vx / current) * speed;
        game->ball.vy = (game->ball.vy / current) * speed;
    }

    if ((game->ball.vy > -BREAKER_MIN_VERTICAL_SPEED) &&
        (game->ball.vy < BREAKER_MIN_VERTICAL_SPEED)) {
        game->ball.vy =
            (game->ball.vy < 0.0f) ?
            -BREAKER_MIN_VERTICAL_SPEED :
            BREAKER_MIN_VERTICAL_SPEED;
    }
}

static void TiltBreaker_LaunchBall(TiltBreakerState *game) {
    float horizontal =
        ((int32_t)(TiltBreaker_Random(game) % 2001U) - 1000) / 1000.0f;

    if ((horizontal > -0.32f) && (horizontal < 0.32f)) {
        horizontal = (horizontal < 0.0f) ? -0.32f : 0.32f;
    }

    TiltBreaker_ParkBall(game);
    game->ball.vx = horizontal * 2.0f;
    game->ball.vy = -BREAKER_INITIAL_BALL_SPEED;
    game->phase = TILT_BREAKER_PHASE_PLAYING;
    game->phase_ticks = 0U;
}

static uint8_t TiltBreaker_BrickStrength(TiltBreakerState *game,
                                         uint8_t row,
                                         uint8_t column) {
    uint8_t strength = 1U;
    uint32_t random = TiltBreaker_Random(game);

    if ((game->level >= 2U) &&
        (((row + column + game->level) % 5U) == 0U)) {
        strength = 2U;
    }
    if ((game->level >= 4U) && ((random % 11U) == 0U)) {
        strength = 3U;
    }
    return strength;
}

static void TiltBreaker_GenerateLevel(TiltBreakerState *game) {
    uint8_t count = 0U;

    for (uint8_t row = 0U; row < TILT_BREAKER_BRICK_ROWS; row++) {
        for (uint8_t column = 0U;
             column < TILT_BREAKER_BRICK_COLUMNS;
             column++) {
            uint8_t index =
                (row * TILT_BREAKER_BRICK_COLUMNS) + column;
            uint8_t gap = 0U;

            if (game->level > 1U) {
                switch (game->level % 4U) {
                    case 0U:
                        gap = ((row + column) % 4U) == 0U;
                        break;
                    case 1U:
                        gap = ((column == 0U) ||
                               (column == (TILT_BREAKER_BRICK_COLUMNS - 1U))) &&
                              ((row & 1U) != 0U);
                        break;
                    case 2U:
                        gap = ((row == 2U) || (row == 3U)) &&
                              ((column == 3U) || (column == 4U));
                        break;
                    default:
                        gap = (TiltBreaker_Random(game) % 9U) == 0U;
                        break;
                }
            }

            if (gap != 0U) {
                game->bricks[index] = 0U;
            } else {
                game->bricks[index] =
                    TiltBreaker_BrickStrength(game, row, column);
                count++;
            }
        }
    }

    if (count < 20U) {
        for (uint8_t i = 0U; i < TILT_BREAKER_BRICK_COUNT; i++) {
            if (game->bricks[i] == 0U) {
                game->bricks[i] = 1U;
                count++;
                if (count >= 20U) {
                    break;
                }
            }
        }
    }
    game->bricks_remaining = count;
}

static void TiltBreaker_StartLevel(TiltBreakerState *game,
                                   uint16_t level) {
    game->level = level;
    game->changed_brick = TILT_BREAKER_NO_BRICK;
    TiltBreaker_GenerateLevel(game);
    TiltBreaker_ResetPaddle(game);
    TiltBreaker_ParkBall(game);
    game->phase = TILT_BREAKER_PHASE_LEVEL_PAUSE;
    game->phase_ticks = BREAKER_LEVEL_PAUSE_TICKS;
}

static void TiltBreaker_ResetGame(TiltBreakerState *game) {
    game->score = 0U;
    game->lives = TILT_BREAKER_STARTING_LIVES;
    game->filtered_accel_x = 0.0f;
    TiltBreaker_StartLevel(game, 1U);
}

static void TiltBreaker_UpdatePaddle(TiltBreakerState *game,
                                     int16_t accel_x) {
    float control;
    float maximum_x =
        TILT_BREAKER_SCREEN_WIDTH - TILT_BREAKER_PADDLE_WIDTH;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) * BREAKER_INPUT_FILTER;
    control = TiltBreaker_DeadZone(game->filtered_accel_x);

    game->paddle.vx += control * BREAKER_PADDLE_ACCEL_SCALE;
    game->paddle.vx *= BREAKER_PADDLE_FRICTION;
    game->paddle.vx =
        TiltBreaker_Clamp(game->paddle.vx,
                          -BREAKER_PADDLE_MAX_SPEED,
                          BREAKER_PADDLE_MAX_SPEED);
    game->paddle.x += game->paddle.vx;

    if (game->paddle.x < 0.0f) {
        game->paddle.x = 0.0f;
        game->paddle.vx = 0.0f;
    } else if (game->paddle.x > maximum_x) {
        game->paddle.x = maximum_x;
        game->paddle.vx = 0.0f;
    }
}

static void TiltBreaker_PaddleCollision(TiltBreakerState *game) {
    float ball_left = game->ball.x - TILT_BREAKER_BALL_RADIUS;
    float ball_right = game->ball.x + TILT_BREAKER_BALL_RADIUS;
    float paddle_right =
        game->paddle.x + TILT_BREAKER_PADDLE_WIDTH;
    float hit_offset;
    float speed;

    if ((game->ball.vy <= 0.0f) ||
        (ball_right < game->paddle.x) ||
        (ball_left > paddle_right) ||
        ((game->ball.y + TILT_BREAKER_BALL_RADIUS) <
         TILT_BREAKER_PADDLE_Y) ||
        ((game->ball.y - TILT_BREAKER_BALL_RADIUS) >
         (TILT_BREAKER_PADDLE_Y + TILT_BREAKER_PADDLE_HEIGHT))) {
        return;
    }

    game->ball.y =
        TILT_BREAKER_PADDLE_Y - TILT_BREAKER_BALL_RADIUS;
    game->ball.vy = -fabsf(game->ball.vy);

    hit_offset =
        (game->ball.x -
         (game->paddle.x + (TILT_BREAKER_PADDLE_WIDTH / 2.0f))) /
        (TILT_BREAKER_PADDLE_WIDTH / 2.0f);
    hit_offset = TiltBreaker_Clamp(hit_offset, -1.0f, 1.0f);
    game->ball.vx +=
        (hit_offset * BREAKER_HIT_ANGLE) +
        (game->paddle.vx * BREAKER_PADDLE_SPIN);

    speed = sqrtf((game->ball.vx * game->ball.vx) +
                  (game->ball.vy * game->ball.vy));
    TiltBreaker_NormalizeBallSpeed(game,
                                   speed * BREAKER_HIT_ACCELERATION);
}

static TiltBreakerEvent TiltBreaker_BrickCollision(
    TiltBreakerState *game) {
    float ball_left = game->ball.x - TILT_BREAKER_BALL_RADIUS;
    float ball_right = game->ball.x + TILT_BREAKER_BALL_RADIUS;
    float ball_top = game->ball.y - TILT_BREAKER_BALL_RADIUS;
    float ball_bottom = game->ball.y + TILT_BREAKER_BALL_RADIUS;

    for (uint8_t i = 0U; i < TILT_BREAKER_BRICK_COUNT; i++) {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        float center_x;
        float center_y;
        float overlap_x;
        float overlap_y;

        if (game->bricks[i] == 0U) {
            continue;
        }

        TiltBreaker_GetBrickRect(i, &x, &y, &width, &height);
        if ((ball_right < x) ||
            (ball_left > (x + width)) ||
            (ball_bottom < y) ||
            (ball_top > (y + height))) {
            continue;
        }

        center_x = x + (width / 2.0f);
        center_y = y + (height / 2.0f);
        overlap_x =
            (width / 2.0f) + TILT_BREAKER_BALL_RADIUS -
            fabsf(game->ball.x - center_x);
        overlap_y =
            (height / 2.0f) + TILT_BREAKER_BALL_RADIUS -
            fabsf(game->ball.y - center_y);

        if (overlap_x < overlap_y) {
            game->ball.vx = -game->ball.vx;
            game->ball.x =
                (game->ball.x < center_x) ?
                x - TILT_BREAKER_BALL_RADIUS :
                x + width + TILT_BREAKER_BALL_RADIUS;
        } else {
            game->ball.vy = -game->ball.vy;
            game->ball.y =
                (game->ball.y < center_y) ?
                y - TILT_BREAKER_BALL_RADIUS :
                y + height + TILT_BREAKER_BALL_RADIUS;
        }

        game->bricks[i]--;
        game->changed_brick = i;
        game->score += 10U;
        if (game->bricks[i] == 0U) {
            game->bricks_remaining--;
            game->score += 15U;
        }
        return TILT_BREAKER_EVENT_BRICK_CHANGED;
    }

    return TILT_BREAKER_EVENT_NONE;
}

void TiltBreaker_Init(TiltBreakerState *game, uint32_t seed) {
    game->rng_state = (seed != 0U) ? seed : 0xC2B2AE35U;
    TiltBreaker_ResetGame(game);
}

TiltBreakerEvent TiltBreaker_Update(TiltBreakerState *game,
                                    int16_t accel_x) {
    TiltBreakerEvent event = TILT_BREAKER_EVENT_NONE;

    game->changed_brick = TILT_BREAKER_NO_BRICK;
    TiltBreaker_UpdatePaddle(game, accel_x);

    if (game->phase != TILT_BREAKER_PHASE_PLAYING) {
        TiltBreaker_ParkBall(game);

        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }
        if (game->phase_ticks == 0U) {
            if (game->phase == TILT_BREAKER_PHASE_GAME_OVER_PAUSE) {
                TiltBreaker_ResetGame(game);
                return TILT_BREAKER_EVENT_GAME_STARTED |
                       TILT_BREAKER_EVENT_LEVEL_STARTED;
            }
            TiltBreaker_LaunchBall(game);
            return TILT_BREAKER_EVENT_ROUND_STARTED;
        }
        return event;
    }

    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    if (game->ball.x < TILT_BREAKER_BALL_RADIUS) {
        game->ball.x = TILT_BREAKER_BALL_RADIUS;
        game->ball.vx = fabsf(game->ball.vx);
    } else if (game->ball.x >
               (TILT_BREAKER_SCREEN_WIDTH -
                TILT_BREAKER_BALL_RADIUS - 1)) {
        game->ball.x =
            TILT_BREAKER_SCREEN_WIDTH - TILT_BREAKER_BALL_RADIUS - 1;
        game->ball.vx = -fabsf(game->ball.vx);
    }

    if (game->ball.y <
        (TILT_BREAKER_PLAYFIELD_TOP + TILT_BREAKER_BALL_RADIUS)) {
        game->ball.y =
            TILT_BREAKER_PLAYFIELD_TOP + TILT_BREAKER_BALL_RADIUS;
        game->ball.vy = fabsf(game->ball.vy);
    }

    TiltBreaker_PaddleCollision(game);
    event |= TiltBreaker_BrickCollision(game);

    if (game->bricks_remaining == 0U) {
        TiltBreaker_StartLevel(game, game->level + 1U);
        return TILT_BREAKER_EVENT_LEVEL_STARTED;
    }

    if (game->ball.y >
        (TILT_BREAKER_SCREEN_HEIGHT + TILT_BREAKER_BALL_RADIUS)) {
        if (game->lives > 0U) {
            game->lives--;
        }
        TiltBreaker_ParkBall(game);

        if (game->lives == 0U) {
            game->phase = TILT_BREAKER_PHASE_GAME_OVER_PAUSE;
            game->phase_ticks = BREAKER_GAME_OVER_TICKS;
        } else {
            game->phase = TILT_BREAKER_PHASE_SERVE_PAUSE;
            game->phase_ticks = BREAKER_SERVE_PAUSE_TICKS;
        }
        event |= TILT_BREAKER_EVENT_LIFE_CHANGED;
    }

    return event;
}
