#include "shake_flight_logic.h"

#include <string.h>

#define SHAKE_GRAVITY                 0.30f
#define SHAKE_MAX_FALL_SPEED          5.8f
#define SHAKE_MIN_RISE_SPEED         -7.0f
#define SHAKE_Z_RAW_GATE              2300.0f
#define SHAKE_Z_THRESHOLD             3600.0f
#define SHAKE_Z_FULL_POWER            9300.0f
#define SHAKE_Z_FILTER                0.24f
#define SHAKE_COOLDOWN_TICKS          8U
#define SHAKE_RECOVERY_TICKS          45U

static uint32_t ShakeFlight_Random(ShakeFlightState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0x91E10DA5U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float ShakeFlight_Clamp(float value,
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

float ShakeFlight_PipeSpeed(const ShakeFlightState *game) {
    float speed = 1.55f + ((float)(game->level - 1U) * 0.11f);

    return ShakeFlight_Clamp(speed, 1.55f, 3.05f);
}

uint8_t ShakeFlight_GapHeight(const ShakeFlightState *game) {
    int16_t gap = 82 - ((int16_t)(game->level - 1U) * 3);

    if (gap < 58) {
        gap = 58;
    }
    return (uint8_t)gap;
}

static int16_t ShakeFlight_NewGap(ShakeFlightState *game) {
    uint8_t gap = ShakeFlight_GapHeight(game);
    int16_t minimum =
        SHAKE_FLIGHT_PLAYFIELD_TOP + 28 + (gap / 2);
    int16_t maximum =
        SHAKE_FLIGHT_GROUND_Y - 28 - (gap / 2);
    uint16_t range = (uint16_t)(maximum - minimum + 1);

    return (int16_t)(
        minimum + (ShakeFlight_Random(game) % range));
}

static void ShakeFlight_ResetRound(ShakeFlightState *game,
                                   uint8_t reset_score) {
    if (reset_score != 0U) {
        game->score = 0U;
        game->level = 1U;
    }
    game->bird_y =
        (SHAKE_FLIGHT_PLAYFIELD_TOP +
         SHAKE_FLIGHT_GROUND_Y) / 2.0f;
    game->bird_vy = 0.0f;
    game->filtered_shake = 0.0f;
    game->initialized_accel = 0U;
    game->shake_cooldown = 8U;
    game->phase = SHAKE_FLIGHT_PHASE_PLAYING;
    game->recovery_ticks = 0U;

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        game->pipes[i].x =
            SHAKE_FLIGHT_SCREEN_WIDTH + 40.0f +
            ((float)i * SHAKE_FLIGHT_PIPE_SPACING);
        game->pipes[i].gap_y = ShakeFlight_NewGap(game);
        game->pipes[i].passed = 0U;
    }
}

static void ShakeFlight_ApplyShake(ShakeFlightState *game,
                                   int16_t accel_z) {
    int32_t dz;
    float raw_shake;
    float power;

    if (game->initialized_accel == 0U) {
        game->previous_accel_z = accel_z;
        game->initialized_accel = 1U;
        return;
    }

    dz = (int32_t)accel_z - game->previous_accel_z;
    game->previous_accel_z = accel_z;

    if (dz < 0) {
        dz = -dz;
    }
    raw_shake = (float)dz;
    if (raw_shake < SHAKE_Z_RAW_GATE) {
        raw_shake = 0.0f;
    }
    game->filtered_shake +=
        (raw_shake - game->filtered_shake) *
        SHAKE_Z_FILTER;

    if (game->shake_cooldown > 0U) {
        game->shake_cooldown--;
        return;
    }
    if (game->filtered_shake <= SHAKE_Z_THRESHOLD) {
        return;
    }

    power =
        (game->filtered_shake - SHAKE_Z_THRESHOLD) /
        SHAKE_Z_FULL_POWER;
    power = ShakeFlight_Clamp(power, 0.0f, 1.0f);
    game->bird_vy = -2.2f - (power * 4.8f);
    game->bird_vy =
        ShakeFlight_Clamp(game->bird_vy,
                          SHAKE_MIN_RISE_SPEED,
                          SHAKE_MAX_FALL_SPEED);
    game->shake_cooldown =
        (uint16_t)(SHAKE_COOLDOWN_TICKS +
                   (uint16_t)(power * 3.0f));
    game->filtered_shake = 0.0f;
}

static void ShakeFlight_UpdatePipes(ShakeFlightState *game,
                                    ShakeFlightEvent *event) {
    float speed = ShakeFlight_PipeSpeed(game);

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        ShakeFlightPipe *pipe = &game->pipes[i];

        pipe->x -= speed;
        if ((pipe->passed == 0U) &&
            ((pipe->x + SHAKE_FLIGHT_PIPE_WIDTH) <
             SHAKE_FLIGHT_BIRD_X)) {
            pipe->passed = 1U;
            game->score++;
            game->level =
                (uint8_t)(1U + (game->score / 8U));
            *event |= SHAKE_FLIGHT_EVENT_HUD_CHANGED;
        }

        if (pipe->x < -SHAKE_FLIGHT_PIPE_WIDTH - 2.0f) {
            pipe->x +=
                SHAKE_FLIGHT_PIPE_SPACING *
                SHAKE_FLIGHT_PIPE_COUNT;
            pipe->gap_y = ShakeFlight_NewGap(game);
            pipe->passed = 0U;
        }
    }
}

