#include "arcade_collection.h"

#include "gravity_pong_render.h"
#include "orb_hunt_render.h"

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
    if (arcade->active_game == ARCADE_GAME_ORB_HUNT) {
        OrbHuntRender_Init(&arcade->orb_hunt);
    } else {
        GravityPongRender_Init(&arcade->gravity_pong);
    }
}

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed) {
    arcade->seed = (seed != 0U) ? seed : 0xA341316CU;
    arcade->active_game = ARCADE_GAME_ORB_HUNT;

    OrbHunt_Init(&arcade->orb_hunt,
                 ArcadeCollection_NextSeed(arcade));
    GravityPong_Init(&arcade->gravity_pong,
                     ArcadeCollection_NextSeed(arcade));
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_Update(ArcadeCollection *arcade,
                             int16_t accel_x,
                             int16_t accel_y) {
    if (arcade->active_game == ARCADE_GAME_ORB_HUNT) {
        OrbHuntEvent event =
            OrbHunt_Update(&arcade->orb_hunt, accel_x, accel_y);
        OrbHuntRender_Frame(&arcade->orb_hunt, event);
    } else {
        GravityPongEvent event =
            GravityPong_Update(&arcade->gravity_pong, accel_x);
        GravityPongRender_Frame(&arcade->gravity_pong, event);
    }
}

void ArcadeCollection_NextGame(ArcadeCollection *arcade) {
    arcade->active_game =
        (ArcadeGameId)((arcade->active_game + 1U) % ARCADE_GAME_COUNT);
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_RestartGame(ArcadeCollection *arcade) {
    uint32_t seed = ArcadeCollection_NextSeed(arcade);

    if (arcade->active_game == ARCADE_GAME_ORB_HUNT) {
        OrbHunt_Init(&arcade->orb_hunt, seed);
    } else {
        GravityPong_Init(&arcade->gravity_pong, seed);
    }
    ArcadeCollection_RenderActive(arcade);
}
