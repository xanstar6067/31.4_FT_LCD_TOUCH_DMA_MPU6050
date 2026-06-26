#include "comet_catch_logic.h"

#include <string.h>

#define COMET_INPUT_FILTER              0.18f
#define COMET_INPUT_DEAD_ZONE           260.0f
#define COMET_CATCHER_ACCEL_SCALE       0.00015f
#define COMET_CATCHER_FRICTION          0.86f
#define COMET_CATCHER_MAX_SPEED         5.8f
#define COMET_CATCHER_Y                 220.0f

#define COMET_PULSE_RADIUS              34.0f
#define COMET_PULSE_DRAIN                3U
#define COMET_PULSE_SPARK_COST          10U
#define COMET_RESTART_PAUSE_TICKS       64U
#define COMET_HIT_INVULNERABLE_TICKS    28U

static uint32_t CometCatch_Random(CometCatchState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xC0E7CA7CU;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float CometCatch_Clamp(float value,
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

static float CometCatch_Abs(float value) {
    return (value < 0.0f) ? -value : value;
}

static float CometCatch_DeadZone(float value) {
    if ((value > -COMET_INPUT_DEAD_ZONE) &&
        (value < COMET_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - COMET_INPUT_DEAD_ZONE :
           value + COMET_INPUT_DEAD_ZONE;
}

static void CometCatch_ClearObjects(CometCatchState *game) {
    memset(game->objects, 0, sizeof(game->objects));
}

static uint8_t CometCatch_SpawnInterval(
    const CometCatchState *game) {
    uint8_t level = (game->level > 14U) ? 14U : game->level;

    return (uint8_t)(27U - level);
}

static void CometCatch_ResetRound(CometCatchState *game) {
    CometCatch_ClearObjects(game);
    game->background_seed = CometCatch_Random(game);
    game->score = 0U;
    game->ticks = 0U;
    game->catcher_x = COMET_CATCH_SCREEN_WIDTH / 2.0f;
    game->catcher_vx = 0.0f;
    game->filtered_accel_x = 0.0f;
    game->phase_ticks = 0U;
    game->spawn_ticks = 8U;
    game->lives = COMET_CATCH_STARTING_LIVES;
    game->level = 1U;
    game->combo = 0U;
    game->best_combo = 0U;
    game->pulse_energy = COMET_CATCH_ENERGY_MAX;
    game->pulse_active = 0U;
    game->invulnerable_ticks = COMET_HIT_INVULNERABLE_TICKS;
    game->phase = COMET_CATCH_PHASE_PLAYING;
}

static void CometCatch_StartRestartPause(CometCatchState *game) {
    CometCatch_ClearObjects(game);
    game->catcher_vx = 0.0f;
    game->pulse_active = 0U;
    game->pulse_energy = 0U;
    game->phase = COMET_CATCH_PHASE_RESTART_PAUSE;
    game->phase_ticks = COMET_RESTART_PAUSE_TICKS;
}

static uint8_t CometCatch_UpdateLevel(CometCatchState *game) {
    uint8_t level = (uint8_t)(1U + (game->score / 85U));

    if (level > 12U) {
        level = 12U;
    }
    if (level == game->level) {
        return 0U;
    }

    game->level = level;
    return 1U;
}

static void CometCatch_SpawnObject(CometCatchState *game) {
    CometCatchObject *object = 0;
    uint8_t spark_roll;
    uint8_t radius;
    uint8_t margin;
    uint16_t horizontal_range;

    for (uint8_t i = 0U; i < COMET_CATCH_MAX_OBJECTS; i++) {
        if (game->objects[i].active == 0U) {
            object = &game->objects[i];
            break;
        }
    }
    if (object == 0) {
        return;
    }

    spark_roll =
        (uint8_t)(CometCatch_Random(game) %
                  ((game->level >= 7U) ? 5U : 6U));
    object->type =
        ((game->level >= 2U) && (spark_roll == 0U)) ?
        COMET_CATCH_OBJECT_SPARK :
        COMET_CATCH_OBJECT_STAR;
    radius =
        (object->type == COMET_CATCH_OBJECT_SPARK) ?
        (uint8_t)(5U + (CometCatch_Random(game) % 3U)) :
        (uint8_t)(4U + (CometCatch_Random(game) % 4U));
    margin = (uint8_t)(radius + 5U);
    horizontal_range =
        (uint16_t)(COMET_CATCH_SCREEN_WIDTH - (2U * margin));

    object->radius = radius;
    object->x =
        margin +
        (float)(CometCatch_Random(game) % horizontal_range);
    object->y =
        COMET_CATCH_PLAYFIELD_TOP + radius + 1.0f;
    object->vx =
        ((int32_t)(CometCatch_Random(game) % 91U) - 45L) /
        100.0f;
    object->vy =
        1.05f +
        ((float)(game->level - 1U) * 0.10f) +
        ((float)(CometCatch_Random(game) % 65U) / 100.0f);
    if (object->type == COMET_CATCH_OBJECT_SPARK) {
        object->vy += 0.18f;
    }
    object->vy = CometCatch_Clamp(object->vy, 1.05f, 3.35f);
    object->style = (uint8_t)(CometCatch_Random(game) & 0x03U);
    object->active = 1U;
}

static CometCatchEvent CometCatch_UpdatePulse(
    CometCatchState *game,
    uint8_t touch_pressed) {
    CometCatchEvent event = COMET_CATCH_EVENT_NONE;
    uint8_t previous_active = game->pulse_active;
    uint8_t previous_energy = game->pulse_energy;

    if ((touch_pressed != 0U) && (game->pulse_energy > 0U)) {
        game->pulse_active = 1U;
        if (game->pulse_energy > COMET_PULSE_DRAIN) {
            game->pulse_energy =
                (uint8_t)(game->pulse_energy - COMET_PULSE_DRAIN);
        } else {
            game->pulse_energy = 0U;
            game->pulse_active = 0U;
        }
    } else {
        game->pulse_active = 0U;
        if (game->pulse_energy < COMET_CATCH_ENERGY_MAX) {
            game->pulse_energy++;
        }
    }

    if (previous_active != game->pulse_active) {
        event |= COMET_CATCH_EVENT_PULSE_CHANGED;
    }
    if (previous_energy != game->pulse_energy) {
        event |= COMET_CATCH_EVENT_HUD;
    }
    return event;
}

static void CometCatch_UpdateCatcher(CometCatchState *game,
                                     int16_t accel_x) {
    float control;
    const float half_width = COMET_CATCH_CATCHER_WIDTH / 2.0f;
    const float minimum_x = half_width + 4.0f;
    const float maximum_x =
        COMET_CATCH_SCREEN_WIDTH - half_width - 5.0f;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) *
        COMET_INPUT_FILTER;
    control = CometCatch_DeadZone(game->filtered_accel_x);

    game->catcher_vx += control * COMET_CATCHER_ACCEL_SCALE;
    game->catcher_vx *= COMET_CATCHER_FRICTION;
    game->catcher_vx =
        CometCatch_Clamp(game->catcher_vx,
                         -COMET_CATCHER_MAX_SPEED,
                         COMET_CATCHER_MAX_SPEED);
    game->catcher_x += game->catcher_vx;

    if (game->catcher_x < minimum_x) {
        game->catcher_x = minimum_x;
        game->catcher_vx = 0.0f;
    } else if (game->catcher_x > maximum_x) {
        game->catcher_x = maximum_x;
        game->catcher_vx = 0.0f;
    }
}

static uint8_t CometCatch_ObjectHitsCatcher(
    const CometCatchState *game,
    const CometCatchObject *object) {
    float dx;

    if ((object->y + object->radius) < (COMET_CATCHER_Y - 8.0f)) {
        return 0U;
    }
    if ((object->y - object->radius) >
        (COMET_CATCHER_Y + COMET_CATCH_CATCHER_HEIGHT)) {
        return 0U;
    }

    dx = CometCatch_Abs(object->x - game->catcher_x);
    return (dx <=
            ((COMET_CATCH_CATCHER_WIDTH / 2.0f) +
             object->radius)) ? 1U : 0U;
}

static uint8_t CometCatch_ObjectInPulse(
    const CometCatchState *game,
    const CometCatchObject *object) {
    float dx;
    float dy;
    float radius;

    if (game->pulse_active == 0U) {
        return 0U;
    }

    dx = object->x - game->catcher_x;
    dy = object->y - (COMET_CATCHER_Y - 8.0f);
    radius = COMET_PULSE_RADIUS + object->radius;
    return (((dx * dx) + (dy * dy)) <=
            (radius * radius)) ? 1U : 0U;
}

static CometCatchEvent CometCatch_CatchStar(
    CometCatchState *game,
    uint8_t by_pulse) {
    uint16_t gain =
        (uint16_t)(8U + game->level + (game->combo / 3U));

    if (by_pulse != 0U) {
        gain = (uint16_t)(gain + 3U);
    }
    game->score += gain;
    if (game->combo < 99U) {
        game->combo++;
    }
    if (game->combo > game->best_combo) {
        game->best_combo = game->combo;
    }

    return COMET_CATCH_EVENT_HUD |
           (CometCatch_UpdateLevel(game) ?
            COMET_CATCH_EVENT_HUD : COMET_CATCH_EVENT_NONE);
}

static CometCatchEvent CometCatch_ClearSpark(
    CometCatchState *game) {
    if (game->pulse_energy > COMET_PULSE_SPARK_COST) {
        game->pulse_energy =
            (uint8_t)(game->pulse_energy - COMET_PULSE_SPARK_COST);
    } else {
        game->pulse_energy = 0U;
        game->pulse_active = 0U;
    }

    game->score += 2U + (game->level / 2U);
    return COMET_CATCH_EVENT_HUD |
           COMET_CATCH_EVENT_PULSE_CHANGED |
           (CometCatch_UpdateLevel(game) ?
            COMET_CATCH_EVENT_HUD : COMET_CATCH_EVENT_NONE);
}

static CometCatchEvent CometCatch_Damage(CometCatchState *game) {
    CometCatchEvent event = COMET_CATCH_EVENT_HUD |
                            COMET_CATCH_EVENT_PULSE_CHANGED;

    if (game->invulnerable_ticks != 0U) {
        return COMET_CATCH_EVENT_NONE;
    }

    if (game->lives > 0U) {
        game->lives--;
    }
    game->combo = 0U;

    if (game->lives == 0U) {
        CometCatch_StartRestartPause(game);
        event |= COMET_CATCH_EVENT_RESTART_PAUSE;
    } else {
        game->invulnerable_ticks = COMET_HIT_INVULNERABLE_TICKS;
    }
    return event;
}

static CometCatchEvent CometCatch_UpdateObjects(
    CometCatchState *game) {
    CometCatchEvent event = COMET_CATCH_EVENT_OBJECTS;

    for (uint8_t i = 0U; i < COMET_CATCH_MAX_OBJECTS; i++) {
        CometCatchObject *object = &game->objects[i];
        uint8_t remove_object = 0U;

        if (object->active == 0U) {
            continue;
        }

        object->x += object->vx;
        object->y += object->vy;
        if (object->x < object->radius) {
            object->x = object->radius;
            object->vx = -object->vx;
        } else if (object->x >
                   (COMET_CATCH_SCREEN_WIDTH -
                    object->radius - 1.0f)) {
            object->x =
                COMET_CATCH_SCREEN_WIDTH - object->radius - 1.0f;
            object->vx = -object->vx;
        }

        if (object->type == COMET_CATCH_OBJECT_STAR) {
            if (CometCatch_ObjectInPulse(game, object) != 0U) {
                event |= CometCatch_CatchStar(game, 1U);
                remove_object = 1U;
            } else if (CometCatch_ObjectHitsCatcher(game,
                                                   object) != 0U) {
                event |= CometCatch_CatchStar(game, 0U);
                remove_object = 1U;
            } else if (object->y >
                       (COMET_CATCH_SCREEN_HEIGHT +
                        object->radius)) {
                if (game->combo != 0U) {
                    game->combo = 0U;
                    event |= COMET_CATCH_EVENT_HUD;
                }
                remove_object = 1U;
            }
        } else {
            if (CometCatch_ObjectInPulse(game, object) != 0U) {
                event |= CometCatch_ClearSpark(game);
                remove_object = 1U;
            } else if (CometCatch_ObjectHitsCatcher(game,
                                                   object) != 0U) {
                event |= CometCatch_Damage(game);
                remove_object = 1U;
            } else if (object->y >
                       (COMET_CATCH_SCREEN_HEIGHT +
                        object->radius)) {
                remove_object = 1U;
            }
        }

        if (remove_object != 0U) {
            object->active = 0U;
        }
        if ((event & COMET_CATCH_EVENT_RESTART_PAUSE) != 0U) {
            break;
        }
    }

    return event;
}

void CometCatch_Init(CometCatchState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x74C0A751U;
    CometCatch_ResetRound(game);
}

CometCatchEvent CometCatch_Update(CometCatchState *game,
                                  int16_t accel_x,
                                  uint8_t touch_pressed) {
    CometCatchEvent event = COMET_CATCH_EVENT_NONE;

    game->ticks++;
    if (game->phase == COMET_CATCH_PHASE_RESTART_PAUSE) {
        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }
        if (game->phase_ticks == 0U) {
            CometCatch_ResetRound(game);
            return COMET_CATCH_EVENT_GAME_STARTED |
                   COMET_CATCH_EVENT_HUD |
                   COMET_CATCH_EVENT_OBJECTS;
        }
        return event;
    }

    event |= CometCatch_UpdatePulse(game, touch_pressed);
    CometCatch_UpdateCatcher(game, accel_x);
    event |= COMET_CATCH_EVENT_OBJECTS;

    if (game->invulnerable_ticks > 0U) {
        game->invulnerable_ticks--;
    }

    if (game->spawn_ticks > 0U) {
        game->spawn_ticks--;
    }
    if (game->spawn_ticks == 0U) {
        CometCatch_SpawnObject(game);
        game->spawn_ticks = CometCatch_SpawnInterval(game);
    }

    event |= CometCatch_UpdateObjects(game);
    return event;
}
