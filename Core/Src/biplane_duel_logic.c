#include "biplane_duel_logic.h"

#include <string.h>

#define BIPLANE_INPUT_FILTER             0.18f
#define BIPLANE_INPUT_DEAD_ZONE          280.0f
#define BIPLANE_PLAYER_ACCEL_SCALE       0.000135f
#define BIPLANE_PLAYER_FRICTION          0.90f
#define BIPLANE_PLAYER_MAX_SPEED         4.5f
#define BIPLANE_PLAYER_MIN_Y             48.0f

#define BIPLANE_BULLET_SPEED             7.4f
#define BIPLANE_ENEMY_BULLET_SPEED       4.7f
#define BIPLANE_FIRE_INTERVAL            8U
#define BIPLANE_ENEMY_FIRE_BASE          56U
#define BIPLANE_BOMB_DROP_BASE           58U
#define BIPLANE_ENEMY_SPAWN_BASE         86U
#define BIPLANE_INVULNERABLE_TICKS       46U
#define BIPLANE_RESTART_PAUSE_TICKS      55U
#define BIPLANE_KILLS_PER_LEVEL          8U

static uint32_t BiplaneDuel_Random(BiplaneDuelState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xA5D34B19U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float BiplaneDuel_Clamp(float value,
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

static float BiplaneDuel_Abs(float value) {
    return (value < 0.0f) ? -value : value;
}

static float BiplaneDuel_DeadZone(float value) {
    if ((value > -BIPLANE_INPUT_DEAD_ZONE) &&
        (value < BIPLANE_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - BIPLANE_INPUT_DEAD_ZONE :
           value + BIPLANE_INPUT_DEAD_ZONE;
}

static void BiplaneDuel_ClearObjects(BiplaneDuelState *game) {
    memset(game->bullets, 0, sizeof(game->bullets));
    memset(game->enemy_bullets, 0, sizeof(game->enemy_bullets));
    memset(game->bombs, 0, sizeof(game->bombs));
    memset(game->enemies, 0, sizeof(game->enemies));
    memset(game->explosions, 0, sizeof(game->explosions));
}

static void BiplaneDuel_AddExplosion(BiplaneDuelState *game,
                                     float x,
                                     float y,
                                     uint8_t radius) {
    BiplaneDuelExplosion *slot = 0;

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_EXPLOSIONS; i++) {
        if (game->explosions[i].active == 0U) {
            slot = &game->explosions[i];
            break;
        }
    }
    if (slot == 0) {
        slot = &game->explosions[0];
    }
    slot->x = x;
    slot->y = y;
    slot->radius = radius;
    slot->ticks = 16U;
    slot->active = 1U;
}

static void BiplaneDuel_ResetPlayer(BiplaneDuelState *game) {
    game->player.x = 76.0f;
    game->player.y = 150.0f;
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.facing = 1;
}

static void BiplaneDuel_ResetZeppelin(BiplaneDuelState *game) {
    int8_t direction =
        ((BiplaneDuel_Random(game) & 1U) != 0U) ? 1 : -1;

    game->zeppelin.active = 1U;
    game->zeppelin.health =
        (uint8_t)(4U + ((game->level > 8U) ? 4U :
                  (game->level / 2U)));
    game->zeppelin.x =
        (direction > 0) ? 62.0f :
        (BIPLANE_DUEL_SCREEN_WIDTH - 62.0f);
    game->zeppelin.y = 66.0f;
    game->zeppelin.vx =
        (float)direction *
        (0.55f + ((float)(game->level - 1U) * 0.035f));
    game->zeppelin.respawn_ticks = 0U;
    game->zeppelin.bomb_ticks =
        (uint16_t)(28U + (BiplaneDuel_Random(game) % 30U));
}

static void BiplaneDuel_ResetGame(BiplaneDuelState *game) {
    BiplaneDuel_ClearObjects(game);
    BiplaneDuel_ResetPlayer(game);
    game->score = 0U;
    game->ticks = 0U;
    game->filtered_accel_x = 0.0f;
    game->filtered_accel_y = 0.0f;
    game->kills = 0U;
    game->phase_ticks = 0U;
    game->spawn_ticks = 36U;
    game->fire_ticks = 5U;
    game->level = 1U;
    game->shields = BIPLANE_DUEL_STARTING_SHIELDS;
    game->invulnerable_ticks = BIPLANE_INVULNERABLE_TICKS;
    game->phase = BIPLANE_DUEL_PHASE_PLAYING;
    BiplaneDuel_ResetZeppelin(game);
}

static void BiplaneDuel_UpdatePlayer(BiplaneDuelState *game,
                                     int16_t accel_x,
                                     int16_t accel_y) {
    const float minimum_x =
        (float)BIPLANE_DUEL_PLAYER_HALF_WIDTH + 2.0f;
    const float maximum_x =
        BIPLANE_DUEL_SCREEN_WIDTH -
        (float)BIPLANE_DUEL_PLAYER_HALF_WIDTH - 2.0f;
    const float maximum_y =
        BIPLANE_DUEL_GROUND_Y -
        (float)BIPLANE_DUEL_PLAYER_HALF_HEIGHT - 2.0f;
    float control_x;
    float control_y;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) *
        BIPLANE_INPUT_FILTER;
    game->filtered_accel_y +=
        ((float)accel_y - game->filtered_accel_y) *
        BIPLANE_INPUT_FILTER;

    control_x = BiplaneDuel_DeadZone(game->filtered_accel_x);
    control_y = BiplaneDuel_DeadZone(game->filtered_accel_y);
    game->player.vx += control_x * BIPLANE_PLAYER_ACCEL_SCALE;
    game->player.vy += control_y * BIPLANE_PLAYER_ACCEL_SCALE;
    game->player.vx *= BIPLANE_PLAYER_FRICTION;
    game->player.vy *= BIPLANE_PLAYER_FRICTION;
    game->player.vx =
        BiplaneDuel_Clamp(game->player.vx,
                          -BIPLANE_PLAYER_MAX_SPEED,
                          BIPLANE_PLAYER_MAX_SPEED);
    game->player.vy =
        BiplaneDuel_Clamp(game->player.vy,
                          -BIPLANE_PLAYER_MAX_SPEED,
                          BIPLANE_PLAYER_MAX_SPEED);

    game->player.x += game->player.vx;
    game->player.y += game->player.vy;
    if (game->player.x < minimum_x) {
        game->player.x = minimum_x;
        game->player.vx = 0.0f;
    } else if (game->player.x > maximum_x) {
        game->player.x = maximum_x;
        game->player.vx = 0.0f;
    }
    if (game->player.y < BIPLANE_PLAYER_MIN_Y) {
        game->player.y = BIPLANE_PLAYER_MIN_Y;
        game->player.vy = 0.0f;
    } else if (game->player.y > maximum_y) {
        game->player.y = maximum_y;
        game->player.vy = 0.0f;
    }

    if (game->player.vx > 0.25f) {
        game->player.facing = 1;
    } else if (game->player.vx < -0.25f) {
        game->player.facing = -1;
    }
}

static void BiplaneDuel_Fire(BiplaneDuelState *game) {
    BiplaneDuelBullet *bullet = 0;
    float direction = (float)game->player.facing;

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        if (game->bullets[i].active == 0U) {
            bullet = &game->bullets[i];
            break;
        }
    }
    if (bullet == 0) {
        return;
    }

    bullet->x =
        game->player.x +
        direction *
        ((float)BIPLANE_DUEL_PLAYER_HALF_WIDTH + 2.0f);
    bullet->y = game->player.y - 2.0f;
    bullet->vx =
        (direction * BIPLANE_BULLET_SPEED) +
        (game->player.vx * 0.25f);
    bullet->vy = game->player.vy * 0.18f;
    bullet->active = 1U;
}

static void BiplaneDuel_UpdateBullets(BiplaneDuelState *game) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        BiplaneDuelBullet *bullet = &game->bullets[i];

        if (bullet->active == 0U) {
            continue;
        }
        bullet->x += bullet->vx;
        bullet->y += bullet->vy;
        if ((bullet->x < -8.0f) ||
            (bullet->x > (BIPLANE_DUEL_SCREEN_WIDTH + 8.0f)) ||
            (bullet->y < (BIPLANE_DUEL_PLAYFIELD_TOP - 6.0f)) ||
            (bullet->y > BIPLANE_DUEL_GROUND_Y)) {
            bullet->active = 0U;
        }
    }
}

