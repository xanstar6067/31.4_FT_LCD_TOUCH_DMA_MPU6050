#ifndef INC_GAME_LEVEL_H_
#define INC_GAME_LEVEL_H_

#include <stdint.h>

#define GAME_MAX_TARGETS       8U
#define GAME_TARGET_RADIUS     7
#define GAME_TARGET_MIN_GAP    38

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
} GameTarget;

typedef struct {
    uint16_t number;
    uint8_t target_count;
    uint8_t collected_count;
    uint8_t theme;
    GameTarget targets[GAME_MAX_TARGETS];
} GameLevel;

void GameLevel_Generate(GameLevel *level,
                        uint16_t level_number,
                        uint32_t *rng_state,
                        uint16_t screen_width,
                        uint16_t screen_height,
                        uint16_t playfield_top,
                        int16_t start_x,
                        int16_t start_y);

#endif /* INC_GAME_LEVEL_H_ */
