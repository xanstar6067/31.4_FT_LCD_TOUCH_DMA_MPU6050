#include "space_dodger_render.h"

#include <stdio.h>
#include "ili9341.h"

#define DODGER_RENDER_PATCH_SIZE       48
#define DODGER_STAR_CELL_WIDTH         32
#define DODGER_STAR_CELL_HEIGHT        30
#define DODGER_STAR_COLUMNS            10U
#define DODGER_STAR_ROWS               7U

#define DODGER_BACKGROUND_COLOR        ILI9341_COLOR565(2, 5, 15)
#define DODGER_STAR_DIM_COLOR          ILI9341_COLOR565(75, 100, 145)
#define DODGER_SHIP_COLOR              ILI9341_COLOR565(80, 210, 245)
#define DODGER_SHIP_EDGE_COLOR         ILI9341_WHITE
#define DODGER_COCKPIT_COLOR           ILI9341_CYAN
#define DODGER_BULLET_COLOR            ILI9341_YELLOW
#define DODGER_ASTEROID_COLOR          ILI9341_COLOR565(145, 110, 85)
#define DODGER_ASTEROID_DAMAGED_COLOR  ILI9341_COLOR565(190, 90, 55)
#define DODGER_CRATER_COLOR            ILI9341_COLOR565(65, 48, 45)

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

static uint8_t dodger_patch_buffer[
    DODGER_RENDER_PATCH_SIZE * DODGER_RENDER_PATCH_SIZE * 2U];
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

static uint16_t DodgerRender_StaticPixel(
    const SpaceDodgerState *game,
    int16_t x,
    int16_t y) {
    uint8_t column;
    uint8_t row;
    uint8_t bright;
    int16_t star_x;
    int16_t star_y;
    int16_t dx;
    int16_t dy;

    if ((y < SPACE_DODGER_PLAYFIELD_TOP) ||
        (x < 0) ||
        (x >= SPACE_DODGER_SCREEN_WIDTH)) {
        return DODGER_BACKGROUND_COLOR;
    }

    column = (uint8_t)(x / DODGER_STAR_CELL_WIDTH);
    row = (uint8_t)(
        (y - SPACE_DODGER_PLAYFIELD_TOP) /
        DODGER_STAR_CELL_HEIGHT);
    if ((column >= DODGER_STAR_COLUMNS) ||
        (row >= DODGER_STAR_ROWS)) {
        return DODGER_BACKGROUND_COLOR;
    }

    DodgerRender_StarPosition(game, column, row,
                              &star_x, &star_y, &bright);
    dx = x - star_x;
    dy = y - star_y;
    if ((dx == 0) && (dy == 0)) {
        return (bright >= 2U) ?
               ILI9341_WHITE : DODGER_STAR_DIM_COLOR;
    }
    if ((bright == 3U) &&
        (((dx == 0) && ((dy == -1) || (dy == 1))) ||
         ((dy == 0) && ((dx == -1) || (dx == 1))))) {
        return DODGER_STAR_DIM_COLOR;
    }
    return DODGER_BACKGROUND_COLOR;
}