static void BiplaneDuel_EnemyFire(BiplaneDuelState *game,
                                  const BiplaneDuelEnemy *enemy) {
    BiplaneDuelEnemyBullet *bullet = 0;
    float direction = (float)enemy->facing;
    float aim_y;

    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        if (game->enemy_bullets[i].active == 0U) {
            bullet = &game->enemy_bullets[i];
            break;
        }
    }
    if (bullet == 0) {
        return;
    }

    aim_y =
        BiplaneDuel_Clamp(
            (game->player.y - enemy->y) * 0.025f,
            -1.6f, 1.6f);
    bullet->x =
        enemy->x +
        direction *
        ((float)BIPLANE_DUEL_ENEMY_HALF_WIDTH + 2.0f);
    bullet->y = enemy->y;
    bullet->vx =
        (direction * BIPLANE_ENEMY_BULLET_SPEED) +
        (enemy->vx * 0.25f);
    bullet->vy = aim_y + (enemy->vy * 0.15f);
    bullet->active = 1U;
}

static void BiplaneDuel_UpdateEnemyBullets(
    BiplaneDuelState *game) {
    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        BiplaneDuelEnemyBullet *bullet =
            &game->enemy_bullets[i];

        if (bullet->active == 0U) {
            continue;
        }
        bullet->x += bullet->vx;
        bullet->y += bullet->vy;
        if ((bullet->x < -8.0f) ||
            (bullet->x > (BIPLANE_DUEL_SCREEN_WIDTH + 8.0f)) ||
            (bullet->y < (BIPLANE_DUEL_PLAYFIELD_TOP - 6.0f)) ||
            (bullet->y > BIPLANE_DUEL_GROUND_Y)) {
            bullet->active = 0U;
        }
    }
}

