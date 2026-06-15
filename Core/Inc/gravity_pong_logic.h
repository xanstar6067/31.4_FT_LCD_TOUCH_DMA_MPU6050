#ifndef INC_GRAVITY_PONG_LOGIC_H_
#define INC_GRAVITY_PONG_LOGIC_H_

#include <stdint.h>

#define GRAVITY_PONG_SCREEN_WIDTH       320
#define GRAVITY_PONG_SCREEN_HEIGHT      240
#define GRAVITY_PONG_PLAYFIELD_TOP      28

#define GRAVITY_PONG_BALL_RADIUS        5
#define GRAVITY_PONG_CORE_X             160
#define GRAVITY_PONG_CORE_Y             134
#define GRAVITY_PONG_CORE_RADIUS        12

#define GRAVITY_PONG_PLAYER_Y           225
#define GRAVITY_PONG_AI_Y               38
#define GRAVITY_PONG_PADDLE_HEIGHT      7
#define GRAVITY_PONG_PLAYER_WIDTH       70
#define GRAVITY_PONG_AI_WIDTH           56
#define GRAVITY_PONG_WIN_SCORE          5U

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} GravityPongBall;

typedef struct {
    float x;
    float vx;
    uint16_t width;
} GravityPongPaddle;

typedef enum {
    GRAVITY_PONG_PHASE_PLAYING = 0,
    GRAVITY_PONG_PHASE_POINT_PAUSE,
    GRAVITY_PONG_PHASE_MATCH_PAUSE
} GravityPongPhase;

typedef struct {
    GravityPongBall ball;
    GravityPongPaddle player;
    GravityPongPaddle ai;
    uint32_t rng_state;
    float filtered_accel_x;
    uint16_t rally;
    uint16_t phase_ticks;
    uint8_t player_score;
    uint8_t ai_score;
    uint8_t last_scorer;
    uint8_t match_winner;
    int8_t serve_direction;
    GravityPongPhase phase;
} GravityPongState;

typedef uint8_t GravityPongEvent;

#define GRAVITY_PONG_EVENT_NONE           0x00U
#define GRAVITY_PONG_EVENT_SCORE_CHANGED  0x01U
#define GRAVITY_PONG_EVENT_ROUND_STARTED  0x02U
#define GRAVITY_PONG_EVENT_MATCH_STARTED  0x04U

void GravityPong_Init(GravityPongState *game, uint32_t seed);
GravityPongEvent GravityPong_Update(GravityPongState *game,
                                    int16_t accel_x);

#endif /* INC_GRAVITY_PONG_LOGIC_H_ */
