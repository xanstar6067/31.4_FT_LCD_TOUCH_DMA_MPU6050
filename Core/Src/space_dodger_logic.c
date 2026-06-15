#include "space_dodger_logic.h"

#include <math.h>
#include <string.h>

#define DODGER_INPUT_FILTER             0.20f
#define DODGER_INPUT_DEAD_ZONE          280.0f
#define DODGER_SHIP_ACCEL_SCALE         0.00015f
#define DODGER_SHIP_FRICTION            0.88f
#define DODGER_SHIP_MAX_SPEED           5.8f
#define DODGER_SHIP_MIN_Y               142.0f

#define DODGER_BULLET_SPEED             8.2f
#define DODGER_BULLET_MAX_AIM_SPEED     3.2f
#define DODGER_ASTEROID_MIN_RADIUS      7U
#define DODGER_ASTEROID_MAX_RADIUS      13U
#define DODGER_INVULNERABLE_TICKS       42U
#define DODGER_RESTART_PAUSE_TICKS      50U
#define DODGER_KILLS_PER_LEVEL          12U

static uint32_t SpaceDodger_Random(SpaceDodgerState *game) {
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

static float SpaceDodger_Clamp(float value,
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

static float SpaceDodger_DeadZone(float value) {
    if ((value > -DODGER_INPUT_DEAD_ZONE) &&
        (value < DODGER_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - DODGER_INPUT_DEAD_ZONE :
           value + DODGER_INPUT_DEAD_ZONE;
}

static void SpaceDodger_ClearObjects(SpaceDodgerState *game) {
    memset(game->asteroids, 0, sizeof(game->asteroids));
    memset(game->bullets, 0, sizeof(game->bullets));
}

static void SpaceDodger_ResetShip(SpaceDodgerState *game) {
    game->ship.x = SPACE_DODGER_SCREEN_WIDTH / 2.0f;
    game->ship.y = SPACE_DODGER_SCREEN_HEIGHT - 24.0f;
    game->ship.vx = 0.0f;
    game->ship.vy = 0.0f;
}

static void SpaceDodger_ResetGame(SpaceDodgerState *game) {
    SpaceDodger_ClearObjects(game);
    SpaceDodger_ResetShip(game);
    game->background_seed = SpaceDodger_Random(game);
    game->score = 0U;
    game->ticks = 0U;
    game->filtered_accel_x = 0.0f;
    game->filtered_accel_y = 0.0f;
    game->kills = 0U;
    game->phase_ticks = 0U;
    game->level = 1U;
    game->shields = SPACE_DODGER_STARTING_SHIELDS;
    game->spawn_ticks = 5U;
    game->fire_ticks = 8U;
    game->invulnerable_ticks = DODGER_INVULNERABLE_TICKS;
    game->phase = SPACE_DODGER_PHASE_PLAYING;
}

static void SpaceDodger_UpdateShip(SpaceDodgerState *game,
                                   int16_t accel_x,
                                   int16_t accel_y) {
    const float half_width = SPACE_DODGER_SHIP_WIDTH / 2.0f;
    const float half_height = SPACE_DODGER_SHIP_HEIGHT / 2.0f;
    const float minimum_x = half_width + 1.0f;
    const float maximum_x =
        SPACE_DODGER_SCREEN_WIDTH - half_width - 2.0f;
    const float maximum_y =
        SPACE_DODGER_SCREEN_HEIGHT - half_height - 2.0f;
    float control_x;
    float control_y;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) *
        DODGER_INPUT_FILTER;
    game->filtered_accel_y +=
        ((float)accel_y - game->filtered_accel_y) *
        DODGER_INPUT_FILTER;

    control_x = SpaceDodger_DeadZone(game->filtered_accel_x);
    control_y = SpaceDodger_DeadZone(game->filtered_accel_y);
    game->ship.vx += control_x * DODGER_SHIP_ACCEL_SCALE;
    game->ship.vy += control_y * DODGER_SHIP_ACCEL_SCALE;
    game->ship.vx *= DODGER_SHIP_FRICTION;
    game->ship.vy *= DODGER_SHIP_FRICTION;
    game->ship.vx =
        SpaceDodger_Clamp(game->ship.vx,
                          -DODGER_SHIP_MAX_SPEED,
                          DODGER_SHIP_MAX_SPEED);
    game->ship.vy =
        SpaceDodger_Clamp(game->ship.vy,
                          -DODGER_SHIP_MAX_SPEED,
                          DODGER_SHIP_MAX_SPEED);

    game->ship.x += game->ship.vx;
    game->ship.y += game->ship.vy;
    if (game->ship.x < minimum_x) {
        game->ship.x = minimum_x;
        game->ship.vx = 0.0f;
    } else if (game->ship.x > maximum_x) {
        game->ship.x = maximum_x;
        game->ship.vx = 0.0f;
    }
    if (game->ship.y < DODGER_SHIP_MIN_Y) {
        game->ship.y = DODGER_SHIP_MIN_Y;
        game->ship.vy = 0.0f;
    } else if (game->ship.y > maximum_y) {
        game->ship.y = maximum_y;
        game->ship.vy = 0.0f;
    }
}

static uint8_t SpaceDodger_SpawnInterval(
    const SpaceDodgerState *game) {
    uint8_t reduction = (game->level > 12U) ? 12U : game->level;
    return (uint8_t)(27U - reduction);
}

static void SpaceDodger_SpawnAsteroid(SpaceDodgerState *game) {
    SpaceDodgerAsteroid *asteroid = 0;
    uint8_t radius;
    uint16_t horizontal_range;

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        if (game->asteroids[i].active == 0U) {
            asteroid = &game->asteroids[i];
            break;
        }
    }
    if (asteroid == 0) {
        return;
    }

    radius = (uint8_t)(
        DODGER_ASTEROID_MIN_RADIUS +
        (SpaceDodger_Random(game) %
         (DODGER_ASTEROID_MAX_RADIUS -
          DODGER_ASTEROID_MIN_RADIUS + 1U)));
    horizontal_range =
        SPACE_DODGER_SCREEN_WIDTH - (2U * radius) - 8U;

    asteroid->radius = radius;
    asteroid->x =
        radius + 4.0f +
        (float)(SpaceDodger_Random(game) % horizontal_range);
    asteroid->y =
        SPACE_DODGER_PLAYFIELD_TOP + radius + 1.0f;
    asteroid->vx =
        ((int32_t)(SpaceDodger_Random(game) % 121U) - 60) /
        100.0f;
    asteroid->vy =
        1.15f +
        ((float)(game->level - 1U) * 0.075f) +
        ((float)(SpaceDodger_Random(game) % 70U) / 100.0f);
    asteroid->vy = SpaceDodger_Clamp(asteroid->vy, 1.15f, 3.25f);
    asteroid->health =
        ((radius >= 11U) && (game->level >= 2U)) ? 2U : 1U;
    asteroid->style = (uint8_t)(SpaceDodger_Random(game) & 0x03U);
    asteroid->active = 1U;
}

static void SpaceDodger_Fire(SpaceDodgerState *game) {
    SpaceDodgerBullet *bullet = 0;
    float target_x = game->ship.x;
    float best_y = -1000.0f;
    float aim;

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        if (game->bullets[i].active == 0U) {
            bullet = &game->bullets[i];
            break;
        }
    }
    if (bullet == 0) {
        return;
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        const SpaceDodgerAsteroid *asteroid = &game->asteroids[i];

        if ((asteroid->active != 0U) &&
            (asteroid->y < (game->ship.y - 10.0f)) &&
            (asteroid->y > best_y)) {
            best_y = asteroid->y;
            target_x = asteroid->x +
                       (asteroid->vx * 8.0f);
        }
    }

    aim = (target_x - game->ship.x) * 0.045f;
    bullet->x = game->ship.x;
    bullet->y =
        game->ship.y - (SPACE_DODGER_SHIP_HEIGHT / 2.0f) - 3.0f;
    bullet->vx =
        SpaceDodger_Clamp(aim,
                          -DODGER_BULLET_MAX_AIM_SPEED,
                          DODGER_BULLET_MAX_AIM_SPEED);
    bullet->vy = -DODGER_BULLET_SPEED;
    bullet->active = 1U;
}

static void SpaceDodger_UpdateBullets(SpaceDodgerState *game) {
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        SpaceDodgerBullet *bullet = &game->bullets[i];

        if (bullet->active == 0U) {
            continue;
        }
        bullet->x += bullet->vx;
        bullet->y += bullet->vy;
        if ((bullet->y < (SPACE_DODGER_PLAYFIELD_TOP - 8.0f)) ||
            (bullet->x < -4.0f) ||
            (bullet->x > (SPACE_DODGER_SCREEN_WIDTH + 4.0f))) {
            bullet->active = 0U;
        }
    }
}

