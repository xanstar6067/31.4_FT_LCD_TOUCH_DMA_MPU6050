#include "space_dodger_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define DODGER_RENDER_PATCH_SIZE       48
#define DODGER_STAR_CELL_WIDTH         32
#define DODGER_STAR_CELL_HEIGHT        30
#define DODGER_STAR_COLUMNS            10U
#define DODGER_STAR_ROWS               7U

#define DODGER_BACKGROUND_COLOR        DISPLAY_COLOR565(2, 5, 15)
#define DODGER_STAR_DIM_COLOR          DISPLAY_COLOR565(75, 100, 145)
#define DODGER_SHIP_COLOR              DISPLAY_COLOR565(80, 210, 245)
#define DODGER_SHIP_EDGE_COLOR         DISPLAY_WHITE
#define DODGER_COCKPIT_COLOR           DISPLAY_CYAN
#define DODGER_BULLET_COLOR            DISPLAY_YELLOW
#define DODGER_ASTEROID_COLOR          DISPLAY_COLOR565(145, 110, 85)
#define DODGER_ASTEROID_DAMAGED_COLOR  DISPLAY_COLOR565(190, 90, 55)
#define DODGER_CRATER_COLOR            DISPLAY_COLOR565(65, 48, 45)

#if (DODGER_RENDER_PATCH_SIZE * DODGER_RENDER_PATCH_SIZE * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for space dodger patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} DodgerRenderRect;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t radius;
    uint8_t active;
} DodgerAsteroidSnapshot;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
} DodgerBulletSnapshot;

#define dodger_patch_buffer render_scratch_buffer
static DodgerAsteroidSnapshot previous_asteroids[
    SPACE_DODGER_MAX_ASTEROIDS];
static DodgerBulletSnapshot previous_bullets[
    SPACE_DODGER_MAX_BULLETS];
static int16_t previous_ship_x;
static int16_t previous_ship_y;
static uint8_t previous_ship_visible;
static uint8_t dodger_renderer_initialized;

static int16_t DodgerRender_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static uint32_t DodgerRender_Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static void DodgerRender_StarPosition(
    const SpaceDodgerState *game,
    uint8_t column,
    uint8_t row,
    int16_t *x,
    int16_t *y,
    uint8_t *bright) {
    uint32_t hash = DodgerRender_Hash(
        game->background_seed +
        ((uint32_t)row * 0x9E3779B9U) +
        ((uint32_t)column * 0x85EBCA6BU));

    *x = (int16_t)(
        (column * DODGER_STAR_CELL_WIDTH) +
        3U + (hash % 26U));
    *y = (int16_t)(
        SPACE_DODGER_PLAYFIELD_TOP +
        (row * DODGER_STAR_CELL_HEIGHT) +
        3U + ((hash >> 8) % 24U));
    *bright = (uint8_t)((hash >> 20) & 0x03U);
}

static uint8_t DodgerRender_PointInCircle(
    int16_t x,
    int16_t y,
    int16_t center_x,
    int16_t center_y,
    int16_t radius) {
    int32_t dx = (int32_t)x - center_x;
    int32_t dy = (int32_t)y - center_y;

    return ((dx * dx) + (dy * dy)) <=
           ((int32_t)radius * radius);
}

static uint8_t DodgerRender_ShipVisible(
    const SpaceDodgerState *game) {
    if (game->phase != SPACE_DODGER_PHASE_PLAYING) {
        return 0U;
    }
    if ((game->invulnerable_ticks != 0U) &&
        (((game->ticks / 3U) & 1U) != 0U)) {
        return 0U;
    }
    return 1U;
}

static uint8_t DodgerRender_PointInShip(
    int16_t x,
    int16_t y,
    int16_t ship_x,
    int16_t ship_y,
    uint16_t *color) {
    int16_t dx = x - ship_x;
    int16_t dy = y - ship_y;
    int16_t absolute_x = (dx < 0) ? -dx : dx;
    int16_t wing_width;

    if ((dy < -9) || (dy > 9)) {
        return 0U;
    }

    wing_width = (dy >= -2) ? (dy + 3) : 3;
    if ((absolute_x > 3) &&
        ((dy < -2) || (absolute_x > wing_width))) {
        return 0U;
    }

    if (((dy == -9) && (absolute_x == 0)) ||
        ((dy >= 6) && (absolute_x == wing_width)) ||
        (absolute_x == 3)) {
        *color = DODGER_SHIP_EDGE_COLOR;
    } else if (((dx * dx) + ((dy + 2) * (dy + 2))) <= 5) {
        *color = DODGER_COCKPIT_COLOR;
    } else {
        *color = DODGER_SHIP_COLOR;
    }
    return 1U;
}

