#ifndef INC_SPACE_DODGER_LOGIC_H_
#define INC_SPACE_DODGER_LOGIC_H_

#include <stdint.h>

#define SPACE_DODGER_SCREEN_WIDTH       320
#define SPACE_DODGER_SCREEN_HEIGHT      240
#define SPACE_DODGER_PLAYFIELD_TOP      28

#define SPACE_DODGER_SHIP_WIDTH         25
#define SPACE_DODGER_SHIP_HEIGHT        19
#define SPACE_DODGER_MAX_ASTEROIDS      8U
#define SPACE_DODGER_MAX_BULLETS        6U
#define SPACE_DODGER_STARTING_SHIELDS   3U

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} SpaceDodgerShip;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t radius;
    uint8_t health;
    uint8_t style;
    uint8_t active;
} SpaceDodgerAsteroid;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t active;
} SpaceDodgerBullet;

typedef enum {
    SPACE_DODGER_PHASE_PLAYING = 0,
    SPACE_DODGER_PHASE_RESTART_PAUSE
} SpaceDodgerPhase;

typedef struct {
    SpaceDodgerShip ship;
    SpaceDodgerAsteroid asteroids[SPACE_DODGER_MAX_ASTEROIDS];
    SpaceDodgerBullet bullets[SPACE_DODGER_MAX_BULLETS];
    uint32_t rng_state;
    uint32_t background_seed;
    uint32_t score;
    uint32_t ticks;
    float filtered_accel_x;
    float filtered_accel_y;
    uint16_t kills;
    uint16_t phase_ticks;
    uint8_t level;
    uint8_t shields;
    uint8_t spawn_ticks;
    uint8_t fire_ticks;
    uint8_t invulnerable_ticks;
    SpaceDodgerPhase phase;
} SpaceDodgerState;

typedef uint8_t SpaceDodgerEvent;

#define SPACE_DODGER_EVENT_NONE           0x00U
#define SPACE_DODGER_EVENT_HUD_CHANGED    0x01U
#define SPACE_DODGER_EVENT_GAME_STARTED   0x02U
#define SPACE_DODGER_EVENT_RESTART_PAUSE  0x04U

void SpaceDodger_Init(SpaceDodgerState *game, uint32_t seed);
SpaceDodgerEvent SpaceDodger_Update(SpaceDodgerState *game,
                                    int16_t accel_x,
                                    int16_t accel_y);

#endif /* INC_SPACE_DODGER_LOGIC_H_ */