static void BiplaneDuel_DropBomb(BiplaneDuelState *game) {
    BiplaneDuelBomb *bomb = 0;

    if (game->zeppelin.active == 0U) {
        return;
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        if (game->bombs[i].active == 0U) {
            bomb = &game->bombs[i];
            break;
        }
    }
    if (bomb == 0) {
        return;
    }

    bomb->x = game->zeppelin.x;
    bomb->y = game->zeppelin.y + 16.0f;
    bomb->vx = game->zeppelin.vx * 0.20f;
    bomb->vy =
        1.75f + ((float)(game->level - 1U) * 0.08f);
    bomb->active = 1U;
}

static void BiplaneDuel_UpdateZeppelin(
    BiplaneDuelState *game) {
    if (game->zeppelin.active == 0U) {
        if (game->zeppelin.respawn_ticks > 0U) {
            game->zeppelin.respawn_ticks--;
        }
        if (game->zeppelin.respawn_ticks == 0U) {
            BiplaneDuel_ResetZeppelin(game);
        }
        return;
    }

    game->zeppelin.x += game->zeppelin.vx;
    if (game->zeppelin.x < 44.0f) {
        game->zeppelin.x = 44.0f;
        game->zeppelin.vx =
            BiplaneDuel_Abs(game->zeppelin.vx);
    } else if (game->zeppelin.x >
               (BIPLANE_DUEL_SCREEN_WIDTH - 44.0f)) {
        game->zeppelin.x =
            BIPLANE_DUEL_SCREEN_WIDTH - 44.0f;
        game->zeppelin.vx =
            -BiplaneDuel_Abs(game->zeppelin.vx);
    }

    if (game->zeppelin.bomb_ticks > 0U) {
        game->zeppelin.bomb_ticks--;
    }
    if (game->zeppelin.bomb_ticks == 0U) {
        uint16_t level_reduction =
            (game->level > 10U) ? 18U : (game->level * 2U);

        BiplaneDuel_DropBomb(game);
        game->zeppelin.bomb_ticks =
            (uint16_t)(BIPLANE_BOMB_DROP_BASE -
                       level_reduction +
                       (BiplaneDuel_Random(game) % 22U));
    }
}

