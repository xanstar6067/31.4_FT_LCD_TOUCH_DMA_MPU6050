#ifndef INC_BIPLANE_DUEL_LOGIC_H_
#define INC_BIPLANE_DUEL_LOGIC_H_

#include <stdint.h>

#define BIPLANE_DUEL_SCREEN_WIDTH       320
#define BIPLANE_DUEL_SCREEN_HEIGHT      240
#define BIPLANE_DUEL_PLAYFIELD_TOP      28
#define BIPLANE_DUEL_GROUND_Y           214

#define BIPLANE_DUEL_PLAYER_HALF_WIDTH  16
#define BIPLANE_DUEL_PLAYER_HALF_HEIGHT 10
#define BIPLANE_DUEL_ZEPPELIN_WIDTH     66
#define BIPLANE_DUEL_ZEPPELIN_HEIGHT    26
#define BIPLANE_DUEL_ENEMY_HALF_WIDTH   15
#define BIPLANE_DUEL_ENEMY_HALF_HEIGHT  9
#define BIPLANE_DUEL_MAX_BULLETS        7U
#define BIPLANE_DUEL_MAX_ENEMY_BULLETS  5U
#define BIPLANE_DUEL_MAX_BOMBS          4U
#define BIPLANE_DUEL_MAX_ENEMIES        3U
#define BIPLANE_DUEL_MAX_EXPLOSIONS     4U
#define BIPLANE_DUEL_STARTING_SHIELDS   3U

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int8_t facing;
} BiplaneDuelPlayer;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t active;
} BiplaneDuelBullet;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t active;
} BiplaneDuelEnemyBullet;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t active;
} BiplaneDuelBomb;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int8_t facing;
    uint8_t fire_ticks;
    uint8_t health;
    uint8_t active;
} BiplaneDuelEnemy;

typedef struct {
    float x;
    float y;
    float vx;
    uint16_t respawn_ticks;
    uint16_t bomb_ticks;
    uint8_t health;
    uint8_t active;
} BiplaneDuelZeppelin;

typedef struct {
    float x;
    float y;
    uint8_t radius;
    uint8_t ticks;
    uint8_t active;
} BiplaneDuelExplosion;

typedef enum {
    BIPLANE_DUEL_PHASE_PLAYING = 0,
    BIPLANE_DUEL_PHASE_RESTART_PAUSE
} BiplaneDuelPhase;

typedef struct {
    BiplaneDuelPlayer player;
    BiplaneDuelZeppelin zeppelin;
    BiplaneDuelBullet bullets[BIPLANE_DUEL_MAX_BULLETS];
    BiplaneDuelEnemyBullet enemy_bullets[
        BIPLANE_DUEL_MAX_ENEMY_BULLETS];
    BiplaneDuelBomb bombs[BIPLANE_DUEL_MAX_BOMBS];
    BiplaneDuelEnemy enemies[BIPLANE_DUEL_MAX_ENEMIES];
    BiplaneDuelExplosion explosions[BIPLANE_DUEL_MAX_EXPLOSIONS];
    uint32_t rng_state;
    uint32_t score;
    uint32_t ticks;
    float filtered_accel_x;
    float filtered_accel_y;
    uint16_t kills;
    uint16_t phase_ticks;
    uint16_t spawn_ticks;
    uint8_t fire_ticks;
    uint8_t level;
    uint8_t shields;
    uint8_t invulnerable_ticks;
    BiplaneDuelPhase phase;
} BiplaneDuelState;

typedef uint8_t BiplaneDuelEvent;

#define BIPLANE_DUEL_EVENT_NONE          0x00U
#define BIPLANE_DUEL_EVENT_HUD_CHANGED   0x01U
#define BIPLANE_DUEL_EVENT_GAME_STARTED  0x02U
#define BIPLANE_DUEL_EVENT_PLAYER_HIT    0x04U
#define BIPLANE_DUEL_EVENT_RESTART_PAUSE 0x08U

void BiplaneDuel_Init(BiplaneDuelState *game, uint32_t seed);
BiplaneDuelEvent BiplaneDuel_Update(BiplaneDuelState *game,
                                    int16_t accel_x,
                                    int16_t accel_y);

#endif /* INC_BIPLANE_DUEL_LOGIC_H_ */