static void DodgerRender_ClipRect(DodgerRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < SPACE_DODGER_PLAYFIELD_TOP) {
        rect->y0 = SPACE_DODGER_PLAYFIELD_TOP;
    }
    if (rect->x1 >= SPACE_DODGER_SCREEN_WIDTH) {
        rect->x1 = SPACE_DODGER_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= SPACE_DODGER_SCREEN_HEIGHT) {
        rect->y1 = SPACE_DODGER_SCREEN_HEIGHT - 1;
    }
}

static void DodgerRender_IncludeRect(DodgerRenderRect *destination,
                                     DodgerRenderRect source) {
    if (source.x0 < destination->x0) {
        destination->x0 = source.x0;
    }
    if (source.y0 < destination->y0) {
        destination->y0 = source.y0;
    }
    if (source.x1 > destination->x1) {
        destination->x1 = source.x1;
    }
    if (source.y1 > destination->y1) {
        destination->y1 = source.y1;
    }
}

static uint8_t DodgerRender_RectsOverlap(
    DodgerRenderRect first,
    DodgerRenderRect second) {
    return (first.x0 <= second.x1) &&
           (first.x1 >= second.x0) &&
           (first.y0 <= second.y1) &&
           (first.y1 >= second.y0);
}

static void DodgerRender_PutPixel(DodgerRenderRect rect,
                                  uint16_t width,
                                  uint16_t height,
                                  int16_t x,
                                  int16_t y,
                                  uint16_t color) {
    uint32_t offset;

    if ((x < rect.x0) || (x > rect.x1) ||
        (y < rect.y0) || (y > rect.y1)) {
        return;
    }

    offset =
        (((uint32_t)(y - rect.y0) * width) +
         (uint32_t)(x - rect.x0)) * 2U;
    if (offset >= ((uint32_t)width * height * 2U)) {
        return;
    }
    dodger_patch_buffer[offset] = (uint8_t)(color >> 8);
    dodger_patch_buffer[offset + 1U] = (uint8_t)(color & 0xFFU);
}

static void DodgerRender_FillPatch(uint16_t width,
                                   uint16_t height,
                                   uint16_t color) {
    uint32_t pixel_count = (uint32_t)width * height;
    uint8_t high = (uint8_t)(color >> 8);
    uint8_t low = (uint8_t)(color & 0xFFU);

    for (uint32_t i = 0U; i < pixel_count; i++) {
        dodger_patch_buffer[i * 2U] = high;
        dodger_patch_buffer[(i * 2U) + 1U] = low;
    }
}

static void DodgerRender_DrawCircleToPatch(
    DodgerRenderRect rect,
    uint16_t width,
    uint16_t height,
    int16_t center_x,
    int16_t center_y,
    int16_t radius,
    uint16_t color) {
    int16_t x0 = center_x - radius;
    int16_t x1 = center_x + radius;
    int16_t y0 = center_y - radius;
    int16_t y1 = center_y + radius;

    if (x0 < rect.x0) {
        x0 = rect.x0;
    }
    if (x1 > rect.x1) {
        x1 = rect.x1;
    }
    if (y0 < rect.y0) {
        y0 = rect.y0;
    }
    if (y1 > rect.y1) {
        y1 = rect.y1;
    }

    for (int16_t y = y0; y <= y1; y++) {
        for (int16_t x = x0; x <= x1; x++) {
            if (DodgerRender_PointInCircle(
                    x, y, center_x, center_y, radius) != 0U) {
                DodgerRender_PutPixel(
                    rect, width, height, x, y, color);
            }
        }
    }
}