static void BiplaneDuel_UpdateBombs(BiplaneDuelState *game) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        BiplaneDuelBomb *bomb = &game->bombs[i];

        if (bomb->active == 0U) {
            continue;
        }
        bomb->x += bomb->vx;
        bomb->y += bomb->vy;
        bomb->vy += 0.035f;
        if (bomb->y >= (BIPLANE_DUEL_GROUND_Y - 3.0f)) {
            BiplaneDuel_AddExplosion(game,
                                     bomb->x,
                                     BIPLANE_DUEL_GROUND_Y - 4.0f,
                                     9U);
            bomb->active = 0U;
        }
    }
}

static uint16_t BiplaneDuel_EnemySpawnInterval(
    const BiplaneDuelState *game) {
    uint16_t reduction =
        (game->level > 12U) ? 36U :
        ((uint16_t)game->level * 3U);
    return (uint16_t)(BIPLANE_ENEMY_SPAWN_BASE - reduction);
}

static void BiplaneDuel_SpawnEnemy(BiplaneDuelState *game) {
    BiplaneDuelEnemy *enemy = 0;
    int8_t direction =
        ((BiplaneDuel_Random(game) & 1U) != 0U) ? 1 : -1;
    uint16_t y_range =
        BIPLANE_DUEL_GROUND_Y -
        BIPLANE_DUEL_PLAYFIELD_TOP - 70U;

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        if (game->enemies[i].active == 0U) {
            enemy = &game->enemies[i];
            break;
        }
    }
    if (enemy == 0) {
        return;
    }

    enemy->x =
        (direction > 0) ? -24.0f :
        (BIPLANE_DUEL_SCREEN_WIDTH + 24.0f);
    enemy->y =
        BIPLANE_DUEL_PLAYFIELD_TOP + 38.0f +
        (float)(BiplaneDuel_Random(game) % y_range);
    enemy->vx =
        (float)direction *
        (1.20f +
         ((float)(game->level - 1U) * 0.07f) +
         ((float)(BiplaneDuel_Random(game) % 40U) / 100.0f));
    enemy->vy =
        ((int32_t)(BiplaneDuel_Random(game) % 81U) - 40) /
        100.0f;
    enemy->facing = direction;
    enemy->fire_ticks =
        (uint8_t)(24U + (BiplaneDuel_Random(game) % 34U));
    enemy->health = (game->level >= 5U) ? 2U : 1U;
    enemy->active = 1U;
}

static void BiplaneDuel_UpdateEnemies(BiplaneDuelState *game) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        BiplaneDuelEnemy *enemy = &game->enemies[i];
        float chase;

        if (enemy->active == 0U) {
            continue;
        }

        chase =
            BiplaneDuel_Clamp(
                (game->player.y - enemy->y) * 0.004f,
                -0.055f, 0.055f);
        enemy->vy += chase;
        enemy->vy =
            BiplaneDuel_Clamp(enemy->vy, -1.25f, 1.25f);
        enemy->x += enemy->vx;
        enemy->y += enemy->vy;
        if (enemy->y < BIPLANE_PLAYER_MIN_Y) {
            enemy->y = BIPLANE_PLAYER_MIN_Y;
            enemy->vy = BiplaneDuel_Abs(enemy->vy);
        } else if (enemy->y >
                   (BIPLANE_DUEL_GROUND_Y - 18.0f)) {
            enemy->y = BIPLANE_DUEL_GROUND_Y - 18.0f;
            enemy->vy = -BiplaneDuel_Abs(enemy->vy);
        }

        if (((enemy->facing > 0) &&
             (enemy->x > (BIPLANE_DUEL_SCREEN_WIDTH + 30.0f))) ||
            ((enemy->facing < 0) &&
             (enemy->x < -30.0f))) {
            enemy->active = 0U;
            continue;
        }

        if (enemy->fire_ticks > 0U) {
            enemy->fire_ticks--;
        }
        if (enemy->fire_ticks == 0U) {
            uint8_t reduction =
                (game->level > 8U) ? 16U :
                (uint8_t)(game->level * 2U);

            BiplaneDuel_EnemyFire(game, enemy);
            enemy->fire_ticks =
                (uint8_t)(BIPLANE_ENEMY_FIRE_BASE -
                          reduction +
                          (BiplaneDuel_Random(game) % 18U));
        }
    }
}

