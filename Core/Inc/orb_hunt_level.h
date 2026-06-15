#ifndef INC_ORB_HUNT_LEVEL_H_
#define INC_ORB_HUNT_LEVEL_H_

#include <stdint.h>

#define ORB_HUNT_MAX_TARGETS       8U
#define ORB_HUNT_TARGET_RADIUS     7
#define ORB_HUNT_TARGET_MIN_GAP    38

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
} OrbHuntTarget;

typedef struct {
    uint16_t number;
    uint8_t target_count;
    uint8_t collected_count;
    uint8_t theme;
    OrbHuntTarget targets[ORB_HUNT_MAX_TARGETS];
} OrbHuntLevel;

void OrbHuntLevel_Generate(OrbHuntLevel *level,
                           uint16_t level_number,
                           uint32_t *rng_state,
                           uint16_t screen_width,
                           uint16_t screen_height,
                           uint16_t playfield_top,
                           int16_t start_x,
                           int16_t start_y);

#endif /* INC_ORB_HUNT_LEVEL_H_ */
