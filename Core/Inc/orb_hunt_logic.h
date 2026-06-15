#ifndef INC_ORB_HUNT_LOGIC_H_
#define INC_ORB_HUNT_LOGIC_H_

#include <stdint.h>
#include "orb_hunt_level.h"

#define ORB_HUNT_SCREEN_WIDTH       320
#define ORB_HUNT_SCREEN_HEIGHT      240
#define ORB_HUNT_PLAYFIELD_TOP      28

#define ORB_HUNT_BALL_RADIUS          15
#define ORB_HUNT_BALL_MAX_SPEED       6.0f
#define ORB_HUNT_BALL_ACCEL_SCALE     0.00012f
#define ORB_HUNT_BALL_FRICTION        0.94f
#define ORB_HUNT_BOUNCE_DAMPING       0.72f
#define ORB_HUNT_INPUT_FILTER         0.20f
#define ORB_HUNT_INPUT_DEAD_ZONE      260.0f

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} OrbHuntBallState;

typedef struct {
    OrbHuntBallState ball;
    OrbHuntLevel level;
    uint32_t rng_state;
    float filtered_accel_x;
    float filtered_accel_y;
} OrbHuntState;

typedef uint8_t OrbHuntEvent;

#define ORB_HUNT_EVENT_NONE              0x00U
#define ORB_HUNT_EVENT_TARGET_COLLECTED  0x01U
#define ORB_HUNT_EVENT_LEVEL_STARTED     0x02U

void OrbHunt_Init(OrbHuntState *game, uint32_t seed);
OrbHuntEvent OrbHunt_Update(OrbHuntState *game,
                            int16_t accel_x,
                            int16_t accel_y);
uint8_t OrbHunt_TargetsRemaining(const OrbHuntState *game);

#endif /* INC_ORB_HUNT_LOGIC_H_ */