static void BiplaneDuel_UpdateExplosions(
    BiplaneDuelState *game) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_EXPLOSIONS; i++) {
        BiplaneDuelExplosion *explosion = &game->explosions[i];

        if (explosion->active == 0U) {
            continue;
        }
        if (explosion->ticks > 0U) {
            explosion->ticks--;
            if ((explosion->ticks & 3U) == 0U) {
                explosion->radius++;
            }
        }
        if (explosion->ticks == 0U) {
            explosion->active = 0U;
        }
    }
}

static uint8_t BiplaneDuel_PointHitsRect(
    float point_x,
    float point_y,
    float center_x,
    float center_y,
    float half_width,
    float half_height) {
    return (BiplaneDuel_Abs(point_x - center_x) <= half_width) &&
           (BiplaneDuel_Abs(point_y - center_y) <= half_height);
}

static BiplaneDuelEvent BiplaneDuel_BulletCollisions(
    BiplaneDuelState *game) {
    BiplaneDuelEvent event = BIPLANE_DUEL_EVENT_NONE;

    for (uint8_t bullet_index = 0U;
         bullet_index < BIPLANE_DUEL_MAX_BULLETS;
         bullet_index++) {
        BiplaneDuelBullet *bullet = &game->bullets[bullet_index];

        if (bullet->active == 0U) {
            continue;
        }

        if ((game->zeppelin.active != 0U) &&
            (BiplaneDuel_PointHitsRect(
                bullet->x, bullet->y,
                game->zeppelin.x, game->zeppelin.y,
                34.0f, 15.0f) != 0U)) {
            bullet->active = 0U;
            game->score += 5U;
            if (game->zeppelin.health > 0U) {
                game->zeppelin.health--;
            }
            if (game->zeppelin.health == 0U) {
                game->zeppelin.active = 0U;
                game->zeppelin.respawn_ticks = 90U;
                game->score += 95U;
                game->kills++;
                BiplaneDuel_AddExplosion(
                    game, game->zeppelin.x, game->zeppelin.y, 15U);
            }
            event |= BIPLANE_DUEL_EVENT_HUD_CHANGED;
            continue;
        }

        for (uint8_t enemy_index = 0U;
             enemy_index < BIPLANE_DUEL_MAX_ENEMIES;
             enemy_index++) {
            BiplaneDuelEnemy *enemy = &game->enemies[enemy_index];

            if ((enemy->active == 0U) ||
                (BiplaneDuel_PointHitsRect(
                    bullet->x, bullet->y,
                    enemy->x, enemy->y,
                    BIPLANE_DUEL_ENEMY_HALF_WIDTH,
                    BIPLANE_DUEL_ENEMY_HALF_HEIGHT) == 0U)) {
                continue;
            }

            bullet->active = 0U;
            if (enemy->health > 1U) {
                enemy->health--;
                game->score += 12U;
            } else {
                enemy->active = 0U;
                game->score += 35U;
                game->kills++;
                BiplaneDuel_AddExplosion(
                    game, enemy->x, enemy->y, 10U);
            }
            event |= BIPLANE_DUEL_EVENT_HUD_CHANGED;
            break;
        }

        if (bullet->active == 0U) {
            continue;
        }
        for (uint8_t bomb_index = 0U;
             bomb_index < BIPLANE_DUEL_MAX_BOMBS;
             bomb_index++) {
            BiplaneDuelBomb *bomb = &game->bombs[bomb_index];

            if ((bomb->active == 0U) ||
                (BiplaneDuel_PointHitsRect(
                    bullet->x, bullet->y,
                    bomb->x, bomb->y, 6.0f, 7.0f) == 0U)) {
                continue;
            }

            bullet->active = 0U;
            bomb->active = 0U;
            game->score += 8U;
            BiplaneDuel_AddExplosion(game, bomb->x, bomb->y, 6U);
            event |= BIPLANE_DUEL_EVENT_HUD_CHANGED;
            break;
        }
    }
    return event;
}

