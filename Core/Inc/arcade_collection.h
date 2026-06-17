#ifndef INC_ARCADE_COLLECTION_H_
#define INC_ARCADE_COLLECTION_H_

#include <stdint.h>
#include "balance_tower_logic.h"
#include "biplane_duel_logic.h"
#include "gravity_pong_logic.h"
#include "life_game_logic.h"
#include "marble_maze_logic.h"
#include "mpu6050_page.h"
#include "orb_hunt_logic.h"
#include "shake_flight_logic.h"
#include "space_dodger_logic.h"
#include "system_info_page.h"
#include "tilt_breaker_logic.h"

typedef enum {
    ARCADE_PAGE_SYSTEM_INFO = 0,
    ARCADE_PAGE_MPU6050,
    ARCADE_PAGE_ORB_HUNT,
    ARCADE_PAGE_GRAVITY_PONG,
    ARCADE_PAGE_TILT_BREAKER,
    ARCADE_PAGE_MARBLE_MAZE,
    ARCADE_PAGE_SPACE_DODGER,
    ARCADE_PAGE_BALANCE_TOWER,
    ARCADE_PAGE_SHAKE_FLIGHT,
    ARCADE_PAGE_BIPLANE_DUEL,
    ARCADE_PAGE_LIFE_GAME,
    ARCADE_PAGE_COUNT
} ArcadePageId;

typedef struct {
    ArcadePageId active_page;
    uint32_t seed;
    SystemInfoPage system_info;
    MPU6050Page mpu6050;
    OrbHuntState orb_hunt;
    GravityPongState gravity_pong;
    TiltBreakerState tilt_breaker;
    MarbleMazeState marble_maze;
    SpaceDodgerState space_dodger;
    BalanceTowerState balance_tower;
    ShakeFlightState shake_flight;
    BiplaneDuelState biplane_duel;
    LifeGameState life_game;
} ArcadeCollection;

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed);
void ArcadeCollection_Update(ArcadeCollection *arcade,
                             const MPU6050_Data_t *mpu_data,
                             HAL_StatusTypeDef mpu_status);
void ArcadeCollection_NextPage(ArcadeCollection *arcade);
void ArcadeCollection_RestartActive(ArcadeCollection *arcade);

#endif /* INC_ARCADE_COLLECTION_H_ */