static SpaceDodgerEvent SpaceDodger_UpdateAsteroids(
    SpaceDodgerState *game) {
    SpaceDodgerEvent event = SPACE_DODGER_EVENT_NONE;

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        SpaceDodgerAsteroid *asteroid = &game->asteroids[i];

        if (asteroid->active == 0U) {
            continue;
        }

        asteroid->x += asteroid->vx;
        asteroid->y += asteroid->vy;
        if (asteroid->x < asteroid->radius) {
            asteroid->x = asteroid->radius;
            asteroid->vx = fabsf(asteroid->vx);
        } else if (asteroid->x >
                   (SPACE_DODGER_SCREEN_WIDTH -
                    asteroid->radius - 1.0f)) {
            asteroid->x =
                SPACE_DODGER_SCREEN_WIDTH -
                asteroid->radius - 1.0f;
            asteroid->vx = -fabsf(asteroid->vx);
        }

        if (asteroid->y >
            (SPACE_DODGER_SCREEN_HEIGHT + asteroid->radius)) {
            asteroid->active = 0U;
            game->score++;
            event |= SPACE_DODGER_EVENT_HUD_CHANGED;
        }
    }
    return event;
}

static SpaceDodgerEvent SpaceDodger_BulletCollisions(
    SpaceDodgerState *game) {
    SpaceDodgerEvent event = SPACE_DODGER_EVENT_NONE;

    for (uint8_t bullet_index = 0U;
         bullet_index < SPACE_DODGER_MAX_BULLETS;
         bullet_index++) {
        SpaceDodgerBullet *bullet = &game->bullets[bullet_index];

        if (bullet->active == 0U) {
            continue;
        }

        for (uint8_t asteroid_index = 0U;
             asteroid_index < SPACE_DODGER_MAX_ASTEROIDS;
             asteroid_index++) {
            SpaceDodgerAsteroid *asteroid =
                &game->asteroids[asteroid_index];
            float dx;
            float dy;
            float hit_radius;

            if (asteroid->active == 0U) {
                continue;
            }
            dx = bullet->x - asteroid->x;
            dy = bullet->y - asteroid->y;
            hit_radius = asteroid->radius + 2.0f;
            if (((dx * dx) + (dy * dy)) >
                (hit_radius * hit_radius)) {
                continue;
            }

            bullet->active = 0U;
            if (asteroid->health > 1U) {
                asteroid->health--;
                if (asteroid->radius > DODGER_ASTEROID_MIN_RADIUS) {
                    asteroid->radius -= 2U;
                }
            } else {
                uint8_t new_level;

                asteroid->active = 0U;
                game->score += 10U + (2U * asteroid->radius);
                game->kills++;
                new_level = (uint8_t)(
                    1U + (game->kills / DODGER_KILLS_PER_LEVEL));
                if (new_level != game->level) {
                    game->level = new_level;
                }
            }
            event |= SPACE_DODGER_EVENT_HUD_CHANGED;
            break;
        }
    }
    return event;
}