static uint8_t BiplaneDuel_PlayerVisible(
    const BiplaneDuelState *game) {
    if (game->phase != BIPLANE_DUEL_PHASE_PLAYING) {
        return 0U;
    }
    if ((game->invulnerable_ticks != 0U) &&
        (((game->ticks / 3U) & 1U) != 0U)) {
        return 0U;
    }
    return 1U;
}

static uint8_t BiplaneDuel_PlayerHitByObjects(
    const BiplaneDuelState *game) {
    float player_x = game->player.x;
    float player_y = game->player.y;

    if (BiplaneDuel_PlayerVisible(game) == 0U) {
        return 0U;
    }

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        const BiplaneDuelBomb *bomb = &game->bombs[i];

        if ((bomb->active != 0U) &&
            (BiplaneDuel_PointHitsRect(
                bomb->x, bomb->y,
                player_x, player_y,
                BIPLANE_DUEL_PLAYER_HALF_WIDTH - 2.0f,
                BIPLANE_DUEL_PLAYER_HALF_HEIGHT - 1.0f) != 0U)) {
            return 1U;
        }
    }
    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        const BiplaneDuelEnemyBullet *bullet =
            &game->enemy_bullets[i];

        if ((bullet->active != 0U) &&
            (BiplaneDuel_PointHitsRect(
                bullet->x, bullet->y,
                player_x, player_y,
                BIPLANE_DUEL_PLAYER_HALF_WIDTH - 4.0f,
                BIPLANE_DUEL_PLAYER_HALF_HEIGHT - 3.0f) != 0U)) {
            return 1U;
        }
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        const BiplaneDuelEnemy *enemy = &game->enemies[i];

        if ((enemy->active != 0U) &&
            (BiplaneDuel_PointHitsRect(
                enemy->x, enemy->y,
                player_x, player_y,
                BIPLANE_DUEL_PLAYER_HALF_WIDTH +
                BIPLANE_DUEL_ENEMY_HALF_WIDTH - 5.0f,
                BIPLANE_DUEL_PLAYER_HALF_HEIGHT +
                BIPLANE_DUEL_ENEMY_HALF_HEIGHT - 4.0f) != 0U)) {
            return 1U;
        }
    }
    if ((game->zeppelin.active != 0U) &&
        (BiplaneDuel_PointHitsRect(
            player_x, player_y,
            game->zeppelin.x, game->zeppelin.y,
            42.0f, 20.0f) != 0U)) {
        return 1U;
    }
    return 0U;
}

