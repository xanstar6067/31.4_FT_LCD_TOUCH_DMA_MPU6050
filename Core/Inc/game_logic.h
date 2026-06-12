#ifndef INC_GAME_LOGIC_H_
#define INC_GAME_LOGIC_H_

#include <stdint.h>
#include "game_level.h"

#define GAME_SCREEN_WIDTH       320
#define GAME_SCREEN_HEIGHT      240
#define GAME_PLAYFIELD_TOP      28

#define BALL_RADIUS             15
#define BALL_MAX_SPEED          6.0f
#define BALL_ACCEL_SCALE        0.00012f
#define BALL_FRICTION           0.94f
#define BALL_BOUNCE_DAMPING     0.72f
#define BALL_INPUT_FILTER       0.20f
#define BALL_INPUT_DEAD_ZONE    260.0f

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} BallState;

typedef struct {
    BallState ball;
    GameLevel level;
    uint32_t rng_state;
    float filtered_accel_x;
    float filtered_accel_y;
} GameState;

typedef uint8_t GameEvent;

#define GAME_EVENT_NONE              0x00U
#define GAME_EVENT_TARGET_COLLECTED  0x01U
#define GAME_EVENT_LEVEL_STARTED     0x02U

void Game_Init(GameState *game, uint32_t seed);
GameEvent Game_Update(GameState *game, int16_t accel_x, int16_t accel_y);
uint8_t Game_TargetsRemaining(const GameState *game);

#endif /* INC_GAME_LOGIC_H_ */