static uint8_t SpaceDodger_ShipHit(
    const SpaceDodgerState *game,
    const SpaceDodgerAsteroid *asteroid) {
    float half_width = (SPACE_DODGER_SHIP_WIDTH / 2.0f) - 2.0f;
    float half_height = (SPACE_DODGER_SHIP_HEIGHT / 2.0f) - 1.0f;
    float nearest_x =
        SpaceDodger_Clamp(asteroid->x,
                          game->ship.x - half_width,
                          game->ship.x + half_width);
    float nearest_y =
        SpaceDodger_Clamp(asteroid->y,
                          game->ship.y - half_height,
                          game->ship.y + half_height);
    float dx = asteroid->x - nearest_x;
    float dy = asteroid->y - nearest_y;

    return ((dx * dx) + (dy * dy)) <=
           ((float)asteroid->radius * asteroid->radius);
}

static SpaceDodgerEvent SpaceDodger_ShipCollisions(
    SpaceDodgerState *game) {
    if (game->invulnerable_ticks != 0U) {
        return SPACE_DODGER_EVENT_NONE;
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        SpaceDodgerAsteroid *asteroid = &game->asteroids[i];

        if ((asteroid->active == 0U) ||
            (SpaceDodger_ShipHit(game, asteroid) == 0U)) {
            continue;
        }

        asteroid->active = 0U;
        if (game->shields > 0U) {
            game->shields--;
        }
        if (game->score >= 25U) {
            game->score -= 25U;
        } else {
            game->score = 0U;
        }

        if (game->shields == 0U) {
            SpaceDodger_ClearObjects(game);
            game->ship.vx = 0.0f;
            game->ship.vy = 0.0f;
            game->phase = SPACE_DODGER_PHASE_RESTART_PAUSE;
            game->phase_ticks = DODGER_RESTART_PAUSE_TICKS;
            return SPACE_DODGER_EVENT_HUD_CHANGED |
                   SPACE_DODGER_EVENT_RESTART_PAUSE;
        }

        game->invulnerable_ticks = DODGER_INVULNERABLE_TICKS;
        return SPACE_DODGER_EVENT_HUD_CHANGED;
    }
    return SPACE_DODGER_EVENT_NONE;
}