static uint8_t ShakeFlight_Collides(
    const ShakeFlightState *game) {
    float bird_top =
        game->bird_y - SHAKE_FLIGHT_BIRD_RADIUS;
    float bird_bottom =
        game->bird_y + SHAKE_FLIGHT_BIRD_RADIUS;

    if (bird_top < SHAKE_FLIGHT_PLAYFIELD_TOP) {
        return 1U;
    }
    if (bird_bottom >= SHAKE_FLIGHT_GROUND_Y) {
        return 1U;
    }

    for (uint8_t i = 0U; i < SHAKE_FLIGHT_PIPE_COUNT; i++) {
        const ShakeFlightPipe *pipe = &game->pipes[i];
        float pipe_left = pipe->x;
        float pipe_right =
            pipe->x + SHAKE_FLIGHT_PIPE_WIDTH;
        int16_t half_gap = ShakeFlight_GapHeight(game) / 2;
        int16_t gap_top = pipe->gap_y - half_gap;
        int16_t gap_bottom = pipe->gap_y + half_gap;

        if (((SHAKE_FLIGHT_BIRD_X + SHAKE_FLIGHT_BIRD_RADIUS) <
             pipe_left) ||
            ((SHAKE_FLIGHT_BIRD_X - SHAKE_FLIGHT_BIRD_RADIUS) >
             pipe_right)) {
            continue;
        }
        if ((bird_top < gap_top) || (bird_bottom > gap_bottom)) {
            return 1U;
        }
    }
    return 0U;
}

void ShakeFlight_Init(ShakeFlightState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x739A4C2BU;
    game->level = 1U;
    ShakeFlight_ResetRound(game, 1U);
}

ShakeFlightEvent ShakeFlight_Update(ShakeFlightState *game,
                                    int16_t accel_z) {
    ShakeFlightEvent event = SHAKE_FLIGHT_EVENT_NONE;

    game->ticks++;
    if (game->phase == SHAKE_FLIGHT_PHASE_RECOVERY) {
        if (game->recovery_ticks > 0U) {
            game->recovery_ticks--;
        }
        if (game->recovery_ticks == 0U) {
            ShakeFlight_ResetRound(game, 1U);
            return SHAKE_FLIGHT_EVENT_HUD_CHANGED |
                   SHAKE_FLIGHT_EVENT_ROUND_STARTED;
        }
        return event;
    }

    ShakeFlight_ApplyShake(game, accel_z);
    game->bird_vy += SHAKE_GRAVITY;
    game->bird_vy =
        ShakeFlight_Clamp(game->bird_vy,
                          SHAKE_MIN_RISE_SPEED,
                          SHAKE_MAX_FALL_SPEED);
    game->bird_y += game->bird_vy;
    ShakeFlight_UpdatePipes(game, &event);

    if (ShakeFlight_Collides(game) != 0U) {
        game->phase = SHAKE_FLIGHT_PHASE_RECOVERY;
        game->recovery_ticks = SHAKE_RECOVERY_TICKS;
        return event | SHAKE_FLIGHT_EVENT_CRASH;
    }

    return event;
}