static void DodgerRender_DrawStarsToPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t row = 0U; row < DODGER_STAR_ROWS; row++) {
        for (uint8_t column = 0U;
             column < DODGER_STAR_COLUMNS;
             column++) {
            int16_t x;
            int16_t y;
            uint8_t bright;

            DodgerRender_StarPosition(game, column, row,
                                      &x, &y, &bright);
            if ((x < (rect.x0 - 1)) || (x > (rect.x1 + 1)) ||
                (y < (rect.y0 - 1)) || (y > (rect.y1 + 1))) {
                continue;
            }
            if (bright == 3U) {
                DodgerRender_PutPixel(
                    rect, width, height, x - 1, y,
                    DODGER_STAR_DIM_COLOR);
                DodgerRender_PutPixel(
                    rect, width, height, x + 1, y,
                    DODGER_STAR_DIM_COLOR);
                DodgerRender_PutPixel(
                    rect, width, height, x, y - 1,
                    DODGER_STAR_DIM_COLOR);
                DodgerRender_PutPixel(
                    rect, width, height, x, y + 1,
                    DODGER_STAR_DIM_COLOR);
            }
            DodgerRender_PutPixel(
                rect, width, height, x, y,
                (bright >= 2U) ?
                DISPLAY_WHITE : DODGER_STAR_DIM_COLOR);
        }
    }
}

static void DodgerRender_DrawAsteroidsToPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        const SpaceDodgerAsteroid *asteroid = &game->asteroids[i];
        int16_t asteroid_x;
        int16_t asteroid_y;
        int16_t crater_x;
        int16_t crater_y;
        int16_t crater_radius;
        DodgerRenderRect bounds;

        if (asteroid->active == 0U) {
            continue;
        }
        asteroid_x = DodgerRender_Round(asteroid->x);
        asteroid_y = DodgerRender_Round(asteroid->y);
        bounds.x0 = asteroid_x - asteroid->radius;
        bounds.y0 = asteroid_y - asteroid->radius;
        bounds.x1 = asteroid_x + asteroid->radius;
        bounds.y1 = asteroid_y + asteroid->radius;
        if (DodgerRender_RectsOverlap(rect, bounds) == 0U) {
            continue;
        }

        DodgerRender_DrawCircleToPatch(
            rect, width, height,
            asteroid_x, asteroid_y, asteroid->radius,
            (asteroid->health > 1U) ?
            DODGER_ASTEROID_COLOR :
            DODGER_ASTEROID_DAMAGED_COLOR);

        crater_x = asteroid_x +
            (((asteroid->style & 1U) != 0U) ?
             (asteroid->radius / 3) :
             -(asteroid->radius / 3));
        crater_y = asteroid_y +
            (((asteroid->style & 2U) != 0U) ?
             (asteroid->radius / 4) :
             -(asteroid->radius / 4));
        crater_radius = asteroid->radius / 4;
        if (crater_radius < 2) {
            crater_radius = 2;
        }
        DodgerRender_DrawCircleToPatch(
            rect, width, height,
            crater_x, crater_y, crater_radius,
            DODGER_CRATER_COLOR);
        DodgerRender_DrawCircleToPatch(
            rect, width, height,
            asteroid_x - (asteroid->radius / 3),
            asteroid_y - (asteroid->radius / 3),
            1, DISPLAY_WHITE);
    }
}

static void DodgerRender_DrawBulletsToPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        const SpaceDodgerBullet *bullet = &game->bullets[i];
        int16_t bullet_x;
        int16_t bullet_y;
        DodgerRenderRect bounds;

        if (bullet->active == 0U) {
            continue;
        }
        bullet_x = DodgerRender_Round(bullet->x);
        bullet_y = DodgerRender_Round(bullet->y);
        bounds.x0 = bullet_x - 1;
        bounds.y0 = bullet_y - 4;
        bounds.x1 = bullet_x + 1;
        bounds.y1 = bullet_y + 3;
        if (DodgerRender_RectsOverlap(rect, bounds) == 0U) {
            continue;
        }

        for (int16_t y = bounds.y0; y <= bounds.y1; y++) {
            for (int16_t x = bounds.x0; x <= bounds.x1; x++) {
                DodgerRender_PutPixel(
                    rect, width, height, x, y,
                    ((x == bullet_x) && (y <= bullet_y)) ?
                    DISPLAY_WHITE : DODGER_BULLET_COLOR);
            }
        }
    }
}

