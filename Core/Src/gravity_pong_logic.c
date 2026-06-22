#include "gravity_pong_logic.h"

#include <math.h>

#define PONG_INPUT_FILTER          0.22f
#define PONG_INPUT_DEAD_ZONE       280.0f
#define PONG_PLAYER_ACCEL_SCALE    0.00045f
#define PONG_PLAYER_FRICTION       0.82f
#define PONG_PLAYER_MAX_SPEED      7.0f
#define PONG_AI_MAX_SPEED          3.4f

#define PONG_INITIAL_BALL_SPEED    3.6f
#define PONG_MAX_BALL_SPEED        8.4f
#define PONG_MIN_VERTICAL_SPEED    2.4f
#define PONG_PADDLE_SPIN           0.38f
#define PONG_HIT_ANGLE             2.5f
#define PONG_RALLY_ACCELERATION    1.035f

#define PONG_CORE_GRAVITY          2.2f
#define PONG_CORE_SOFTENING        500.0f
#define PONG_CORE_BOUNCE           1.04f

#define PONG_POINT_PAUSE_TICKS     28U
#define PONG_MATCH_PAUSE_TICKS     60U

#define PONG_PLAYER_ID             1U
#define PONG_AI_ID                 2U

static uint32_t GravityPong_Random(GravityPongState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xB5297A4DU;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float GravityPong_Clamp(float value,
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

static float GravityPong_DeadZone(float value) {
    if ((value > -PONG_INPUT_DEAD_ZONE) &&
        (value < PONG_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - PONG_INPUT_DEAD_ZONE :
           value + PONG_INPUT_DEAD_ZONE;
}

static void GravityPong_ResetPaddles(GravityPongState *game) {
    game->player.width = GRAVITY_PONG_PLAYER_WIDTH;
    game->player.x =
        (GRAVITY_PONG_SCREEN_WIDTH - game->player.width) / 2.0f;
    game->player.vx = 0.0f;

    game->ai.width = GRAVITY_PONG_AI_WIDTH;
    game->ai.x =
        (GRAVITY_PONG_SCREEN_WIDTH - game->ai.width) / 2.0f;
    game->ai.vx = 0.0f;
    game->filtered_accel_x = 0.0f;
}

static void GravityPong_ParkBall(GravityPongState *game) {
    game->ball.x = GRAVITY_PONG_CORE_X;
    game->ball.y = GRAVITY_PONG_CORE_Y;
    game->ball.vx = 0.0f;
    game->ball.vy = 0.0f;
    game->rally = 0U;
}

static void GravityPong_LaunchBall(GravityPongState *game) {
    float horizontal =
        ((int32_t)(GravityPong_Random(game) % 2001U) - 1000) / 1000.0f;

    if ((horizontal > -0.28f) && (horizontal < 0.28f)) {
        horizontal = (horizontal < 0.0f) ? -0.28f : 0.28f;
    }

    game->ball.x = GRAVITY_PONG_CORE_X;
    game->ball.y = GRAVITY_PONG_CORE_Y;
    game->ball.vx = horizontal * 1.7f;
    game->ball.vy =
        (float)game->serve_direction * PONG_INITIAL_BALL_SPEED;
    game->phase = GRAVITY_PONG_PHASE_PLAYING;
    game->phase_ticks = 0U;
    game->rally = 0U;
}

static void GravityPong_NormalizeBallSpeed(GravityPongState *game,
                                           float speed) {
    float current =
        sqrtf((game->ball.vx * game->ball.vx) +
              (game->ball.vy * game->ball.vy));

    speed = GravityPong_Clamp(speed,
                              PONG_INITIAL_BALL_SPEED,
                              PONG_MAX_BALL_SPEED);
    if (current > 0.001f) {
        game->ball.vx = (game->ball.vx / current) * speed;
        game->ball.vy = (game->ball.vy / current) * speed;
    }

    if ((game->ball.vy > -PONG_MIN_VERTICAL_SPEED) &&
        (game->ball.vy < PONG_MIN_VERTICAL_SPEED)) {
        game->ball.vy =
            (game->ball.vy < 0.0f) ?
            -PONG_MIN_VERTICAL_SPEED :
            PONG_MIN_VERTICAL_SPEED;
    }
}

static void GravityPong_UpdatePlayer(GravityPongState *game,
                                     int16_t accel_x) {
    float control;
    float maximum_x =
        GRAVITY_PONG_SCREEN_WIDTH - game->player.width;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) * PONG_INPUT_FILTER;
    control = GravityPong_DeadZone(game->filtered_accel_x);

    game->player.vx += control * PONG_PLAYER_ACCEL_SCALE;
    game->player.vx *= PONG_PLAYER_FRICTION;
    game->player.vx =
        GravityPong_Clamp(game->player.vx,
                          -PONG_PLAYER_MAX_SPEED,
                          PONG_PLAYER_MAX_SPEED);
    game->player.x += game->player.vx;

    if (game->player.x < 0.0f) {
        game->player.x = 0.0f;
        game->player.vx = 0.0f;
    } else if (game->player.x > maximum_x) {
        game->player.x = maximum_x;
        game->player.vx = 0.0f;
    }
}

static void GravityPong_UpdateAi(GravityPongState *game) {
    float ai_center = game->ai.x + (game->ai.width / 2.0f);
    float target = GRAVITY_PONG_CORE_X;
    float error;
    float maximum_x = GRAVITY_PONG_SCREEN_WIDTH - game->ai.width;

    if (game->ball.vy < 0.0f) {
        target = game->ball.x + (game->ball.vx * 5.0f);
    }

    error = target - ai_center;
    game->ai.vx =
        GravityPong_Clamp(error * 0.12f,
                          -PONG_AI_MAX_SPEED,
                          PONG_AI_MAX_SPEED);
    game->ai.x += game->ai.vx;
    game->ai.x = GravityPong_Clamp(game->ai.x, 0.0f, maximum_x);
}

static void GravityPong_ApplyCore(GravityPongState *game) {
    float dx = GRAVITY_PONG_CORE_X - game->ball.x;
    float dy = GRAVITY_PONG_CORE_Y - game->ball.y;
    float distance_squared = (dx * dx) + (dy * dy);
    float collision_radius =
        GRAVITY_PONG_CORE_RADIUS + GRAVITY_PONG_BALL_RADIUS;
    float collision_squared = collision_radius * collision_radius;

    if (distance_squared > collision_squared) {
        float gravity =
            PONG_CORE_GRAVITY / (distance_squared + PONG_CORE_SOFTENING);
        game->ball.vx += dx * gravity;
        game->ball.vy += dy * gravity;
        return;
    }

    if (distance_squared > 0.01f) {
        float distance = sqrtf(distance_squared);
        float nx = (game->ball.x - GRAVITY_PONG_CORE_X) / distance;
        float ny = (game->ball.y - GRAVITY_PONG_CORE_Y) / distance;
        float inward = (game->ball.vx * nx) + (game->ball.vy * ny);

        if (inward < 0.0f) {
            float speed =
                sqrtf((game->ball.vx * game->ball.vx) +
                      (game->ball.vy * game->ball.vy));

            game->ball.vx -= 2.0f * inward * nx;
            game->ball.vy -= 2.0f * inward * ny;
            game->ball.x =
                GRAVITY_PONG_CORE_X + (nx * collision_radius);
            game->ball.y =
                GRAVITY_PONG_CORE_Y + (ny * collision_radius);
            GravityPong_NormalizeBallSpeed(game, speed * PONG_CORE_BOUNCE);
        }
    }
}

static uint8_t GravityPong_PaddleCollision(GravityPongState *game,
                                           GravityPongPaddle *paddle,
                                           int16_t paddle_y,
                                           uint8_t is_player) {
    float ball_left = game->ball.x - GRAVITY_PONG_BALL_RADIUS;
    float ball_right = game->ball.x + GRAVITY_PONG_BALL_RADIUS;
    float paddle_right = paddle->x + paddle->width;
    float hit_offset;
    float speed;

    if ((ball_right < paddle->x) || (ball_left > paddle_right)) {
        return 0U;
    }

    if (is_player != 0U) {
        if ((game->ball.vy <= 0.0f) ||
            ((game->ball.y + GRAVITY_PONG_BALL_RADIUS) < paddle_y) ||
            ((game->ball.y - GRAVITY_PONG_BALL_RADIUS) >
             (paddle_y + GRAVITY_PONG_PADDLE_HEIGHT))) {
            return 0U;
        }
        game->ball.y = paddle_y - GRAVITY_PONG_BALL_RADIUS;
        game->ball.vy = -fabsf(game->ball.vy);
    } else {
        if ((game->ball.vy >= 0.0f) ||
            ((game->ball.y - GRAVITY_PONG_BALL_RADIUS) >
             (paddle_y + GRAVITY_PONG_PADDLE_HEIGHT)) ||
            ((game->ball.y + GRAVITY_PONG_BALL_RADIUS) < paddle_y)) {
            return 0U;
        }
        game->ball.y =
            paddle_y + GRAVITY_PONG_PADDLE_HEIGHT +
            GRAVITY_PONG_BALL_RADIUS;
        game->ball.vy = fabsf(game->ball.vy);
    }

    hit_offset =
        (game->ball.x - (paddle->x + (paddle->width / 2.0f))) /
        (paddle->width / 2.0f);
    hit_offset = GravityPong_Clamp(hit_offset, -1.0f, 1.0f);
    game->ball.vx +=
        (hit_offset * PONG_HIT_ANGLE) +
        (paddle->vx * PONG_PADDLE_SPIN);

    speed = sqrtf((game->ball.vx * game->ball.vx) +
                  (game->ball.vy * game->ball.vy));
    game->rally++;
    GravityPong_NormalizeBallSpeed(game, speed * PONG_RALLY_ACCELERATION);
    return 1U;
}

static GravityPongEvent GravityPong_Score(GravityPongState *game,
                                          uint8_t scorer) {
    game->last_scorer = scorer;
    game->serve_direction = (scorer == PONG_PLAYER_ID) ? -1 : 1;
    GravityPong_ResetPaddles(game);
    GravityPong_ParkBall(game);

    if (scorer == PONG_PLAYER_ID) {
        game->player_score++;
    } else {
        game->ai_score++;
    }

    if ((game->player_score >= GRAVITY_PONG_WIN_SCORE) ||
        (game->ai_score >= GRAVITY_PONG_WIN_SCORE)) {
        game->match_winner =
            (game->player_score > game->ai_score) ?
            PONG_PLAYER_ID : PONG_AI_ID;
        game->phase = GRAVITY_PONG_PHASE_MATCH_PAUSE;
        game->phase_ticks = PONG_MATCH_PAUSE_TICKS;
    } else {
        game->phase = GRAVITY_PONG_PHASE_POINT_PAUSE;
        game->phase_ticks = PONG_POINT_PAUSE_TICKS;
    }

    return GRAVITY_PONG_EVENT_SCORE_CHANGED;
}

void GravityPong_Init(GravityPongState *game, uint32_t seed) {
    game->rng_state = (seed != 0U) ? seed : 0x68E31DA4U;
    game->filtered_accel_x = 0.0f;
    game->player_score = 0U;
    game->ai_score = 0U;
    game->last_scorer = 0U;
    game->match_winner = 0U;
    game->serve_direction =
        ((GravityPong_Random(game) & 1U) != 0U) ? 1 : -1;
    game->phase = GRAVITY_PONG_PHASE_POINT_PAUSE;
    game->phase_ticks = PONG_POINT_PAUSE_TICKS;
    GravityPong_ResetPaddles(game);
    GravityPong_ParkBall(game);
}

GravityPongEvent GravityPong_Update(GravityPongState *game,
                                    int16_t accel_x) {
    GravityPongEvent event = GRAVITY_PONG_EVENT_NONE;

    if (game->phase != GRAVITY_PONG_PHASE_PLAYING) {
        game->player.vx = 0.0f;
        game->ai.vx = 0.0f;

        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }

        if (game->phase_ticks == 0U) {
            if (game->phase == GRAVITY_PONG_PHASE_MATCH_PAUSE) {
                game->player_score = 0U;
                game->ai_score = 0U;
                game->match_winner = 0U;
                GravityPong_ResetPaddles(game);
                event |= GRAVITY_PONG_EVENT_MATCH_STARTED;
            }
            GravityPong_LaunchBall(game);
            event |= GRAVITY_PONG_EVENT_ROUND_STARTED;
        }
        return event;
    }

    GravityPong_UpdatePlayer(game, accel_x);
    GravityPong_UpdateAi(game);

    GravityPong_ApplyCore(game);
    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    if (game->ball.x < GRAVITY_PONG_BALL_RADIUS) {
        game->ball.x = GRAVITY_PONG_BALL_RADIUS;
        game->ball.vx = fabsf(game->ball.vx);
    } else if (game->ball.x >
               (GRAVITY_PONG_SCREEN_WIDTH -
                GRAVITY_PONG_BALL_RADIUS - 1)) {
        game->ball.x =
            GRAVITY_PONG_SCREEN_WIDTH - GRAVITY_PONG_BALL_RADIUS - 1;
        game->ball.vx = -fabsf(game->ball.vx);
    }

    GravityPong_PaddleCollision(game,
                                &game->player,
                                GRAVITY_PONG_PLAYER_Y,
                                1U);
    GravityPong_PaddleCollision(game,
                                &game->ai,
                                GRAVITY_PONG_AI_Y,
                                0U);

    if (game->ball.y < (GRAVITY_PONG_PLAYFIELD_TOP -
                        GRAVITY_PONG_BALL_RADIUS)) {
        event |= GravityPong_Score(game, PONG_PLAYER_ID);
    } else if (game->ball.y >
               (GRAVITY_PONG_SCREEN_HEIGHT +
                GRAVITY_PONG_BALL_RADIUS)) {
        event |= GravityPong_Score(game, PONG_AI_ID);
    }

    return event;
}
