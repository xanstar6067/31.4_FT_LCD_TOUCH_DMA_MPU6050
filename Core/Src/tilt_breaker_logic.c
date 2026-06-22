#include "tilt_breaker_logic.h"

#include <math.h>

#define BREAKER_INPUT_FILTER          0.22f
#define BREAKER_INPUT_DEAD_ZONE       280.0f
#define BREAKER_PADDLE_ACCEL_SCALE    0.00045f
#define BREAKER_PADDLE_FRICTION       0.82f
#define BREAKER_PADDLE_MAX_SPEED      7.8f

#define BREAKER_INITIAL_BALL_SPEED    3.3f
#define BREAKER_MAX_BALL_SPEED        6.5f
#define BREAKER_MIN_VERTICAL_SPEED    2.1f
#define BREAKER_PADDLE_SPIN           0.38f
#define BREAKER_HIT_ANGLE             2.25f
#define BREAKER_HIT_ACCELERATION      1.006f

#define BREAKER_SERVE_PAUSE_TICKS     28U
#define BREAKER_LEVEL_PAUSE_TICKS     45U
#define BREAKER_GAME_OVER_TICKS       65U

#define BREAKER_BONUS_SPEED           2.0f
#define BREAKER_BONUS_CHANCE          5U
#define BREAKER_WIDE_PADDLE_WIDTH     96U
#define BREAKER_WIDE_TICKS            240U
#define BREAKER_MAX_LIVES             6U

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
    game->paddle.width = TILT_BREAKER_PADDLE_WIDTH;
    game->paddle.x =
        (TILT_BREAKER_SCREEN_WIDTH - game->paddle.width) / 2.0f;
    game->paddle.vx = 0.0f;
}

static void TiltBreaker_ClearBonus(TiltBreakerState *game) {
    game->bonus.active = 0U;
    game->bonus.type = TILT_BREAKER_BONUS_NONE;
    game->bonus.x = 0.0f;
    game->bonus.y = 0.0f;
}

static void TiltBreaker_SetPaddleWidth(TiltBreakerState *game,
                                       uint16_t width) {
    float center =
        game->paddle.x + (game->paddle.width / 2.0f);

    game->paddle.width = width;
    game->paddle.x =
        TiltBreaker_Clamp(center - (game->paddle.width / 2.0f),
                          0.0f,
                          TILT_BREAKER_SCREEN_WIDTH -
                          game->paddle.width);
}