static void DodgerRender_DrawShipToPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect,
    uint16_t width,
    uint16_t height) {
    int16_t ship_x;
    int16_t ship_y;
    DodgerRenderRect bounds;

    if (DodgerRender_ShipVisible(game) == 0U) {
        return;
    }

    ship_x = DodgerRender_Round(game->ship.x);
    ship_y = DodgerRender_Round(game->ship.y);
    bounds.x0 = ship_x - 13;
    bounds.y0 = ship_y - 10;
    bounds.x1 = ship_x + 13;
    bounds.y1 = ship_y + 10;
    if (DodgerRender_RectsOverlap(rect, bounds) == 0U) {
        return;
    }

    for (int16_t y = bounds.y0; y <= bounds.y1; y++) {
        for (int16_t x = bounds.x0; x <= bounds.x1; x++) {
            uint16_t color;

            if (DodgerRender_PointInShip(
                    x, y, ship_x, ship_y, &color) != 0U) {
                DodgerRender_PutPixel(
                    rect, width, height, x, y, color);
            }
        }
    }
}

static uint8_t DodgerRender_DrawPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect) {
    uint16_t width;
    uint16_t height;

    DodgerRender_ClipRect(&rect);
    if ((rect.x1 < rect.x0) || (rect.y1 < rect.y0)) {
        return 1U;
    }
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);
    if ((width > DODGER_RENDER_PATCH_SIZE) ||
        (height > DODGER_RENDER_PATCH_SIZE)) {
        return 0U;
    }

    DodgerRender_FillPatch(
        width, height, DODGER_BACKGROUND_COLOR);
    DodgerRender_DrawStarsToPatch(
        game, rect, width, height);
    DodgerRender_DrawAsteroidsToPatch(
        game, rect, width, height);
    DodgerRender_DrawBulletsToPatch(
        game, rect, width, height);
    DodgerRender_DrawShipToPatch(
        game, rect, width, height);

    DISPLAY_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        dodger_patch_buffer);
    return 1U;
}

static void DodgerRender_DrawStars(const SpaceDodgerState *game) {
    for (uint8_t row = 0U; row < DODGER_STAR_ROWS; row++) {
        for (uint8_t column = 0U;
             column < DODGER_STAR_COLUMNS;
             column++) {
            int16_t x;
            int16_t y;
            uint8_t bright;

            DodgerRender_StarPosition(game, column, row,
                                      &x, &y, &bright);
            DISPLAY_FillRectangle_DMA(
                (uint16_t)x, (uint16_t)y, 1, 1,
                (bright >= 2U) ?
                DISPLAY_WHITE : DODGER_STAR_DIM_COLOR);
            if (bright == 3U) {
                DISPLAY_FillRectangle_DMA(
                    (uint16_t)(x - 1), (uint16_t)y,
                    3, 1, DODGER_STAR_DIM_COLOR);
                DISPLAY_FillRectangle_DMA(
                    (uint16_t)x, (uint16_t)(y - 1),
                    1, 3, DODGER_STAR_DIM_COLOR);
                DISPLAY_FillRectangle_DMA(
                    (uint16_t)x, (uint16_t)y,
                    1, 1, DISPLAY_WHITE);
            }
        }
    }
}

