#include "arcade_collection.h"

#include "balance_tower_render.h"
#include "biplane_duel_render.h"
#include "gravity_pong_render.h"
#include "life_game_render.h"
#include "marble_maze_render.h"
#include "micro_colony_render.h"
#include "orb_hunt_render.h"
#include "render_scratch.h"
#include "shake_flight_render.h"
#include "space_dodger_render.h"
#include "tilt_breaker_render.h"

uint8_t render_scratch_buffer[RENDER_SCRATCH_BUFFER_SIZE]
    __attribute__((aligned(4)));

static uint32_t ArcadeCollection_NextSeed(ArcadeCollection *arcade) {
    uint32_t value = arcade->seed;

    if (value == 0U) {
        value = 0x9E3779B9U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    arcade->seed = value;
    return value;
}

static void ArcadeCollection_RenderActive(ArcadeCollection *arcade) {
    switch (arcade->active_page) {
        case ARCADE_PAGE_SYSTEM_INFO:
            SystemInfoPage_Render(&arcade->system_info);
            break;
        case ARCADE_PAGE_MPU6050:
            arcade->mpu6050.renderer_initialized = 0U;
            MPU6050Page_Render(&arcade->mpu6050);
            break;
        case ARCADE_PAGE_ORB_HUNT:
            OrbHuntRender_Init(&arcade->orb_hunt);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPongRender_Init(&arcade->gravity_pong);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreakerRender_Init(&arcade->tilt_breaker);
            break;
        case ARCADE_PAGE_MARBLE_MAZE:
            MarbleMazeRender_Init(&arcade->marble_maze);
            break;
        case ARCADE_PAGE_SPACE_DODGER:
            SpaceDodgerRender_Init(&arcade->space_dodger);
            break;
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTowerRender_Init(&arcade->balance_tower);
            break;
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlightRender_Init(&arcade->shake_flight);
            break;
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuelRender_Init(&arcade->biplane_duel);
            break;
        case ARCADE_PAGE_LIFE_GAME:
            LifeGameRender_Init(&arcade->life_game);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColonyRender_Init(&arcade->micro_colony);
            break;
        default:
            break;
    }
}

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed) {
    arcade->seed = (seed != 0U) ? seed : 0xA341316CU;
    arcade->active_page = ARCADE_PAGE_SYSTEM_INFO;

    SystemInfoPage_Init(&arcade->system_info);
    MPU6050Page_Init(&arcade->mpu6050);
    OrbHunt_Init(&arcade->orb_hunt,
                 ArcadeCollection_NextSeed(arcade));
    GravityPong_Init(&arcade->gravity_pong,
                     ArcadeCollection_NextSeed(arcade));
    TiltBreaker_Init(&arcade->tilt_breaker,
                     ArcadeCollection_NextSeed(arcade));
    MarbleMaze_Init(&arcade->marble_maze,
                    ArcadeCollection_NextSeed(arcade));
    SpaceDodger_Init(&arcade->space_dodger,
                     ArcadeCollection_NextSeed(arcade));
    BalanceTower_Init(&arcade->balance_tower,
                      ArcadeCollection_NextSeed(arcade));
    ShakeFlight_Init(&arcade->shake_flight,
                     ArcadeCollection_NextSeed(arcade));
    BiplaneDuel_Init(&arcade->biplane_duel,
                     ArcadeCollection_NextSeed(arcade));
    LifeGame_Init(&arcade->life_game,
                  ArcadeCollection_NextSeed(arcade));
    MicroColony_Init(&arcade->micro_colony,
                     ArcadeCollection_NextSeed(arcade));
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_Update(ArcadeCollection *arcade,
                             const MPU6050_Data_t *mpu_data,
                             HAL_StatusTypeDef mpu_status) {
    switch (arcade->active_page) {
        case ARCADE_PAGE_SYSTEM_INFO:
            break;
        case ARCADE_PAGE_MPU6050:
            MPU6050Page_Update(&arcade->mpu6050,
                               mpu_data,
                               mpu_status);
            break;
        case ARCADE_PAGE_ORB_HUNT: {
            OrbHuntEvent event =
                OrbHunt_Update(&arcade->orb_hunt,
                               mpu_data->accel_x,
                               mpu_data->accel_y);
            OrbHuntRender_Frame(&arcade->orb_hunt, event);
            break;
        }
        case ARCADE_PAGE_GRAVITY_PONG: {
            GravityPongEvent event =
                GravityPong_Update(&arcade->gravity_pong,
                                   mpu_data->accel_x);
            GravityPongRender_Frame(&arcade->gravity_pong, event);
            break;
        }
        case ARCADE_PAGE_TILT_BREAKER: {
            TiltBreakerEvent event =
                TiltBreaker_Update(&arcade->tilt_breaker,
                                   mpu_data->accel_x);
            TiltBreakerRender_Frame(&arcade->tilt_breaker, event);
            break;
        }
        case ARCADE_PAGE_MARBLE_MAZE: {
            MarbleMazeEvent event =
                MarbleMaze_Update(&arcade->marble_maze,
                                  mpu_data->accel_x,
                                  mpu_data->accel_y);
            MarbleMazeRender_Frame(&arcade->marble_maze, event);
            break;
        }
        case ARCADE_PAGE_SPACE_DODGER: {
            SpaceDodgerEvent event =
                SpaceDodger_Update(&arcade->space_dodger,
                                   mpu_data->accel_x,
                                   mpu_data->accel_y);
            SpaceDodgerRender_Frame(&arcade->space_dodger, event);
            break;
        }
        case ARCADE_PAGE_BALANCE_TOWER: {
            BalanceTowerEvent event =
                BalanceTower_Update(&arcade->balance_tower,
                                    mpu_data->gyro_z);
            BalanceTowerRender_Frame(&arcade->balance_tower, event);
            break;
        }
        case ARCADE_PAGE_SHAKE_FLIGHT: {
            ShakeFlightEvent event =
                ShakeFlight_Update(&arcade->shake_flight,
                                   mpu_data->accel_z);
            ShakeFlightRender_Frame(&arcade->shake_flight, event);
            break;
        }
        case ARCADE_PAGE_BIPLANE_DUEL: {
            BiplaneDuelEvent event =
                BiplaneDuel_Update(&arcade->biplane_duel,
                                   mpu_data->accel_x,
                                   mpu_data->accel_y);
            BiplaneDuelRender_Frame(&arcade->biplane_duel, event);
            break;
        }
        case ARCADE_PAGE_LIFE_GAME: {
            LifeGameEvent event =
                LifeGame_Update(&arcade->life_game,
                                mpu_data,
                                mpu_status);
            LifeGameRender_Frame(&arcade->life_game, event);
            break;
        }
        case ARCADE_PAGE_MICRO_COLONY: {
            MicroColonyEvent event =
                MicroColony_Update(&arcade->micro_colony,
                                   mpu_data,
                                   mpu_status);
            MicroColonyRender_Frame(&arcade->micro_colony,
                                    event);
            break;
        }
        default:
            break;
    }
}

void ArcadeCollection_NextPage(ArcadeCollection *arcade) {
    arcade->active_page =
        (ArcadePageId)((arcade->active_page + 1U) %
                       ARCADE_PAGE_COUNT);
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_RestartActive(ArcadeCollection *arcade) {
    uint32_t seed = ArcadeCollection_NextSeed(arcade);

    switch (arcade->active_page) {
        case ARCADE_PAGE_SYSTEM_INFO:
            SystemInfoPage_Refresh(&arcade->system_info);
            break;
        case ARCADE_PAGE_MPU6050:
            MPU6050_Init();
            MPU6050Page_Init(&arcade->mpu6050);
            break;
        case ARCADE_PAGE_ORB_HUNT:
            OrbHunt_Init(&arcade->orb_hunt, seed);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPong_Init(&arcade->gravity_pong, seed);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreaker_Init(&arcade->tilt_breaker, seed);
            break;
        case ARCADE_PAGE_MARBLE_MAZE:
            MarbleMaze_Init(&arcade->marble_maze, seed);
            break;
        case ARCADE_PAGE_SPACE_DODGER:
            SpaceDodger_Init(&arcade->space_dodger, seed);
            break;
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTower_Init(&arcade->balance_tower, seed);
            break;
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlight_Init(&arcade->shake_flight, seed);
            break;
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuel_Init(&arcade->biplane_duel, seed);
            break;
        case ARCADE_PAGE_LIFE_GAME:
            LifeGame_Init(&arcade->life_game, seed);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColony_Init(&arcade->micro_colony, seed);
            break;
        default:
            break;
    }
    ArcadeCollection_RenderActive(arcade);
}