static void TiltBreaker_ParkBall(TiltBreakerState *game) {
    game->ball.x =
        game->paddle.x + (game->paddle.width / 2.0f);
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

static void TiltBreaker_SpawnBonus(TiltBreakerState *game,
                                   uint8_t brick_index) {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    if ((game->bonus.active != 0U) ||
        ((TiltBreaker_Random(game) % BREAKER_BONUS_CHANCE) != 0U)) {
        return;
    }

    TiltBreaker_GetBrickRect(brick_index, &x, &y, &width, &height);
    game->bonus.x =
        x + (width / 2.0f) - (TILT_BREAKER_BONUS_WIDTH / 2.0f);
    game->bonus.y =
        y + (height / 2.0f) - (TILT_BREAKER_BONUS_HEIGHT / 2.0f);
    game->bonus.type =
        (TiltBreakerBonusType)(1U + (TiltBreaker_Random(game) % 3U));
    game->bonus.active = 1U;
}

static void TiltBreaker_ApplyBonus(TiltBreakerState *game,
                                   TiltBreakerBonusType type) {
    float speed;

    switch (type) {
        case TILT_BREAKER_BONUS_WIDE:
            TiltBreaker_SetPaddleWidth(game, BREAKER_WIDE_PADDLE_WIDTH);
            game->wide_ticks = BREAKER_WIDE_TICKS;
            break;

        case TILT_BREAKER_BONUS_SLOW:
            speed = sqrtf((game->ball.vx * game->ball.vx) +
                          (game->ball.vy * game->ball.vy));
            TiltBreaker_NormalizeBallSpeed(game, speed * 0.72f);
            break;

        case TILT_BREAKER_BONUS_LIFE:
            if (game->lives < BREAKER_MAX_LIVES) {
                game->lives++;
            }
            break;

        default:
            break;
    }
}

static TiltBreakerEvent TiltBreaker_UpdateBonus(TiltBreakerState *game) {
    float bonus_left;
    float bonus_right;
    float bonus_bottom;
    float paddle_right;
    TiltBreakerEvent event = TILT_BREAKER_EVENT_NONE;

    if (game->wide_ticks > 0U) {
        game->wide_ticks--;
        if (game->wide_ticks == 0U) {
            TiltBreaker_SetPaddleWidth(game, TILT_BREAKER_PADDLE_WIDTH);
            event |= TILT_BREAKER_EVENT_BONUS_CHANGED;
        }
    }

    if (game->bonus.active == 0U) {
        return event;
    }

    game->bonus.y += BREAKER_BONUS_SPEED;
    bonus_left = game->bonus.x;
    bonus_right = game->bonus.x + TILT_BREAKER_BONUS_WIDTH;
    bonus_bottom = game->bonus.y + TILT_BREAKER_BONUS_HEIGHT;
    paddle_right = game->paddle.x + game->paddle.width;

    if ((bonus_bottom >= TILT_BREAKER_PADDLE_Y) &&
        (game->bonus.y <= (TILT_BREAKER_PADDLE_Y +
                           TILT_BREAKER_PADDLE_HEIGHT)) &&
        (bonus_right >= game->paddle.x) &&
        (bonus_left <= paddle_right)) {
        TiltBreakerBonusType type = game->bonus.type;

        TiltBreaker_ApplyBonus(game, type);
        TiltBreaker_ClearBonus(game);
        event |= TILT_BREAKER_EVENT_BONUS_CHANGED;
        if (type == TILT_BREAKER_BONUS_LIFE) {
            event |= TILT_BREAKER_EVENT_LIFE_CHANGED;
        }
        return event;
    }

    if (game->bonus.y > TILT_BREAKER_SCREEN_HEIGHT) {
        TiltBreaker_ClearBonus(game);
        return event | TILT_BREAKER_EVENT_BONUS_CHANGED;
    }

    return event | TILT_BREAKER_EVENT_BONUS_CHANGED;
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
    game->wide_ticks = 0U;
    TiltBreaker_ClearBonus(game);
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
    game->wide_ticks = 0U;
    TiltBreaker_ClearBonus(game);
    TiltBreaker_StartLevel(game, 1U);
}

static void TiltBreaker_UpdatePaddle(TiltBreakerState *game,
                                     int16_t accel_x) {
    float control;
    float maximum_x =
        TILT_BREAKER_SCREEN_WIDTH - game->paddle.width;

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
        game->paddle.x + game->paddle.width;
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
         (game->paddle.x + (game->paddle.width / 2.0f))) /
        (game->paddle.width / 2.0f);
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
            TiltBreaker_SpawnBonus(game, i);
        }
        return TILT_BREAKER_EVENT_BRICK_CHANGED |
               ((game->bonus.active != 0U) ?
                TILT_BREAKER_EVENT_BONUS_CHANGED :
                TILT_BREAKER_EVENT_NONE);
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
    event |= TiltBreaker_UpdateBonus(game);
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
        TiltBreaker_ClearBonus(game);
        game->wide_ticks = 0U;
        TiltBreaker_SetPaddleWidth(game, TILT_BREAKER_PADDLE_WIDTH);
        TiltBreaker_ParkBall(game);

        if (game->lives == 0U) {
            game->phase = TILT_BREAKER_PHASE_GAME_OVER_PAUSE;
            game->phase_ticks = BREAKER_GAME_OVER_TICKS;
        } else {
            game->phase = TILT_BREAKER_PHASE_SERVE_PAUSE;
            game->phase_ticks = BREAKER_SERVE_PAUSE_TICKS;
        }
        event |= TILT_BREAKER_EVENT_LIFE_CHANGED |
                 TILT_BREAKER_EVENT_BONUS_CHANGED;
    }

    return event;
}