static void DodgerRender_DrawHud(const SpaceDodgerState *game) {
    char status[24];

    DISPLAY_FillRectangle_DMA(
        0, 0, SPACE_DODGER_SCREEN_WIDTH,
        SPACE_DODGER_PLAYFIELD_TOP, DISPLAY_BLACK);
    DISPLAY_FillRectangle_DMA(
        0, SPACE_DODGER_PLAYFIELD_TOP - 1,
        SPACE_DODGER_SCREEN_WIDTH, 1, DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(
        4, 4, "SPACE DODGER", Font_11x18,
        DISPLAY_WHITE, DISPLAY_BLACK);

    snprintf(status, sizeof(status), "L%u %05lu",
             (unsigned int)game->level,
             (unsigned long)game->score);
    DISPLAY_WriteString_DMA(
        154, 9, status, Font_7x10,
        DISPLAY_CYAN, DISPLAY_BLACK);
    snprintf(status, sizeof(status), "SH %u",
             (unsigned int)game->shields);
    DISPLAY_WriteString_DMA(
        280, 9, status, Font_7x10,
        (game->shields > 1U) ?
        DISPLAY_GREEN : DISPLAY_ORANGE,
        DISPLAY_BLACK);
}

static void DodgerRender_SaveSnapshots(
    const SpaceDodgerState *game) {
    previous_ship_x = DodgerRender_Round(game->ship.x);
    previous_ship_y = DodgerRender_Round(game->ship.y);
    previous_ship_visible = DodgerRender_ShipVisible(game);

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        previous_asteroids[i].x =
            DodgerRender_Round(game->asteroids[i].x);
        previous_asteroids[i].y =
            DodgerRender_Round(game->asteroids[i].y);
        previous_asteroids[i].radius = game->asteroids[i].radius;
        previous_asteroids[i].active = game->asteroids[i].active;
    }
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        previous_bullets[i].x =
            DodgerRender_Round(game->bullets[i].x);
        previous_bullets[i].y =
            DodgerRender_Round(game->bullets[i].y);
        previous_bullets[i].active = game->bullets[i].active;
    }
}

static void DodgerRender_DrawObjects(
    const SpaceDodgerState *game) {
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        const SpaceDodgerAsteroid *asteroid = &game->asteroids[i];

        if (asteroid->active != 0U) {
            int16_t x = DodgerRender_Round(asteroid->x);
            int16_t y = DodgerRender_Round(asteroid->y);
            DodgerRenderRect rect = {
                x - asteroid->radius - 1,
                y - asteroid->radius - 1,
                x + asteroid->radius + 1,
                y + asteroid->radius + 1
            };

            DodgerRender_DrawPatch(game, rect);
        }
    }
    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        const SpaceDodgerBullet *bullet = &game->bullets[i];

        if (bullet->active != 0U) {
            int16_t x = DodgerRender_Round(bullet->x);
            int16_t y = DodgerRender_Round(bullet->y);
            DodgerRenderRect rect = {
                x - 2, y - 5, x + 2, y + 4
            };

            DodgerRender_DrawPatch(game, rect);
        }
    }

    if (DodgerRender_ShipVisible(game) != 0U) {
        int16_t x = DodgerRender_Round(game->ship.x);
        int16_t y = DodgerRender_Round(game->ship.y);
        DodgerRenderRect rect = {
            x - 13, y - 10, x + 13, y + 10
        };

        DodgerRender_DrawPatch(game, rect);
    }
}

static void DodgerRender_DrawWholeField(
    const SpaceDodgerState *game) {
    DISPLAY_FillScreen_DMA(DODGER_BACKGROUND_COLOR);
    DodgerRender_DrawStars(game);
    DodgerRender_DrawHud(game);
    DodgerRender_DrawObjects(game);

    if (game->phase == SPACE_DODGER_PHASE_RESTART_PAUSE) {
        DISPLAY_WriteString_DMA(
            88, 92, "SHIP LOST", Font_16x26,
            DISPLAY_ORANGE, DODGER_BACKGROUND_COLOR);
        DISPLAY_WriteString_DMA(
            111, 126, "AUTO RESTART", Font_7x10,
            DISPLAY_CYAN, DODGER_BACKGROUND_COLOR);
    }

    DodgerRender_SaveSnapshots(game);
    dodger_renderer_initialized = 1U;
}

void SpaceDodgerRender_Init(const SpaceDodgerState *game) {
    dodger_renderer_initialized = 0U;
    DodgerRender_DrawWholeField(game);
}

