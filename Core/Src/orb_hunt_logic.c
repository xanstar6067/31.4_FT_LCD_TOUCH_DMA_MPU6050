#include "orb_hunt_logic.h"

static float OrbHunt_ApplyDeadZone(float value) {
    if ((value > -ORB_HUNT_INPUT_DEAD_ZONE) &&
        (value < ORB_HUNT_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }

    if (value > 0.0f) {
        return value - ORB_HUNT_INPUT_DEAD_ZONE;
    }
    return value + ORB_HUNT_INPUT_DEAD_ZONE;
}

static float OrbHunt_Clamp(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void OrbHunt_ResetBall(OrbHuntBallState *ball) {
    ball->x = ORB_HUNT_SCREEN_WIDTH / 2.0f;
    ball->y = ORB_HUNT_PLAYFIELD_TOP +
              ((ORB_HUNT_SCREEN_HEIGHT - ORB_HUNT_PLAYFIELD_TOP) / 2.0f);
    ball->vx = 0.0f;
    ball->vy = 0.0f;
}

static void OrbHunt_StartLevel(OrbHuntState *game, uint16_t level_number) {
    OrbHunt_ResetBall(&game->ball);
    game->filtered_accel_x = 0.0f;
    game->filtered_accel_y = 0.0f;

    OrbHuntLevel_Generate(&game->level,
                          level_number,
                          &game->rng_state,
                          ORB_HUNT_SCREEN_WIDTH,
                          ORB_HUNT_SCREEN_HEIGHT,
                          ORB_HUNT_PLAYFIELD_TOP,
                          (int16_t)game->ball.x,
                          (int16_t)game->ball.y);
}

static OrbHuntEvent OrbHunt_CollectTargets(OrbHuntState *game) {
    OrbHuntEvent event = ORB_HUNT_EVENT_NONE;
    const int32_t collision_radius =
        ORB_HUNT_BALL_RADIUS + ORB_HUNT_TARGET_RADIUS;
    const int32_t collision_radius_squared =
        collision_radius * collision_radius;

    for (uint8_t i = 0U; i < game->level.target_count; i++) {
        OrbHuntTarget *target = &game->level.targets[i];
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
            event |= ORB_HUNT_EVENT_TARGET_COLLECTED;
        }
    }

    return event;
}

void OrbHunt_Init(OrbHuntState *game, uint32_t seed) {
    game->rng_state = (seed != 0U) ? seed : 0xA341316CU;
    OrbHunt_StartLevel(game, 1U);
}

OrbHuntEvent OrbHunt_Update(OrbHuntState *game,
                            int16_t accel_x,
                            int16_t accel_y) {
    OrbHuntEvent event;
    float control_x;
    float control_y;
    const float minimum_x = ORB_HUNT_BALL_RADIUS;
    const float maximum_x =
        ORB_HUNT_SCREEN_WIDTH - ORB_HUNT_BALL_RADIUS - 1.0f;
    const float minimum_y =
        ORB_HUNT_PLAYFIELD_TOP + ORB_HUNT_BALL_RADIUS;
    const float maximum_y =
        ORB_HUNT_SCREEN_HEIGHT - ORB_HUNT_BALL_RADIUS - 1.0f;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) * ORB_HUNT_INPUT_FILTER;
    game->filtered_accel_y +=
        ((float)accel_y - game->filtered_accel_y) * ORB_HUNT_INPUT_FILTER;

    control_x = OrbHunt_ApplyDeadZone(game->filtered_accel_x);
    control_y = OrbHunt_ApplyDeadZone(game->filtered_accel_y);

    game->ball.vx += control_x * ORB_HUNT_BALL_ACCEL_SCALE;
    game->ball.vy += control_y * ORB_HUNT_BALL_ACCEL_SCALE;
    game->ball.vx *= ORB_HUNT_BALL_FRICTION;
    game->ball.vy *= ORB_HUNT_BALL_FRICTION;
    game->ball.vx = OrbHunt_Clamp(game->ball.vx,
                                  -ORB_HUNT_BALL_MAX_SPEED,
                                  ORB_HUNT_BALL_MAX_SPEED);
    game->ball.vy = OrbHunt_Clamp(game->ball.vy,
                                  -ORB_HUNT_BALL_MAX_SPEED,
                                  ORB_HUNT_BALL_MAX_SPEED);

    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    if (game->ball.x < minimum_x) {
        game->ball.x = minimum_x;
        game->ball.vx = -game->ball.vx * ORB_HUNT_BOUNCE_DAMPING;
    } else if (game->ball.x > maximum_x) {
        game->ball.x = maximum_x;
        game->ball.vx = -game->ball.vx * ORB_HUNT_BOUNCE_DAMPING;
    }

    if (game->ball.y < minimum_y) {
        game->ball.y = minimum_y;
        game->ball.vy = -game->ball.vy * ORB_HUNT_BOUNCE_DAMPING;
    } else if (game->ball.y > maximum_y) {
        game->ball.y = maximum_y;
        game->ball.vy = -game->ball.vy * ORB_HUNT_BOUNCE_DAMPING;
    }

    event = OrbHunt_CollectTargets(game);
    if (game->level.collected_count >= game->level.target_count) {
        uint16_t next_level = game->level.number + 1U;
        OrbHunt_StartLevel(game, next_level);
        event |= ORB_HUNT_EVENT_LEVEL_STARTED;
    }

    return event;
}

uint8_t OrbHunt_TargetsRemaining(const OrbHuntState *game) {
    return game->level.target_count - game->level.collected_count;
}
