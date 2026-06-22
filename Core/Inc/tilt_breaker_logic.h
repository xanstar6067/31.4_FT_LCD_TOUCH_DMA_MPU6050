#ifndef INC_TILT_BREAKER_LOGIC_H_
#define INC_TILT_BREAKER_LOGIC_H_

#include <stdint.h>

#define TILT_BREAKER_SCREEN_WIDTH       320
#define TILT_BREAKER_SCREEN_HEIGHT      240
#define TILT_BREAKER_PLAYFIELD_TOP      28

#define TILT_BREAKER_BALL_RADIUS        5
#define TILT_BREAKER_PADDLE_Y           225
#define TILT_BREAKER_PADDLE_WIDTH       70
#define TILT_BREAKER_PADDLE_HEIGHT      7

#define TILT_BREAKER_BRICK_COLUMNS      8U
#define TILT_BREAKER_BRICK_ROWS         6U
#define TILT_BREAKER_BRICK_COUNT        \
    (TILT_BREAKER_BRICK_COLUMNS * TILT_BREAKER_BRICK_ROWS)
#define TILT_BREAKER_BRICK_WIDTH        30
#define TILT_BREAKER_BRICK_HEIGHT       11
#define TILT_BREAKER_BRICK_GAP_X        8
#define TILT_BREAKER_BRICK_GAP_Y        7
#define TILT_BREAKER_BRICK_LEFT         12
#define TILT_BREAKER_BRICK_TOP          44

#define TILT_BREAKER_BONUS_WIDTH        14
#define TILT_BREAKER_BONUS_HEIGHT       8

#define TILT_BREAKER_STARTING_LIVES     3U
#define TILT_BREAKER_NO_BRICK           0xFFU

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} TiltBreakerBall;

typedef struct {
    float x;
    float vx;
    uint16_t width;
} TiltBreakerPaddle;

typedef enum {
    TILT_BREAKER_BONUS_NONE = 0,
    TILT_BREAKER_BONUS_WIDE,
    TILT_BREAKER_BONUS_SLOW,
    TILT_BREAKER_BONUS_LIFE
} TiltBreakerBonusType;

typedef struct {
    float x;
    float y;
    TiltBreakerBonusType type;
    uint8_t active;
} TiltBreakerBonus;

typedef enum {
    TILT_BREAKER_PHASE_PLAYING = 0,
    TILT_BREAKER_PHASE_SERVE_PAUSE,
    TILT_BREAKER_PHASE_LEVEL_PAUSE,
    TILT_BREAKER_PHASE_GAME_OVER_PAUSE
} TiltBreakerPhase;

typedef struct {
    TiltBreakerBall ball;
    TiltBreakerPaddle paddle;
    TiltBreakerBonus bonus;
    uint32_t rng_state;
    float filtered_accel_x;
    uint32_t score;
    uint16_t level;
    uint16_t phase_ticks;
    uint16_t wide_ticks;
    uint8_t lives;
    uint8_t bricks_remaining;
    uint8_t changed_brick;
    uint8_t bricks[TILT_BREAKER_BRICK_COUNT];
    TiltBreakerPhase phase;
} TiltBreakerState;

typedef uint8_t TiltBreakerEvent;

#define TILT_BREAKER_EVENT_NONE             0x00U
#define TILT_BREAKER_EVENT_BRICK_CHANGED    0x01U
#define TILT_BREAKER_EVENT_LIFE_CHANGED     0x02U
#define TILT_BREAKER_EVENT_ROUND_STARTED    0x04U
#define TILT_BREAKER_EVENT_LEVEL_STARTED    0x08U
#define TILT_BREAKER_EVENT_GAME_STARTED     0x10U
#define TILT_BREAKER_EVENT_BONUS_CHANGED    0x20U

void TiltBreaker_Init(TiltBreakerState *game, uint32_t seed);
TiltBreakerEvent TiltBreaker_Update(TiltBreakerState *game,
                                    int16_t accel_x);
void TiltBreaker_GetBrickRect(uint8_t index,
                              int16_t *x,
                              int16_t *y,
                              int16_t *width,
                              int16_t *height);

#endif /* INC_TILT_BREAKER_LOGIC_H_ */
