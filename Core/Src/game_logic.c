#include "game_logic.h"

static float Game_ApplyDeadZone(float value) {
    if ((value > -BALL_INPUT_DEAD_ZONE) &&
        (value < BALL_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }

    if (value > 0.0f) {
        return value - BALL_INPUT_DEAD_ZONE;
    }
    return value + BALL_INPUT_DEAD_ZONE;
}

static float Game_Clamp(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void Game_ResetBall(BallState *ball) {
    ball->x = GAME_SCREEN_WIDTH / 2.0f;
    ball->y = GAME_PLAYFIELD_TOP +
              ((GAME_SCREEN_HEIGHT - GAME_PLAYFIELD_TOP) / 2.0f);
    ball->vx = 0.0f;
    ball->vy = 0.0f;
}

static void Game_StartLevel(GameState *game, uint16_t level_number) {
    Game_ResetBall(&game->ball);
    game->filtered_accel_x = 0.0f;
    game->filtered_accel_y = 0.0f;

    GameLevel_Generate(&game->level,
                       level_number,
                       &game->rng_state,
                       GAME_SCREEN_WIDTH,
                       GAME_SCREEN_HEIGHT,
                       GAME_PLAYFIELD_TOP,
                       (int16_t)game->ball.x,
                       (int16_t)game->ball.y);
}

static GameEvent Game_CollectTargets(GameState *game) {
    GameEvent event = GAME_EVENT_NONE;
    const int32_t collision_radius = BALL_RADIUS + GAME_TARGET_RADIUS;
    const int32_t collision_radius_squared =
        collision_radius * collision_radius;

    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        GameTarget *target = &game->level.targets[i];
        int32_t dx;
        int32_t dy;

        if (target->active == 0U) {
            continue;
        }

        dx = (int32_t)game->ball.x - target->x;
        dy = (int32_t)game->ball.y - target->y;
        if (((dx * dx) + (dy * dy)) <= collision_radius_squared) {
            target->active = 0U;
            game->level.collected_count++;
            event |= GAME_EVENT_TARGET_COLLECTED;
        }
    }

    return event;
}

void Game_Init(GameState *game, uint32_t seed) {
    game->rng_state = (seed != 0U) ? seed : 0xA341316CU;
    Game_StartLevel(game, 1U);
}

GameEvent Game_Update(GameState *game, int16_t accel_x, int16_t accel_y) {
    GameEvent event;
    float control_x;
    float control_y;
    const float minimum_x = BALL_RADIUS;
    const float maximum_x = GAME_SCREEN_WIDTH - BALL_RADIUS - 1.0f;
    const float minimum_y = GAME_PLAYFIELD_TOP + BALL_RADIUS;
    const float maximum_y = GAME_SCREEN_HEIGHT - BALL_RADIUS - 1.0f;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) * BALL_INPUT_FILTER;
    game->filtered_accel_y +=
        ((float)accel_y - game->filtered_accel_y) * BALL_INPUT_FILTER;

    control_x = Game_ApplyDeadZone(game->filtered_accel_x);
    control_y = Game_ApplyDeadZone(game->filtered_accel_y);

    game->ball.vx += control_x * BALL_ACCEL_SCALE;
    game->ball.vy += control_y * BALL_ACCEL_SCALE;
    game->ball.vx *= BALL_FRICTION;
    game->ball.vy *= BALL_FRICTION;
    game->ball.vx = Game_Clamp(game->ball.vx,
                               -BALL_MAX_SPEED, BALL_MAX_SPEED);
    game->ball.vy = Game_Clamp(game->ball.vy,
                               -BALL_MAX_SPEED, BALL_MAX_SPEED);

    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    if (game->ball.x < minimum_x) {
        game->ball.x = minimum_x;
        game->ball.vx = -game->ball.vx * BALL_BOUNCE_DAMPING;
    } else if (game->ball.x > maximum_x) {
        game->ball.x = maximum_x;
        game->ball.vx = -game->ball.vx * BALL_BOUNCE_DAMPING;
    }

    if (game->ball.y < minimum_y) {
        game->ball.y = minimum_y;
        game->ball.vy = -game->ball.vy * BALL_BOUNCE_DAMPING;
    } else if (game->ball.y > maximum_y) {
        game->ball.y = maximum_y;
        game->ball.vy = -game->ball.vy * BALL_BOUNCE_DAMPING;
    }

    event = Game_CollectTargets(game);
    if (game->level.collected_count >= game->level.target_count) {
        uint16_t next_level = game->level.number + 1U;
        Game_StartLevel(game, next_level);
        event |= GAME_EVENT_LEVEL_STARTED;
    }

    return event;
}

uint8_t Game_TargetsRemaining(const GameState *game) {
    return game->level.target_count - game->level.collected_count;
}
