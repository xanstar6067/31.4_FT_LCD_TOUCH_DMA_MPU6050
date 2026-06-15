#include "arcade_collection.h"

#include "gravity_pong_render.h"
#include "orb_hunt_render.h"
#include "tilt_breaker_render.h"

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
        case ARCADE_PAGE_ORB_HUNT:
            OrbHuntRender_Init(&arcade->orb_hunt);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPongRender_Init(&arcade->gravity_pong);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreakerRender_Init(&arcade->tilt_breaker);
            break;
        default:
            break;
    }
}

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed) {
    arcade->seed = (seed != 0U) ? seed : 0xA341316CU;
    arcade->active_page = ARCADE_PAGE_SYSTEM_INFO;

    SystemInfoPage_Init(&arcade->system_info);
    OrbHunt_Init(&arcade->orb_hunt,
                 ArcadeCollection_NextSeed(arcade));
    GravityPong_Init(&arcade->gravity_pong,
                     ArcadeCollection_NextSeed(arcade));
    TiltBreaker_Init(&arcade->tilt_breaker,
                     ArcadeCollection_NextSeed(arcade));
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_Update(ArcadeCollection *arcade,
                             int16_t accel_x,
                             int16_t accel_y) {
    switch (arcade->active_page) {
        case ARCADE_PAGE_SYSTEM_INFO:
            break;
        case ARCADE_PAGE_ORB_HUNT: {
            OrbHuntEvent event =
                OrbHunt_Update(&arcade->orb_hunt, accel_x, accel_y);
            OrbHuntRender_Frame(&arcade->orb_hunt, event);
            break;
        }
        case ARCADE_PAGE_GRAVITY_PONG: {
            GravityPongEvent event =
                GravityPong_Update(&arcade->gravity_pong, accel_x);
            GravityPongRender_Frame(&arcade->gravity_pong, event);
            break;
        }
        case ARCADE_PAGE_TILT_BREAKER: {
            TiltBreakerEvent event =
                TiltBreaker_Update(&arcade->tilt_breaker, accel_x);
            TiltBreakerRender_Frame(&arcade->tilt_breaker, event);
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
        case ARCADE_PAGE_ORB_HUNT:
            OrbHunt_Init(&arcade->orb_hunt, seed);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPong_Init(&arcade->gravity_pong, seed);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreaker_Init(&arcade->tilt_breaker, seed);
            break;
        default:
            break;
    }
    ArcadeCollection_RenderActive(arcade);
}
