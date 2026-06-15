#include "balance_tower_logic.h"

#include <math.h>
#include <string.h>

#define TOWER_CALIBRATION_TICKS        45U
#define TOWER_RECOVERY_TICKS           42U
#define TOWER_LEVEL_SCORE_STEP         12U

#define TOWER_GYRO_FILTER              0.28f
#define TOWER_GYRO_DEAD_ZONE           32.0f
#define TOWER_GYRO_ANGLE_SCALE         0.00020f
#define TOWER_PLATFORM_MAX_ANGLE       12.0f
#define TOWER_PLATFORM_FAILURE_ANGLE   11.2f
#define TOWER_LEAN_FAILURE_ANGLE       14.5f

static uint32_t BalanceTower_Random(BalanceTowerState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xA511E9B3U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float BalanceTower_Clamp(float value,
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

static float BalanceTower_DeadZone(float value) {
    if ((value > -TOWER_GYRO_DEAD_ZONE) &&
        (value < TOWER_GYRO_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - TOWER_GYRO_DEAD_ZONE :
           value + TOWER_GYRO_DEAD_ZONE;
}

static void BalanceTower_GenerateBlocks(BalanceTowerState *game) {
    uint8_t bonus_blocks = game->level / 2U;

    if (bonus_blocks > 3U) {
        bonus_blocks = 3U;
    }
    game->block_count = (uint8_t)(5U + bonus_blocks);

    for (uint8_t i = 0U; i < BALANCE_TOWER_MAX_BLOCKS; i++) {
        if (i < game->block_count) {
            game->block_widths[i] =
                (uint8_t)(52U + (BalanceTower_Random(game) % 15U));
            game->block_offsets[i] =
                (int8_t)((int32_t)(
                    BalanceTower_Random(game) % 7U) - 3);
            game->block_styles[i] =
                (uint8_t)(BalanceTower_Random(game) % 5U);
        } else {
            game->block_widths[i] = 0U;
            game->block_offsets[i] = 0;
            game->block_styles[i] = 0U;
        }
    }
}

static void BalanceTower_ResetMotion(BalanceTowerState *game) {
    game->platform_angle = 0.0f;
    game->platform_velocity = 0.0f;
    game->tower_lean = 0.0f;
    game->tower_velocity = 0.0f;
    game->filtered_gyro = 0.0f;
    game->disturbance = 0.0f;
    game->stable_ticks = 0U;
}

static void BalanceTower_StartLevel(BalanceTowerState *game,
                                    uint8_t level) {
    game->level = level;
    BalanceTower_GenerateBlocks(game);
    BalanceTower_ResetMotion(game);
    game->phase = BALANCE_TOWER_PHASE_PLAYING;
    game->phase_ticks = 0U;
}

static void BalanceTower_SelectDisturbance(
    BalanceTowerState *game) {
    float strength =
        0.0028f + ((float)(game->level - 1U) * 0.00045f);
    float random =
        ((int32_t)(BalanceTower_Random(game) % 2001U) - 1000) /
        1000.0f;

    strength = BalanceTower_Clamp(strength, 0.0028f, 0.0070f);
    if ((random > -0.28f) && (random < 0.28f)) {
        random = (random < 0.0f) ? -0.28f : 0.28f;
    }
    game->disturbance = random * strength;
    game->phase_ticks = (uint16_t)(
        48U + (BalanceTower_Random(game) % 45U));
}

void BalanceTower_GetBlockRect(const BalanceTowerState *game,
                               uint8_t index,
                               int16_t *x,
                               int16_t *y,
                               int16_t *width,
                               int16_t *height) {
    int16_t horizontal_shift;
    int16_t block_width;

    if (index >= game->block_count) {
        *x = 0;
        *y = 0;
        *width = 0;
        *height = 0;
        return;
    }

    block_width = game->block_widths[index];
    horizontal_shift = (int16_t)(
        game->tower_lean * (float)(index + 1U) * 0.28f);
    *x = BALANCE_TOWER_PLATFORM_CENTER_X -
         (block_width / 2) +
         game->block_offsets[index] +
         horizontal_shift;
    *y = BALANCE_TOWER_PLATFORM_Y -
         BALANCE_TOWER_PLATFORM_THICKNESS -
         ((int16_t)(index + 1U) *
          (BALANCE_TOWER_BLOCK_HEIGHT +
           BALANCE_TOWER_BLOCK_GAP));
    *width = block_width;
    *height = BALANCE_TOWER_BLOCK_HEIGHT;
}

void BalanceTower_Init(BalanceTowerState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x63D83595U;
    game->level = 1U;
    game->phase = BALANCE_TOWER_PHASE_CALIBRATING;
    BalanceTower_GenerateBlocks(game);
}

BalanceTowerEvent BalanceTower_Update(BalanceTowerState *game,
                                      int16_t gyro_z) {
    BalanceTowerEvent event = BALANCE_TOWER_EVENT_NONE;
    float corrected_gyro;
    float tower_target;
    float stability;

    game->ticks++;
    if (game->phase == BALANCE_TOWER_PHASE_CALIBRATING) {
        game->gyro_bias_sum += gyro_z;
        game->calibration_samples++;
        if (game->calibration_samples >= TOWER_CALIBRATION_TICKS) {
            game->gyro_bias =
                game->gyro_bias_sum / game->calibration_samples;
            BalanceTower_StartLevel(game, 1U);
            return BALANCE_TOWER_EVENT_HUD_CHANGED |
                   BALANCE_TOWER_EVENT_ROUND_STARTED;
        }
        return event;
    }

    if (game->phase == BALANCE_TOWER_PHASE_RECOVERY) {
        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }
        if (game->phase_ticks == 0U) {
            BalanceTower_StartLevel(game, game->level);
            return BALANCE_TOWER_EVENT_HUD_CHANGED |
                   BALANCE_TOWER_EVENT_ROUND_STARTED;
        }
        return event;
    }

    corrected_gyro = (float)gyro_z - game->gyro_bias;
    game->filtered_gyro +=
        (corrected_gyro - game->filtered_gyro) *
        TOWER_GYRO_FILTER;
    corrected_gyro = BalanceTower_DeadZone(game->filtered_gyro);

    if (game->phase_ticks > 0U) {
        game->phase_ticks--;
    }
    if (game->phase_ticks == 0U) {
        BalanceTower_SelectDisturbance(game);
    }

    game->platform_velocity += game->disturbance;
    game->platform_velocity -= game->platform_angle * 0.0017f;
    game->platform_velocity *= 0.986f;
    game->platform_angle +=
        game->platform_velocity +
        (corrected_gyro * TOWER_GYRO_ANGLE_SCALE);
    game->platform_angle =
        BalanceTower_Clamp(game->platform_angle,
                           -TOWER_PLATFORM_MAX_ANGLE,
                           TOWER_PLATFORM_MAX_ANGLE);

    tower_target = game->platform_angle * 1.12f;
    game->tower_velocity +=
        (tower_target - game->tower_lean) * 0.020f;
    game->tower_velocity += game->disturbance * 0.85f;
    game->tower_velocity *= 0.935f;
    game->tower_lean += game->tower_velocity;

    stability =
        fabsf(game->platform_angle) +
        (fabsf(game->tower_lean) * 0.42f);
    if (stability < 4.2f) {
        game->stable_ticks++;
        if (game->stable_ticks >= 30U) {
            uint8_t new_level;

            game->stable_ticks = 0U;
            game->score++;
            event |= BALANCE_TOWER_EVENT_HUD_CHANGED;
            new_level = (uint8_t)(
                1U + (game->score / TOWER_LEVEL_SCORE_STEP));
            if (new_level != game->level) {
                BalanceTower_StartLevel(game, new_level);
                return event |
                       BALANCE_TOWER_EVENT_LEVEL_STARTED;
            }
        }
    } else {
        game->stable_ticks = 0U;
    }

    if ((fabsf(game->platform_angle) >
         TOWER_PLATFORM_FAILURE_ANGLE) ||
        (fabsf(game->tower_lean) >
         TOWER_LEAN_FAILURE_ANGLE)) {
        game->phase = BALANCE_TOWER_PHASE_RECOVERY;
        game->phase_ticks = TOWER_RECOVERY_TICKS;
        game->platform_velocity = 0.0f;
        game->tower_velocity = 0.0f;
        return event |
               BALANCE_TOWER_EVENT_RECOVERY_STARTED;
    }

    return event;
}