void SpaceDodger_Init(SpaceDodgerState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x68E31DA4U;
    SpaceDodger_ResetGame(game);
}

SpaceDodgerEvent SpaceDodger_Update(SpaceDodgerState *game,
                                    int16_t accel_x,
                                    int16_t accel_y) {
    SpaceDodgerEvent event = SPACE_DODGER_EVENT_NONE;

    game->ticks++;
    if (game->phase == SPACE_DODGER_PHASE_RESTART_PAUSE) {
        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }
        if (game->phase_ticks == 0U) {
            SpaceDodger_ResetGame(game);
            return SPACE_DODGER_EVENT_HUD_CHANGED |
                   SPACE_DODGER_EVENT_GAME_STARTED;
        }
        return event;
    }

    SpaceDodger_UpdateShip(game, accel_x, accel_y);
    if (game->invulnerable_ticks > 0U) {
        game->invulnerable_ticks--;
    }

    if (game->spawn_ticks > 0U) {
        game->spawn_ticks--;
    }
    if (game->spawn_ticks == 0U) {
        SpaceDodger_SpawnAsteroid(game);
        game->spawn_ticks = SpaceDodger_SpawnInterval(game);
    }

    if (game->fire_ticks > 0U) {
        game->fire_ticks--;
    }
    if (game->fire_ticks == 0U) {
        uint8_t reduction =
            (game->level > 8U) ? 4U : (game->level / 2U);

        SpaceDodger_Fire(game);
        game->fire_ticks = (uint8_t)(13U - reduction);
    }

    SpaceDodger_UpdateBullets(game);
    event |= SpaceDodger_UpdateAsteroids(game);
    event |= SpaceDodger_BulletCollisions(game);
    event |= SpaceDodger_ShipCollisions(game);
    return event;
}
