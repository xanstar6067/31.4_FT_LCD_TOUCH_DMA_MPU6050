#ifndef INC_ARCADE_COLLECTION_H_
#define INC_ARCADE_COLLECTION_H_

#include <stdint.h>

#define ARCADE_ENABLE_BALANCE_TOWER 0
#define ARCADE_ENABLE_BIPLANE_DUEL 0

#if (ARCADE_ENABLE_BALANCE_TOWER == 0)
#warning "Balance Tower is disabled: sources are excluded from the project build."
#endif

#if (ARCADE_ENABLE_BIPLANE_DUEL == 0)
#warning "Biplane Duel is disabled: sources are excluded from the project build."
#endif

#if ARCADE_ENABLE_BALANCE_TOWER
#include "balance_tower_logic.h"
#endif

#if ARCADE_ENABLE_BIPLANE_DUEL
#include "biplane_duel_logic.h"
#endif

#include "gravity_pong_logic.h"
#include "life_game_logic.h"
#include "marble_maze_logic.h"
#include "micro_colony_logic.h"
#include "mpu6000_page.h"
#include "orb_hunt_logic.h"
#include "shake_flight_logic.h"
#include "space_dodger_logic.h"
#include "system_info_page.h"
#include "tetris_logic.h"
#include "tilt_breaker_logic.h"
#include "touch_test_page.h"

typedef enum {
    ARCADE_PAGE_SYSTEM_INFO = 0,
    ARCADE_PAGE_MPU6000,
    ARCADE_PAGE_TOUCH_TEST,
    ARCADE_PAGE_ORB_HUNT,
    ARCADE_PAGE_GRAVITY_PONG,
    ARCADE_PAGE_TILT_BREAKER,
    ARCADE_PAGE_MARBLE_MAZE,
    ARCADE_PAGE_SPACE_DODGER,
#if ARCADE_ENABLE_BALANCE_TOWER
    ARCADE_PAGE_BALANCE_TOWER,
#endif
    ARCADE_PAGE_SHAKE_FLIGHT,
#if ARCADE_ENABLE_BIPLANE_DUEL
    ARCADE_PAGE_BIPLANE_DUEL,
#endif
    ARCADE_PAGE_LIFE_GAME,
    ARCADE_PAGE_MICRO_COLONY,
    ARCADE_PAGE_TETRIS,
    ARCADE_PAGE_COUNT
} ArcadePageId;

typedef struct {
    ArcadePageId active_page;
    uint32_t seed;
    uint32_t sensor_entropy;
    SystemInfoPage system_info;
    MPU6000Page mpu6000;
    TouchTestPage touch_test;
    OrbHuntState orb_hunt;
    GravityPongState gravity_pong;
    TiltBreakerState tilt_breaker;
    MarbleMazeState marble_maze;
    SpaceDodgerState space_dodger;
#if ARCADE_ENABLE_BALANCE_TOWER
    BalanceTowerState balance_tower;
#endif
    ShakeFlightState shake_flight;
#if ARCADE_ENABLE_BIPLANE_DUEL
    BiplaneDuelState biplane_duel;
#endif
    LifeGameState life_game;
    MicroColonyState micro_colony;
    TetrisState tetris;
} ArcadeCollection;

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed);
void ArcadeCollection_Update(ArcadeCollection *arcade,
                             const MPU6000_Data_t *mpu_data,
                             HAL_StatusTypeDef mpu_status);
void ArcadeCollection_NextPage(ArcadeCollection *arcade);
void ArcadeCollection_RestartActive(ArcadeCollection *arcade);

#endif /* INC_ARCADE_COLLECTION_H_ */