static BiplaneDuelEvent BiplaneDuel_PlayerCollisions(
    BiplaneDuelState *game) {
    if (game->invulnerable_ticks != 0U) {
        return BIPLANE_DUEL_EVENT_NONE;
    }
    if (BiplaneDuel_PlayerHitByObjects(game) == 0U) {
        return BIPLANE_DUEL_EVENT_NONE;
    }

    BiplaneDuel_AddExplosion(
        game, game->player.x, game->player.y, 11U);
    if (game->shields > 0U) {
        game->shields--;
    }
    if (game->score >= 25U) {
        game->score -= 25U;
    } else {
        game->score = 0U;
    }

    if (game->shields == 0U) {
        BiplaneDuel_ClearObjects(game);
        game->zeppelin.active = 0U;
        game->zeppelin.respawn_ticks = 0U;
        game->phase = BIPLANE_DUEL_PHASE_RESTART_PAUSE;
        game->phase_ticks = BIPLANE_RESTART_PAUSE_TICKS;
        return BIPLANE_DUEL_EVENT_HUD_CHANGED |
               BIPLANE_DUEL_EVENT_PLAYER_HIT |
               BIPLANE_DUEL_EVENT_RESTART_PAUSE;
    }

    BiplaneDuel_ResetPlayer(game);
    game->invulnerable_ticks = BIPLANE_INVULNERABLE_TICKS;
    return BIPLANE_DUEL_EVENT_HUD_CHANGED |
           BIPLANE_DUEL_EVENT_PLAYER_HIT;
}

static BiplaneDuelEvent BiplaneDuel_UpdateLevel(
    BiplaneDuelState *game) {
    uint8_t new_level =
        (uint8_t)(1U + (game->kills / BIPLANE_KILLS_PER_LEVEL));

    if (new_level == game->level) {
        return BIPLANE_DUEL_EVENT_NONE;
    }
    game->level = new_level;
    if (game->zeppelin.active != 0U) {
        game->zeppelin.vx =
            (game->zeppelin.vx < 0.0f) ?
            -BiplaneDuel_Abs(game->zeppelin.vx) :
            BiplaneDuel_Abs(game->zeppelin.vx);
    }
    return BIPLANE_DUEL_EVENT_HUD_CHANGED;
}

void BiplaneDuel_Init(BiplaneDuelState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x4C6E9F21U;
    BiplaneDuel_ResetGame(game);
}

BiplaneDuelEvent BiplaneDuel_Update(BiplaneDuelState *game,
                                    int16_t accel_x,
                                    int16_t accel_y) {
    BiplaneDuelEvent event = BIPLANE_DUEL_EVENT_NONE;

    game->ticks++;
    if (game->phase == BIPLANE_DUEL_PHASE_RESTART_PAUSE) {
        if (game->phase_ticks > 0U) {
            game->phase_ticks--;
        }
        if (game->phase_ticks == 0U) {
            BiplaneDuel_ResetGame(game);
            return BIPLANE_DUEL_EVENT_HUD_CHANGED |
                   BIPLANE_DUEL_EVENT_GAME_STARTED;
        }
        BiplaneDuel_UpdateExplosions(game);
        return event;
    }

    BiplaneDuel_UpdatePlayer(game, accel_x, accel_y);
    if (game->invulnerable_ticks > 0U) {
        game->invulnerable_ticks--;
    }

    if (game->fire_ticks > 0U) {
        game->fire_ticks--;
    }
    if (game->fire_ticks == 0U) {
        uint8_t reduction =
            (game->level > 8U) ? 3U : (game->level / 3U);

        BiplaneDuel_Fire(game);
        game->fire_ticks =
            (uint8_t)(BIPLANE_FIRE_INTERVAL - reduction);
    }

    if (game->spawn_ticks > 0U) {
        game->spawn_ticks--;
    }
    if (game->spawn_ticks == 0U) {
        BiplaneDuel_SpawnEnemy(game);
        game->spawn_ticks =
            BiplaneDuel_EnemySpawnInterval(game);
    }

    BiplaneDuel_UpdateZeppelin(game);
    BiplaneDuel_UpdateBullets(game);
    BiplaneDuel_UpdateEnemyBullets(game);
    BiplaneDuel_UpdateBombs(game);
    BiplaneDuel_UpdateEnemies(game);
    event |= BiplaneDuel_BulletCollisions(game);
    event |= BiplaneDuel_PlayerCollisions(game);
    event |= BiplaneDuel_UpdateLevel(game);
    BiplaneDuel_UpdateExplosions(game);
    return event;
}
