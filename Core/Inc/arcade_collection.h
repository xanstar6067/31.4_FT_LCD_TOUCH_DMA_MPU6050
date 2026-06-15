#ifndef INC_ARCADE_COLLECTION_H_
#define INC_ARCADE_COLLECTION_H_

#include <stdint.h>
#include "gravity_pong_logic.h"
#include "orb_hunt_logic.h"
#include "tilt_breaker_logic.h"

typedef enum {
    ARCADE_GAME_ORB_HUNT = 0,
    ARCADE_GAME_GRAVITY_PONG,
    ARCADE_GAME_TILT_BREAKER,
    ARCADE_GAME_COUNT
} ArcadeGameId;

typedef struct {
    ArcadeGameId active_game;
    uint32_t seed;
    OrbHuntState orb_hunt;
    GravityPongState gravity_pong;
    TiltBreakerState tilt_breaker;
} ArcadeCollection;

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed);
void ArcadeCollection_Update(ArcadeCollection *arcade,
                             int16_t accel_x,
                             int16_t accel_y);
void ArcadeCollection_NextGame(ArcadeCollection *arcade);
void ArcadeCollection_RestartGame(ArcadeCollection *arcade);

#endif /* INC_ARCADE_COLLECTION_H_ */
