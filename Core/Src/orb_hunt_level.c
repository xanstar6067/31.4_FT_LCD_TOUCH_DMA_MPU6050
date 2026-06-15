#include "orb_hunt_level.h"

#define ORB_HUNT_THEME_COUNT       6U
#define ORB_HUNT_TARGET_EDGE_MARGIN      18
#define ORB_HUNT_TARGET_START_DISTANCE   62
#define ORB_HUNT_TARGET_MAX_ATTEMPTS     96U

static uint32_t OrbHuntLevel_Random(uint32_t *state) {
    uint32_t value = *state;

    if (value == 0U) {
        value = 0x6D2B79F5U;
    }

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int32_t OrbHuntLevel_DistanceSquared(int16_t x1, int16_t y1,
                                            int16_t x2, int16_t y2) {
    int32_t dx = (int32_t)x1 - x2;
    int32_t dy = (int32_t)y1 - y2;
    return (dx * dx) + (dy * dy);
}

static uint8_t OrbHuntLevel_TargetCount(uint16_t level_number) {
    uint16_t count = 3U + ((level_number - 1U) / 2U);

    if (count > ORB_HUNT_MAX_TARGETS) {
        count = ORB_HUNT_MAX_TARGETS;
    }
    return (uint8_t)count;
}

static uint8_t OrbHuntLevel_PositionIsValid(const OrbHuntLevel *level,
                                            uint8_t placed_count,
                                            int16_t x,
                                            int16_t y,
                                            int16_t start_x,
                                            int16_t start_y) {
    const int32_t start_distance =
        ORB_HUNT_TARGET_START_DISTANCE * ORB_HUNT_TARGET_START_DISTANCE;
    const int32_t target_distance =
        ORB_HUNT_TARGET_MIN_GAP * ORB_HUNT_TARGET_MIN_GAP;

    if (OrbHuntLevel_DistanceSquared(x, y, start_x, start_y) <
        start_distance) {
        return 0U;
    }

    for (uint8_t i = 0U; i < placed_count; i++) {
        if (OrbHuntLevel_DistanceSquared(x, y,
                                         level->targets[i].x,
                                         level->targets[i].y) <
            target_distance) {
            return 0U;
        }
    }
    return 1U;
}

static void OrbHuntLevel_FallbackPosition(uint8_t index,
                                          uint16_t screen_width,
                                          uint16_t screen_height,
                                          uint16_t playfield_top,
                                          int16_t *x,
                                          int16_t *y) {
    const uint16_t columns = 4U;
    uint16_t usable_height = screen_height - playfield_top;
    uint16_t column = index % columns;
    uint16_t row = index / columns;

    *x = (int16_t)(((column * 2U + 1U) * screen_width) / (columns * 2U));
    *y = (int16_t)(playfield_top +
                   (((row * 2U + 1U) * usable_height) / 4U));
}

void OrbHuntLevel_Generate(OrbHuntLevel *level,
                           uint16_t level_number,
                           uint32_t *rng_state,
                           uint16_t screen_width,
                           uint16_t screen_height,
                           uint16_t playfield_top,
                           int16_t start_x,
                           int16_t start_y) {
    int16_t min_x = ORB_HUNT_TARGET_EDGE_MARGIN;
    int16_t max_x =
        (int16_t)screen_width - ORB_HUNT_TARGET_EDGE_MARGIN - 1;
    int16_t min_y =
        (int16_t)playfield_top + ORB_HUNT_TARGET_EDGE_MARGIN;
    int16_t max_y =
        (int16_t)screen_height - ORB_HUNT_TARGET_EDGE_MARGIN - 1;
    uint16_t range_x = (uint16_t)(max_x - min_x + 1);
    uint16_t range_y = (uint16_t)(max_y - min_y + 1);

    level->number = level_number;
    level->target_count = OrbHuntLevel_TargetCount(level_number);
    level->collected_count = 0U;
    level->theme = (uint8_t)(OrbHuntLevel_Random(rng_state) %
                             ORB_HUNT_THEME_COUNT);

    for (uint8_t i = 0U; i < ORB_HUNT_MAX_TARGETS; i++) {
        level->targets[i].active = 0U;
    }

    for (uint8_t i = 0U; i < level->target_count; i++) {
        int16_t x = 0;
        int16_t y = 0;
        uint8_t found = 0U;

        for (uint8_t attempt = 0U;
             attempt < ORB_HUNT_TARGET_MAX_ATTEMPTS;
             attempt++) {
            x = (int16_t)(min_x +
                          (OrbHuntLevel_Random(rng_state) % range_x));
            y = (int16_t)(min_y +
                          (OrbHuntLevel_Random(rng_state) % range_y));

            if (OrbHuntLevel_PositionIsValid(level, i, x, y,
                                             start_x, start_y) != 0U) {
                found = 1U;
                break;
            }
        }

        if (found == 0U) {
            OrbHuntLevel_FallbackPosition(i, screen_width, screen_height,
                                          playfield_top, &x, &y);
        }

        level->targets[i].x = x;
        level->targets[i].y = y;
        level->targets[i].active = 1U;
    }
}