void SpaceDodgerRender_Frame(const SpaceDodgerState *game,
                             SpaceDodgerEvent event) {
    int16_t ship_x;
    int16_t ship_y;
    uint8_t ship_visible;

    if ((dodger_renderer_initialized == 0U) ||
        ((event & (SPACE_DODGER_EVENT_GAME_STARTED |
                   SPACE_DODGER_EVENT_RESTART_PAUSE)) != 0U)) {
        DodgerRender_DrawWholeField(game);
        return;
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        const SpaceDodgerAsteroid *asteroid = &game->asteroids[i];
        const DodgerAsteroidSnapshot *previous =
            &previous_asteroids[i];
        int16_t current_x = DodgerRender_Round(asteroid->x);
        int16_t current_y = DodgerRender_Round(asteroid->y);
        uint8_t current_radius = asteroid->radius;
        DodgerRenderRect dirty;

        if ((asteroid->active == 0U) &&
            (previous->active == 0U)) {
            continue;
        }
        if ((asteroid->active == previous->active) &&
            (current_x == previous->x) &&
            (current_y == previous->y) &&
            (current_radius == previous->radius)) {
            continue;
        }

        if (previous->active != 0U) {
            dirty.x0 = previous->x - previous->radius - 1;
            dirty.y0 = previous->y - previous->radius - 1;
            dirty.x1 = previous->x + previous->radius + 1;
            dirty.y1 = previous->y + previous->radius + 1;
        } else {
            dirty.x0 = current_x - current_radius - 1;
            dirty.y0 = current_y - current_radius - 1;
            dirty.x1 = current_x + current_radius + 1;
            dirty.y1 = current_y + current_radius + 1;
        }
        if (asteroid->active != 0U) {
            DodgerRenderRect current = {
                current_x - current_radius - 1,
                current_y - current_radius - 1,
                current_x + current_radius + 1,
                current_y + current_radius + 1
            };

            DodgerRender_IncludeRect(&dirty, current);
        }
        if (DodgerRender_DrawPatch(game, dirty) == 0U) {
            DodgerRender_DrawWholeField(game);
            return;
        }
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        const SpaceDodgerBullet *bullet = &game->bullets[i];
        const DodgerBulletSnapshot *previous = &previous_bullets[i];
        int16_t current_x = DodgerRender_Round(bullet->x);
        int16_t current_y = DodgerRender_Round(bullet->y);
        DodgerRenderRect dirty;

        if ((bullet->active == 0U) &&
            (previous->active == 0U)) {
            continue;
        }
        if ((bullet->active == previous->active) &&
            (current_x == previous->x) &&
            (current_y == previous->y)) {
            continue;
        }

        if (previous->active != 0U) {
            dirty.x0 = previous->x - 2;
            dirty.y0 = previous->y - 5;
            dirty.x1 = previous->x + 2;
            dirty.y1 = previous->y + 4;
        } else {
            dirty.x0 = current_x - 2;
            dirty.y0 = current_y - 5;
            dirty.x1 = current_x + 2;
            dirty.y1 = current_y + 4;
        }
        if (bullet->active != 0U) {
            DodgerRenderRect current = {
                current_x - 2, current_y - 5,
                current_x + 2, current_y + 4
            };

            DodgerRender_IncludeRect(&dirty, current);
        }
        if (DodgerRender_DrawPatch(game, dirty) == 0U) {
            DodgerRender_DrawWholeField(game);
            return;
        }
    }

    ship_x = DodgerRender_Round(game->ship.x);
    ship_y = DodgerRender_Round(game->ship.y);
    ship_visible = DodgerRender_ShipVisible(game);
    if ((ship_x != previous_ship_x) ||
        (ship_y != previous_ship_y) ||
        (ship_visible != previous_ship_visible)) {
        DodgerRenderRect dirty = {
            previous_ship_x - 13,
            previous_ship_y - 10,
            previous_ship_x + 13,
            previous_ship_y + 10
        };
        DodgerRenderRect current = {
            ship_x - 13, ship_y - 10,
            ship_x + 13, ship_y + 10
        };

        DodgerRender_IncludeRect(&dirty, current);
        if (DodgerRender_DrawPatch(game, dirty) == 0U) {
            DodgerRender_DrawWholeField(game);
            return;
        }
    }

    if ((event & SPACE_DODGER_EVENT_HUD_CHANGED) != 0U) {
        DodgerRender_DrawHud(game);
    }
    DodgerRender_SaveSnapshots(game);
}