static uint16_t DodgerRender_ComposedPixel(
    const SpaceDodgerState *game,
    int16_t ship_x,
    int16_t ship_y,
    uint8_t ship_visible,
    int16_t x,
    int16_t y) {
    uint16_t color;

    if ((ship_visible != 0U) &&
        (DodgerRender_PointInShip(
            x, y, ship_x, ship_y, &color) != 0U)) {
        return color;
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_BULLETS; i++) {
        const SpaceDodgerBullet *bullet = &game->bullets[i];
        int16_t bullet_x;
        int16_t bullet_y;

        if (bullet->active == 0U) {
            continue;
        }
        bullet_x = DodgerRender_Round(bullet->x);
        bullet_y = DodgerRender_Round(bullet->y);
        if ((x >= (bullet_x - 1)) &&
            (x <= (bullet_x + 1)) &&
            (y >= (bullet_y - 4)) &&
            (y <= (bullet_y + 3))) {
            return ((x == bullet_x) && (y <= bullet_y)) ?
                   ILI9341_WHITE : DODGER_BULLET_COLOR;
        }
    }

    for (uint8_t i = 0U; i < SPACE_DODGER_MAX_ASTEROIDS; i++) {
        const SpaceDodgerAsteroid *asteroid = &game->asteroids[i];
        int16_t asteroid_x;
        int16_t asteroid_y;
        int16_t crater_x;
        int16_t crater_y;
        int16_t crater_radius;

        if (asteroid->active == 0U) {
            continue;
        }
        asteroid_x = DodgerRender_Round(asteroid->x);
        asteroid_y = DodgerRender_Round(asteroid->y);
        if (DodgerRender_PointInCircle(
                x, y, asteroid_x, asteroid_y,
                asteroid->radius) == 0U) {
            continue;
        }

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
        if (DodgerRender_PointInCircle(
                x, y, crater_x, crater_y,
                crater_radius) != 0U) {
            return DODGER_CRATER_COLOR;
        }
        if (DodgerRender_PointInCircle(
                x, y,
                asteroid_x - (asteroid->radius / 3),
                asteroid_y - (asteroid->radius / 3),
                1) != 0U) {
            return ILI9341_WHITE;
        }
        return (asteroid->health > 1U) ?
               DODGER_ASTEROID_COLOR :
               DODGER_ASTEROID_DAMAGED_COLOR;
    }

    return DodgerRender_StaticPixel(game, x, y);
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

static uint8_t DodgerRender_DrawPatch(
    const SpaceDodgerState *game,
    DodgerRenderRect rect) {
    int16_t ship_x = DodgerRender_Round(game->ship.x);
    int16_t ship_y = DodgerRender_Round(game->ship.y);
    uint8_t ship_visible = DodgerRender_ShipVisible(game);
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

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

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                DodgerRender_ComposedPixel(
                    game, ship_x, ship_y,
                    ship_visible, x, y);

            dodger_patch_buffer[offset++] = (uint8_t)(color >> 8);
            dodger_patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    ILI9341_DrawImage_DMA_1D(
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
            ILI9341_FillRectangle_DMA(
                (uint16_t)x, (uint16_t)y, 1, 1,
                (bright >= 2U) ?
                ILI9341_WHITE : DODGER_STAR_DIM_COLOR);
            if (bright == 3U) {
                ILI9341_FillRectangle_DMA(
                    (uint16_t)(x - 1), (uint16_t)y,
                    3, 1, DODGER_STAR_DIM_COLOR);
                ILI9341_FillRectangle_DMA(
                    (uint16_t)x, (uint16_t)(y - 1),
                    1, 3, DODGER_STAR_DIM_COLOR);
                ILI9341_FillRectangle_DMA(
                    (uint16_t)x, (uint16_t)y,
                    1, 1, ILI9341_WHITE);
            }
        }
    }
}

static void DodgerRender_DrawHud(const SpaceDodgerState *game) {
    char status[24];

    ILI9341_FillRectangle_DMA(
        0, 0, SPACE_DODGER_SCREEN_WIDTH,
        SPACE_DODGER_PLAYFIELD_TOP, ILI9341_BLACK);
    ILI9341_FillRectangle_DMA(
        0, SPACE_DODGER_PLAYFIELD_TOP - 1,
        SPACE_DODGER_SCREEN_WIDTH, 1, ILI9341_GRAY);
    ILI9341_WriteString_DMA(
        4, 4, "SPACE DODGER", Font_11x18,
        ILI9341_WHITE, ILI9341_BLACK);

    snprintf(status, sizeof(status), "L%u %05lu",
             (unsigned int)game->level,
             (unsigned long)game->score);
    ILI9341_WriteString_DMA(
        154, 9, status, Font_7x10,
        ILI9341_CYAN, ILI9341_BLACK);
    snprintf(status, sizeof(status), "SH %u",
             (unsigned int)game->shields);
    ILI9341_WriteString_DMA(
        280, 9, status, Font_7x10,
        (game->shields > 1U) ?
        ILI9341_GREEN : ILI9341_ORANGE,
        ILI9341_BLACK);
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
    ILI9341_FillScreen_DMA(DODGER_BACKGROUND_COLOR);
    DodgerRender_DrawStars(game);
    DodgerRender_DrawHud(game);
    DodgerRender_DrawObjects(game);

    if (game->phase == SPACE_DODGER_PHASE_RESTART_PAUSE) {
        ILI9341_WriteString_DMA(
            88, 92, "SHIP LOST", Font_16x26,
            ILI9341_ORANGE, DODGER_BACKGROUND_COLOR);
        ILI9341_WriteString_DMA(
            111, 126, "AUTO RESTART", Font_7x10,
            ILI9341_CYAN, DODGER_BACKGROUND_COLOR);
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
