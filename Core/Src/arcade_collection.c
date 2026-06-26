#include "arcade_collection.h"

#if ARCADE_ENABLE_BALANCE_TOWER
#include "balance_tower_render.h"
#endif
#if ARCADE_ENABLE_BIPLANE_DUEL
#include "biplane_duel_render.h"
#endif
#include "comet_catch_render.h"
#include "display_driver.h"
#include "gravity_pong_render.h"
#include "life_game_render.h"
#include "marble_maze_render.h"
#include "micro_colony_render.h"
#include "orb_hunt_render.h"
#include "render_scratch.h"
#include "shake_flight_render.h"
#include "space_dodger_render.h"
#include "tetris_render.h"
#include "tilt_breaker_render.h"

uint8_t render_scratch_buffer[RENDER_SCRATCH_BUFFER_SIZE]
    __attribute__((aligned(4)));

static uint8_t ArcadeCollection_ReadTouch(uint16_t *x,
                                          uint16_t *y) {
    if (DISPLAY_TouchPressed()) {
        return DISPLAY_TouchGetCoordinates(x, y) ? 1U : 0U;
    }
    return 0U;
}

static uint32_t ArcadeCollection_Mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static void ArcadeCollection_MixSensorEntropy(
    ArcadeCollection *arcade,
    const MPU6000_Data_t *mpu_data,
    HAL_StatusTypeDef mpu_status) {
    uint32_t value = arcade->sensor_entropy;

    value ^= HAL_GetTick() + 0x9E3779B9U +
             (value << 6) + (value >> 2);
    if (mpu_status == HAL_OK) {
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_x << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_z;
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_y << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_x;
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_z << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_y;
    }

    arcade->sensor_entropy = ArcadeCollection_Mix32(value);
}

static uint32_t ArcadeCollection_NextSeed(ArcadeCollection *arcade) {
    uint32_t value =
        arcade->seed ^ arcade->sensor_entropy ^ HAL_GetTick();

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
        case ARCADE_PAGE_MPU6000:
            arcade->mpu6000.renderer_initialized = 0U;
            MPU6000Page_Render(&arcade->mpu6000);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            arcade->touch_test.renderer_initialized = 0U;
            TouchTestPage_Render(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH:
            CometCatchRender_Init(&arcade->comet_catch);
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
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTowerRender_Init(&arcade->balance_tower);
            break;
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlightRender_Init(&arcade->shake_flight);
            break;
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuelRender_Init(&arcade->biplane_duel);
            break;
#endif
        case ARCADE_PAGE_LIFE_GAME:
            LifeGameRender_Init(&arcade->life_game);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColonyRender_Init(&arcade->micro_colony);
            break;
        case ARCADE_PAGE_TETRIS:
            TetrisRender_Init(&arcade->tetris);
            break;
        default:
            break;
    }
}

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed) {
    arcade->seed = (seed != 0U) ? seed : 0xA341316CU;
    arcade->sensor_entropy =
        ArcadeCollection_Mix32(arcade->seed ^ HAL_GetTick());
    arcade->active_page = ARCADE_PAGE_SYSTEM_INFO;

    SystemInfoPage_Init(&arcade->system_info);
    MPU6000Page_Init(&arcade->mpu6000);
    TouchTestPage_Init(&arcade->touch_test);
    CometCatch_Init(&arcade->comet_catch,
                    ArcadeCollection_NextSeed(arcade));
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
#if ARCADE_ENABLE_BALANCE_TOWER
    BalanceTower_Init(&arcade->balance_tower,
                      ArcadeCollection_NextSeed(arcade));
#endif
    ShakeFlight_Init(&arcade->shake_flight,
                     ArcadeCollection_NextSeed(arcade));
#if ARCADE_ENABLE_BIPLANE_DUEL
    BiplaneDuel_Init(&arcade->biplane_duel,
                     ArcadeCollection_NextSeed(arcade));
#endif
    LifeGame_Init(&arcade->life_game,
                  ArcadeCollection_NextSeed(arcade));
    MicroColony_Init(&arcade->micro_colony,
                     ArcadeCollection_NextSeed(arcade));
    Tetris_Init(&arcade->tetris,
                ArcadeCollection_NextSeed(arcade));
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_Update(ArcadeCollection *arcade,
                             const MPU6000_Data_t *mpu_data,
                             HAL_StatusTypeDef mpu_status) {
    ArcadeCollection_MixSensorEntropy(arcade,
                                      mpu_data,
                                      mpu_status);

    switch (arcade->active_page) {
        case ARCADE_PAGE_SYSTEM_INFO:
            break;
        case ARCADE_PAGE_MPU6000:
            MPU6000Page_Update(&arcade->mpu6000,
                               mpu_data,
                               mpu_status);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            TouchTestPage_Update(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            CometCatchEvent event =
                CometCatch_Update(&arcade->comet_catch,
                                  mpu_data->accel_x,
                                  touch_pressed);
            CometCatchRender_Frame(&arcade->comet_catch, event);
            break;
        }
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
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER: {
            BalanceTowerEvent event =
                BalanceTower_Update(&arcade->balance_tower,
                                    mpu_data->gyro_z);
            BalanceTowerRender_Frame(&arcade->balance_tower, event);
            break;
        }
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT: {
            ShakeFlightEvent event =
                ShakeFlight_Update(&arcade->shake_flight,
                                   mpu_data->accel_z);
            ShakeFlightRender_Frame(&arcade->shake_flight, event);
            break;
        }
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL: {
            BiplaneDuelEvent event =
                BiplaneDuel_Update(&arcade->biplane_duel,
                                   mpu_data->accel_x,
                                   mpu_data->accel_y);
            BiplaneDuelRender_Frame(&arcade->biplane_duel, event);
            break;
        }
#endif
        case ARCADE_PAGE_LIFE_GAME: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            LifeGameEvent event =
                LifeGame_Update(&arcade->life_game,
                                mpu_data,
                                mpu_status);
            event |= LifeGame_HandleTouch(&arcade->life_game,
                                          touch_pressed,
                                          touch_x,
                                          touch_y);
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
        case ARCADE_PAGE_TETRIS: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            TetrisEvent event =
                Tetris_HandleTouch(&arcade->tetris,
                                   touch_pressed,
                                   touch_x,
                                   touch_y);

            if ((event & TETRIS_EVENT_RESTART_REQUEST) != 0U) {
                Tetris_Init(&arcade->tetris,
                            ArcadeCollection_NextSeed(arcade));
                TetrisRender_Init(&arcade->tetris);
                break;
            }

            event |= Tetris_Update(&arcade->tetris);
            TetrisRender_Frame(&arcade->tetris, event);
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
        case ARCADE_PAGE_MPU6000:
            MPU6000_Init();
            MPU6000Page_Init(&arcade->mpu6000);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            TouchTestPage_Init(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH:
            CometCatch_Init(&arcade->comet_catch, seed);
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
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTower_Init(&arcade->balance_tower, seed);
            break;
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlight_Init(&arcade->shake_flight, seed);
            break;
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuel_Init(&arcade->biplane_duel, seed);
            break;
#endif
        case ARCADE_PAGE_LIFE_GAME:
            LifeGame_Init(&arcade->life_game, seed);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColony_Init(&arcade->micro_colony, seed);
            break;
        case ARCADE_PAGE_TETRIS:
            Tetris_Init(&arcade->tetris, seed);
            break;
        default:
            break;
    }
    ArcadeCollection_RenderActive(arcade);
}
