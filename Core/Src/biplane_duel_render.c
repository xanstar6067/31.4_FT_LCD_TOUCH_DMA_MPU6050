#include "biplane_duel_render.h"

#include <stdio.h>
#include "ili9341.h"
#include "render_scratch.h"

#define BIPLANE_RENDER_PATCH_WIDTH       96U
#define BIPLANE_RENDER_PATCH_HEIGHT      48U

#define BIPLANE_SKY_COLOR                ILI9341_COLOR565(12, 185, 230)
#define BIPLANE_CLOUD_COLOR              ILI9341_COLOR565(155, 235, 240)
#define BIPLANE_CLOUD_SHADE              ILI9341_COLOR565(105, 215, 225)
#define BIPLANE_GRASS_COLOR              ILI9341_COLOR565(105, 185, 45)
#define BIPLANE_GRASS_DARK               ILI9341_COLOR565(50, 115, 35)
#define BIPLANE_DIRT_COLOR               ILI9341_COLOR565(150, 105, 55)
#define BIPLANE_DIRT_DARK                ILI9341_COLOR565(95, 70, 45)
#define BIPLANE_HOUSE_WALL               ILI9341_COLOR565(240, 225, 190)
#define BIPLANE_HOUSE_ROOF               ILI9341_COLOR565(180, 55, 40)
#define BIPLANE_HOUSE_DARK               ILI9341_COLOR565(95, 55, 40)

#define BIPLANE_PLAYER_BLUE              ILI9341_COLOR565(45, 125, 230)
#define BIPLANE_PLAYER_LIGHT             ILI9341_COLOR565(95, 220, 255)
#define BIPLANE_ENEMY_ORANGE             ILI9341_COLOR565(245, 120, 35)
#define BIPLANE_ENEMY_RED                ILI9341_COLOR565(190, 35, 30)
#define BIPLANE_ZEPPELIN_GREEN           ILI9341_COLOR565(130, 220, 120)
#define BIPLANE_ZEPPELIN_DARK            ILI9341_COLOR565(35, 145, 65)
#define BIPLANE_ZEPPELIN_HURT            ILI9341_COLOR565(220, 215, 85)
#define BIPLANE_BOMB_GRAY                ILI9341_COLOR565(110, 120, 125)
#define BIPLANE_BOMB_DARK                ILI9341_COLOR565(55, 65, 75)

#if (BIPLANE_RENDER_PATCH_WIDTH * \
     BIPLANE_RENDER_PATCH_HEIGHT * 2U) > RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for biplane duel patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} BiplaneRenderRect;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t facing;
    uint8_t visible;
} BiplanePlayerSnapshot;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t health;
    uint8_t active;
} BiplaneZeppelinSnapshot;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t active;
} BiplaneBulletSnapshot;

typedef struct {
    int16_t x;
    int16_t y;
    int8_t facing;
    uint8_t health;
    uint8_t active;
} BiplaneEnemySnapshot;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t radius;
    uint8_t active;
} BiplaneExplosionSnapshot;

#define biplane_patch_buffer render_scratch_buffer
static BiplanePlayerSnapshot previous_player;
static BiplaneZeppelinSnapshot previous_zeppelin;
static BiplaneBulletSnapshot previous_bullets[
    BIPLANE_DUEL_MAX_BULLETS];
static BiplaneBulletSnapshot previous_enemy_bullets[
    BIPLANE_DUEL_MAX_ENEMY_BULLETS];
static BiplaneBulletSnapshot previous_bombs[
    BIPLANE_DUEL_MAX_BOMBS];
static BiplaneEnemySnapshot previous_enemies[
    BIPLANE_DUEL_MAX_ENEMIES];
static BiplaneExplosionSnapshot previous_explosions[
    BIPLANE_DUEL_MAX_EXPLOSIONS];
static uint8_t biplane_renderer_initialized;

static int16_t BiplaneRender_Round(float value) {
    return (int16_t)(
        value + ((value >= 0.0f) ? 0.5f : -0.5f));
}

static int16_t BiplaneRender_Abs16(int16_t value) {
    return (value < 0) ? (int16_t)-value : value;
}

static uint8_t BiplaneRender_PointInCircle(
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

static void BiplaneRender_ClipRect(BiplaneRenderRect *rect) {
    if (rect->x0 < 0) {
        rect->x0 = 0;
    }
    if (rect->y0 < BIPLANE_DUEL_PLAYFIELD_TOP) {
        rect->y0 = BIPLANE_DUEL_PLAYFIELD_TOP;
    }
    if (rect->x1 >= BIPLANE_DUEL_SCREEN_WIDTH) {
        rect->x1 = BIPLANE_DUEL_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= BIPLANE_DUEL_SCREEN_HEIGHT) {
        rect->y1 = BIPLANE_DUEL_SCREEN_HEIGHT - 1;
    }
}

static void BiplaneRender_IncludeRect(
    BiplaneRenderRect *destination,
    BiplaneRenderRect source) {
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

static uint8_t BiplaneRender_RectsOverlap(
    BiplaneRenderRect first,
    BiplaneRenderRect second) {
    return (first.x0 <= second.x1) &&
           (first.x1 >= second.x0) &&
           (first.y0 <= second.y1) &&
           (first.y1 >= second.y0);
}

static uint16_t BiplaneRender_StaticPixel(int16_t x, int16_t y) {
    if (y < BIPLANE_DUEL_PLAYFIELD_TOP) {
        return ILI9341_BLACK;
    }

    if ((y >= 184) && (y < 197)) {
        int16_t half_width = (int16_t)(6 + ((y - 184) * 3));

        if (BiplaneRender_Abs16(x - 168) <= half_width) {
            return BIPLANE_HOUSE_ROOF;
        }
    }
    if ((x >= 145) && (x <= 191) &&
        (y >= 197) && (y < BIPLANE_DUEL_GROUND_Y)) {
        if ((x == 145) || (x == 191) || (y == 197)) {
            return BIPLANE_HOUSE_DARK;
        }
        if ((x >= 164) && (x <= 174) && (y >= 202)) {
            return BIPLANE_HOUSE_DARK;
        }
        if (((x >= 151) && (x <= 158)) ||
            ((x >= 179) && (x <= 186))) {
            if ((y >= 201) && (y <= 207)) {
                return ILI9341_CYAN;
            }
        }
        return BIPLANE_HOUSE_WALL;
    }

    if (y >= BIPLANE_DUEL_GROUND_Y) {
        if (y < (BIPLANE_DUEL_GROUND_Y + 4)) {
            return BIPLANE_GRASS_COLOR;
        }
        return BIPLANE_DIRT_COLOR;
    }

    if (BiplaneRender_PointInCircle(x, y, 24, 45, 22) ||
        BiplaneRender_PointInCircle(x, y, 47, 43, 18) ||
        BiplaneRender_PointInCircle(x, y, 16, 70, 18) ||
        BiplaneRender_PointInCircle(x, y, 47, 72, 20)) {
        return BIPLANE_CLOUD_COLOR;
    }
    if (BiplaneRender_PointInCircle(x, y, 112, 152, 19) ||
        BiplaneRender_PointInCircle(x, y, 138, 150, 25) ||
        BiplaneRender_PointInCircle(x, y, 164, 159, 18) ||
        BiplaneRender_PointInCircle(x, y, 285, 182, 21)) {
        return BIPLANE_CLOUD_SHADE;
    }
    return BIPLANE_SKY_COLOR;
}

static void BiplaneRender_PutPixel(BiplaneRenderRect rect,
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
    biplane_patch_buffer[offset] = (uint8_t)(color >> 8);
    biplane_patch_buffer[offset + 1U] =
        (uint8_t)(color & 0xFFU);
}

static void BiplaneRender_FillPatchBackground(
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    uint32_t offset = 0U;

    (void)width;
    (void)height;
    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color = BiplaneRender_StaticPixel(x, y);

            biplane_patch_buffer[offset++] =
                (uint8_t)(color >> 8);
            biplane_patch_buffer[offset++] =
                (uint8_t)(color & 0xFFU);
        }
    }
}

static uint8_t BiplaneRender_PointInPlane(
    int16_t x,
    int16_t y,
    int16_t center_x,
    int16_t center_y,
    int8_t facing,
    uint8_t enemy,
    uint16_t *color) {
    int16_t dx = (int16_t)((x - center_x) * facing);
    int16_t dy = y - center_y;
    int16_t ady = BiplaneRender_Abs16(dy);
    uint16_t body = (enemy != 0U) ?
                    BIPLANE_ENEMY_ORANGE :
                    BIPLANE_PLAYER_BLUE;
    uint16_t wing = (enemy != 0U) ?
                    BIPLANE_ENEMY_RED :
                    BIPLANE_PLAYER_LIGHT;

    if ((dx >= 14) && (dx <= 17) && (ady <= 5)) {
        *color = ILI9341_WHITE;
        return 1U;
    }
    if ((dx >= -11) && (dx <= 7) &&
        (((dy >= -9) && (dy <= -5)) ||
         ((dy >= 5) && (dy <= 8)))) {
        *color = wing;
        return 1U;
    }
    if (((dx == -4) || (dx == 5)) &&
        (dy >= -5) && (dy <= 5)) {
        *color = ILI9341_GRAY;
        return 1U;
    }
    if ((dx >= -14) && (dx <= 13) && (ady <= 3)) {
        *color = body;
        if ((dx >= -1) && (dx <= 4) &&
            (dy >= -5) && (dy <= -2)) {
            *color = ILI9341_CYAN;
        }
        if ((dx <= -11) || (dx >= 11) || (ady == 3)) {
            *color = ILI9341_WHITE;
        }
        return 1U;
    }
    if ((dx >= -17) && (dx <= -11) && (ady <= 6)) {
        *color = wing;
        return 1U;
    }
    return 0U;
}

static void BiplaneRender_DrawPlaneToPatch(
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height,
    int16_t center_x,
    int16_t center_y,
    int8_t facing,
    uint8_t enemy) {
    BiplaneRenderRect bounds = {
        center_x - 18,
        center_y - 11,
        center_x + 18,
        center_y + 11
    };

    (void)height;
    if (BiplaneRender_RectsOverlap(rect, bounds) == 0U) {
        return;
    }
    for (int16_t y = bounds.y0; y <= bounds.y1; y++) {
        for (int16_t x = bounds.x0; x <= bounds.x1; x++) {
            uint16_t color;

            if (BiplaneRender_PointInPlane(
                    x, y, center_x, center_y,
                    facing, enemy, &color) != 0U) {
                BiplaneRender_PutPixel(
                    rect, width, height, x, y, color);
            }
        }
    }
}

static void BiplaneRender_DrawZeppelinToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    int16_t center_x;
    int16_t center_y;
    int8_t direction;
    uint16_t body_color;
    BiplaneRenderRect bounds;

    if (game->zeppelin.active == 0U) {
        return;
    }
    center_x = BiplaneRender_Round(game->zeppelin.x);
    center_y = BiplaneRender_Round(game->zeppelin.y);
    direction = (game->zeppelin.vx >= 0.0f) ? 1 : -1;
    body_color = (game->zeppelin.health <= 1U) ?
                 BIPLANE_ZEPPELIN_HURT :
                 BIPLANE_ZEPPELIN_GREEN;
    bounds.x0 = center_x - 36;
    bounds.y0 = center_y - 17;
    bounds.x1 = center_x + 36;
    bounds.y1 = center_y + 19;
    if (BiplaneRender_RectsOverlap(rect, bounds) == 0U) {
        return;
    }

    for (int16_t y = bounds.y0; y <= bounds.y1; y++) {
        for (int16_t x = bounds.x0; x <= bounds.x1; x++) {
            int16_t dx = x - center_x;
            int16_t dy = y - center_y;
            uint8_t body =
                (((int32_t)dx * dx * 121) +
                 ((int32_t)dy * dy * 1089)) <=
                (121L * 1089L);

            if (body != 0U) {
                uint16_t color = body_color;

                if ((BiplaneRender_Abs16(dx) > 29) ||
                    (BiplaneRender_Abs16(dy) > 10)) {
                    color = BIPLANE_ZEPPELIN_DARK;
                }
                if (((dx == -12) || (dx == 0) || (dx == 12)) &&
                    (dy >= -2) && (dy <= 2)) {
                    color = ILI9341_PINK;
                }
                BiplaneRender_PutPixel(
                    rect, width, height, x, y, color);
                continue;
            }
            if (((dx * direction) <= -28) &&
                ((dx * direction) >= -36) &&
                (dy >= -12) && (dy <= 12) &&
                (BiplaneRender_Abs16(dy) <=
                 (int16_t)(14 - BiplaneRender_Abs16(dx * direction + 28)))) {
                BiplaneRender_PutPixel(
                    rect, width, height,
                    x, y, BIPLANE_ZEPPELIN_DARK);
            }
        }
    }
    for (int16_t y = center_y + 10; y <= center_y + 16; y++) {
        for (int16_t x = center_x - 13; x <= center_x + 13; x++) {
            uint16_t color =
                ((y == center_y + 10) ||
                 (y == center_y + 16) ||
                 (x == center_x - 13) ||
                 (x == center_x + 13)) ?
                BIPLANE_BOMB_DARK : ILI9341_GRAY;

            BiplaneRender_PutPixel(
                rect, width, height, x, y, color);
        }
    }
}

static void BiplaneRender_DrawBombsToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        const BiplaneDuelBomb *bomb = &game->bombs[i];
        int16_t center_x;
        int16_t center_y;
        BiplaneRenderRect bounds;

        if (bomb->active == 0U) {
            continue;
        }
        center_x = BiplaneRender_Round(bomb->x);
        center_y = BiplaneRender_Round(bomb->y);
        bounds.x0 = center_x - 7;
        bounds.y0 = center_y - 10;
        bounds.x1 = center_x + 7;
        bounds.y1 = center_y + 8;
        if (BiplaneRender_RectsOverlap(rect, bounds) == 0U) {
            continue;
        }
        for (int16_t y = bounds.y0; y <= bounds.y1; y++) {
            for (int16_t x = bounds.x0; x <= bounds.x1; x++) {
                int16_t dx = x - center_x;
                int16_t dy = y - center_y;

                if ((dy >= -10) && (dy <= -5) &&
                    (BiplaneRender_Abs16(dx) <= (int16_t)(dy + 11))) {
                    BiplaneRender_PutPixel(
                        rect, width, height, x, y,
                        (dy < -7) ? ILI9341_YELLOW :
                        ILI9341_ORANGE);
                } else if (BiplaneRender_PointInCircle(
                               x, y, center_x, center_y, 5) != 0U) {
                    BiplaneRender_PutPixel(
                        rect, width, height, x, y,
                        (dx < -1) ? BIPLANE_BOMB_GRAY :
                        BIPLANE_BOMB_DARK);
                }
            }
        }
    }
}

static void BiplaneRender_DrawBulletsToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        const BiplaneDuelBullet *bullet = &game->bullets[i];
        int16_t bullet_x;
        int16_t bullet_y;
        int16_t direction;

        if (bullet->active == 0U) {
            continue;
        }
        bullet_x = BiplaneRender_Round(bullet->x);
        bullet_y = BiplaneRender_Round(bullet->y);
        direction = (bullet->vx >= 0.0f) ? 1 : -1;
        for (int16_t x = bullet_x - (direction < 0 ? 5 : 0);
             x <= bullet_x + (direction > 0 ? 5 : 0);
             x++) {
            BiplaneRender_PutPixel(
                rect, width, height, x, bullet_y,
                ILI9341_YELLOW);
            BiplaneRender_PutPixel(
                rect, width, height, x, bullet_y + 1,
                ILI9341_WHITE);
        }
    }

    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        const BiplaneDuelEnemyBullet *bullet =
            &game->enemy_bullets[i];
        int16_t bullet_x;
        int16_t bullet_y;

        if (bullet->active == 0U) {
            continue;
        }
        bullet_x = BiplaneRender_Round(bullet->x);
        bullet_y = BiplaneRender_Round(bullet->y);
        for (int16_t y = bullet_y - 1; y <= bullet_y + 1; y++) {
            for (int16_t x = bullet_x - 2; x <= bullet_x + 2; x++) {
                BiplaneRender_PutPixel(
                    rect, width, height, x, y,
                    (y == bullet_y) ? ILI9341_RED :
                    ILI9341_ORANGE);
            }
        }
    }
}

static void BiplaneRender_DrawEnemiesToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        const BiplaneDuelEnemy *enemy = &game->enemies[i];

        if (enemy->active == 0U) {
            continue;
        }
        BiplaneRender_DrawPlaneToPatch(
            rect, width, height,
            BiplaneRender_Round(enemy->x),
            BiplaneRender_Round(enemy->y),
            enemy->facing,
            1U);
    }
}

static uint8_t BiplaneRender_PlayerVisible(
    const BiplaneDuelState *game) {
    if (game->phase != BIPLANE_DUEL_PHASE_PLAYING) {
        return 0U;
    }
    if ((game->invulnerable_ticks != 0U) &&
        (((game->ticks / 3U) & 1U) != 0U)) {
        return 0U;
    }
    return 1U;
}

static void BiplaneRender_DrawPlayerToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    if (BiplaneRender_PlayerVisible(game) == 0U) {
        return;
    }
    BiplaneRender_DrawPlaneToPatch(
        rect, width, height,
        BiplaneRender_Round(game->player.x),
        BiplaneRender_Round(game->player.y),
        game->player.facing,
        0U);
}

static void BiplaneRender_DrawExplosionsToPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect,
    uint16_t width,
    uint16_t height) {
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_EXPLOSIONS; i++) {
        const BiplaneDuelExplosion *explosion =
            &game->explosions[i];
        int16_t center_x;
        int16_t center_y;

        if (explosion->active == 0U) {
            continue;
        }
        center_x = BiplaneRender_Round(explosion->x);
        center_y = BiplaneRender_Round(explosion->y);
        for (int16_t y = center_y - explosion->radius;
             y <= center_y + explosion->radius;
             y++) {
            for (int16_t x = center_x - explosion->radius;
                 x <= center_x + explosion->radius;
                 x++) {
                int16_t dx = x - center_x;
                int16_t dy = y - center_y;
                int32_t distance = ((int32_t)dx * dx) +
                                   ((int32_t)dy * dy);
                int32_t outer =
                    (int32_t)explosion->radius *
                    explosion->radius;
                int32_t inner =
                    (int32_t)(explosion->radius / 2) *
                    (explosion->radius / 2);

                if (distance <= inner) {
                    BiplaneRender_PutPixel(
                        rect, width, height, x, y,
                        ILI9341_YELLOW);
                } else if (distance <= outer) {
                    BiplaneRender_PutPixel(
                        rect, width, height, x, y,
                        ILI9341_ORANGE);
                }
            }
        }
    }
}

static uint8_t BiplaneRender_DrawPatch(
    const BiplaneDuelState *game,
    BiplaneRenderRect rect) {
    uint16_t width;
    uint16_t height;

    BiplaneRender_ClipRect(&rect);
    if ((rect.x1 < rect.x0) || (rect.y1 < rect.y0)) {
        return 1U;
    }
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);
    if ((width > BIPLANE_RENDER_PATCH_WIDTH) ||
        (height > BIPLANE_RENDER_PATCH_HEIGHT)) {
        return 0U;
    }

    BiplaneRender_FillPatchBackground(rect, width, height);
    BiplaneRender_DrawZeppelinToPatch(game, rect, width, height);
    BiplaneRender_DrawEnemiesToPatch(game, rect, width, height);
    BiplaneRender_DrawBombsToPatch(game, rect, width, height);
    BiplaneRender_DrawBulletsToPatch(game, rect, width, height);
    BiplaneRender_DrawExplosionsToPatch(game, rect, width, height);
    BiplaneRender_DrawPlayerToPatch(game, rect, width, height);

    ILI9341_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        biplane_patch_buffer);
    return 1U;
}

static BiplaneRenderRect BiplaneRender_PlayerRect(
    int16_t x,
    int16_t y) {
    BiplaneRenderRect rect = {
        x - 19, y - 12, x + 19, y + 12
    };
    return rect;
}

static BiplaneRenderRect BiplaneRender_ZeppelinRect(
    int16_t x,
    int16_t y) {
    BiplaneRenderRect rect = {
        x - 37, y - 18, x + 37, y + 20
    };
    return rect;
}

static BiplaneRenderRect BiplaneRender_EnemyRect(
    int16_t x,
    int16_t y) {
    BiplaneRenderRect rect = {
        x - 18, y - 12, x + 18, y + 12
    };
    return rect;
}

static BiplaneRenderRect BiplaneRender_BulletRect(
    int16_t x,
    int16_t y) {
    BiplaneRenderRect rect = {
        x - 7, y - 4, x + 7, y + 4
    };
    return rect;
}

static BiplaneRenderRect BiplaneRender_BombRect(
    int16_t x,
    int16_t y) {
    BiplaneRenderRect rect = {
        x - 8, y - 11, x + 8, y + 9
    };
    return rect;
}

static BiplaneRenderRect BiplaneRender_ExplosionRect(
    int16_t x,
    int16_t y,
    uint8_t radius) {
    BiplaneRenderRect rect = {
        x - radius - 2,
        y - radius - 2,
        x + radius + 2,
        y + radius + 2
    };
    return rect;
}

static void BiplaneRender_DrawHud(
    const BiplaneDuelState *game) {
    char text[24];

    ILI9341_FillRectangle_DMA(
        0, 0, BIPLANE_DUEL_SCREEN_WIDTH,
        BIPLANE_DUEL_PLAYFIELD_TOP, ILI9341_BLACK);
    ILI9341_FillRectangle_DMA(
        0, BIPLANE_DUEL_PLAYFIELD_TOP - 1,
        BIPLANE_DUEL_SCREEN_WIDTH, 1, ILI9341_GRAY);
    ILI9341_WriteString_DMA(
        4, 4, "BIPLANE DUEL", Font_11x18,
        ILI9341_WHITE, ILI9341_BLACK);
    snprintf(text, sizeof(text), "L%u %05lu",
             (unsigned int)game->level,
             (unsigned long)game->score);
    ILI9341_WriteString_DMA(
        158, 9, text, Font_7x10,
        ILI9341_CYAN, ILI9341_BLACK);
    snprintf(text, sizeof(text), "SH %u",
             (unsigned int)game->shields);
    ILI9341_WriteString_DMA(
        280, 9, text, Font_7x10,
        (game->shields > 1U) ?
        ILI9341_GREEN : ILI9341_ORANGE,
        ILI9341_BLACK);
}

static void BiplaneRender_DrawStaticField(void) {
    ILI9341_FillScreen_DMA(BIPLANE_SKY_COLOR);

    ILI9341_FillCircle_DMA(24, 45, 22, BIPLANE_CLOUD_COLOR);
    ILI9341_FillCircle_DMA(47, 43, 18, BIPLANE_CLOUD_COLOR);
    ILI9341_FillCircle_DMA(16, 70, 18, BIPLANE_CLOUD_COLOR);
    ILI9341_FillCircle_DMA(47, 72, 20, BIPLANE_CLOUD_COLOR);
    ILI9341_FillCircle_DMA(112, 152, 19, BIPLANE_CLOUD_SHADE);
    ILI9341_FillCircle_DMA(138, 150, 25, BIPLANE_CLOUD_SHADE);
    ILI9341_FillCircle_DMA(164, 159, 18, BIPLANE_CLOUD_SHADE);
    ILI9341_FillCircle_DMA(285, 182, 21, BIPLANE_CLOUD_SHADE);

    for (uint16_t y = 184U; y < 197U; y++) {
        uint16_t half_width =
            (uint16_t)(6U + ((y - 184U) * 3U));

        ILI9341_FillRectangle_DMA(
            (uint16_t)(168U - half_width), y,
            (uint16_t)((half_width * 2U) + 1U), 1,
            BIPLANE_HOUSE_ROOF);
    }
    ILI9341_FillRectangle_DMA(
        145, 197, 47,
        BIPLANE_DUEL_GROUND_Y - 197,
        BIPLANE_HOUSE_WALL);
    ILI9341_FillRectangle_DMA(
        145, 197, 47, 1, BIPLANE_HOUSE_DARK);
    ILI9341_FillRectangle_DMA(
        145, 197, 1,
        BIPLANE_DUEL_GROUND_Y - 197,
        BIPLANE_HOUSE_DARK);
    ILI9341_FillRectangle_DMA(
        191, 197, 1,
        BIPLANE_DUEL_GROUND_Y - 197,
        BIPLANE_HOUSE_DARK);
    ILI9341_FillRectangle_DMA(
        151, 201, 8, 7, ILI9341_CYAN);
    ILI9341_FillRectangle_DMA(
        179, 201, 8, 7, ILI9341_CYAN);
    ILI9341_FillRectangle_DMA(
        164, 202, 11,
        BIPLANE_DUEL_GROUND_Y - 202,
        BIPLANE_HOUSE_DARK);

    ILI9341_FillRectangle_DMA(
        0, BIPLANE_DUEL_GROUND_Y,
        BIPLANE_DUEL_SCREEN_WIDTH, 4,
        BIPLANE_GRASS_COLOR);
    ILI9341_FillRectangle_DMA(
        0, BIPLANE_DUEL_GROUND_Y + 4,
        BIPLANE_DUEL_SCREEN_WIDTH,
        BIPLANE_DUEL_SCREEN_HEIGHT -
        BIPLANE_DUEL_GROUND_Y - 4,
        BIPLANE_DIRT_COLOR);
}

static void BiplaneRender_SaveSnapshots(
    const BiplaneDuelState *game) {
    previous_player.x = BiplaneRender_Round(game->player.x);
    previous_player.y = BiplaneRender_Round(game->player.y);
    previous_player.facing = game->player.facing;
    previous_player.visible = BiplaneRender_PlayerVisible(game);

    previous_zeppelin.x = BiplaneRender_Round(game->zeppelin.x);
    previous_zeppelin.y = BiplaneRender_Round(game->zeppelin.y);
    previous_zeppelin.health = game->zeppelin.health;
    previous_zeppelin.active = game->zeppelin.active;

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        previous_bullets[i].x =
            BiplaneRender_Round(game->bullets[i].x);
        previous_bullets[i].y =
            BiplaneRender_Round(game->bullets[i].y);
        previous_bullets[i].active = game->bullets[i].active;
    }
    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        previous_enemy_bullets[i].x =
            BiplaneRender_Round(game->enemy_bullets[i].x);
        previous_enemy_bullets[i].y =
            BiplaneRender_Round(game->enemy_bullets[i].y);
        previous_enemy_bullets[i].active =
            game->enemy_bullets[i].active;
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        previous_bombs[i].x =
            BiplaneRender_Round(game->bombs[i].x);
        previous_bombs[i].y =
            BiplaneRender_Round(game->bombs[i].y);
        previous_bombs[i].active = game->bombs[i].active;
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        previous_enemies[i].x =
            BiplaneRender_Round(game->enemies[i].x);
        previous_enemies[i].y =
            BiplaneRender_Round(game->enemies[i].y);
        previous_enemies[i].facing = game->enemies[i].facing;
        previous_enemies[i].health = game->enemies[i].health;
        previous_enemies[i].active = game->enemies[i].active;
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_EXPLOSIONS; i++) {
        previous_explosions[i].x =
            BiplaneRender_Round(game->explosions[i].x);
        previous_explosions[i].y =
            BiplaneRender_Round(game->explosions[i].y);
        previous_explosions[i].radius =
            game->explosions[i].radius;
        previous_explosions[i].active =
            game->explosions[i].active;
    }
}

static void BiplaneRender_DrawObjects(
    const BiplaneDuelState *game) {
    if (game->zeppelin.active != 0U) {
        BiplaneRender_DrawPatch(
            game,
            BiplaneRender_ZeppelinRect(
                BiplaneRender_Round(game->zeppelin.x),
                BiplaneRender_Round(game->zeppelin.y)));
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        if (game->enemies[i].active != 0U) {
            BiplaneRender_DrawPatch(
                game,
                BiplaneRender_EnemyRect(
                    BiplaneRender_Round(game->enemies[i].x),
                    BiplaneRender_Round(game->enemies[i].y)));
        }
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        if (game->bombs[i].active != 0U) {
            BiplaneRender_DrawPatch(
                game,
                BiplaneRender_BombRect(
                    BiplaneRender_Round(game->bombs[i].x),
                    BiplaneRender_Round(game->bombs[i].y)));
        }
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        if (game->bullets[i].active != 0U) {
            BiplaneRender_DrawPatch(
                game,
                BiplaneRender_BulletRect(
                    BiplaneRender_Round(game->bullets[i].x),
                    BiplaneRender_Round(game->bullets[i].y)));
        }
    }
    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        if (game->enemy_bullets[i].active != 0U) {
            BiplaneRender_DrawPatch(
                game,
                BiplaneRender_BulletRect(
                    BiplaneRender_Round(game->enemy_bullets[i].x),
                    BiplaneRender_Round(game->enemy_bullets[i].y)));
        }
    }
    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_EXPLOSIONS; i++) {
        if (game->explosions[i].active != 0U) {
            BiplaneRender_DrawPatch(
                game,
                BiplaneRender_ExplosionRect(
                    BiplaneRender_Round(game->explosions[i].x),
                    BiplaneRender_Round(game->explosions[i].y),
                    game->explosions[i].radius));
        }
    }
    if (BiplaneRender_PlayerVisible(game) != 0U) {
        BiplaneRender_DrawPatch(
            game,
            BiplaneRender_PlayerRect(
                BiplaneRender_Round(game->player.x),
                BiplaneRender_Round(game->player.y)));
    }
}

static void BiplaneRender_DrawWholeField(
    const BiplaneDuelState *game) {
    BiplaneRender_DrawStaticField();
    BiplaneRender_DrawHud(game);
    BiplaneRender_DrawObjects(game);

    if (game->phase == BIPLANE_DUEL_PHASE_RESTART_PAUSE) {
        ILI9341_WriteString_DMA(
            74, 88, "PLANE LOST", Font_16x26,
            ILI9341_ORANGE, BIPLANE_SKY_COLOR);
        ILI9341_WriteString_DMA(
            108, 123, "AUTO RESTART", Font_7x10,
            ILI9341_CYAN, BIPLANE_SKY_COLOR);
    }

    BiplaneRender_SaveSnapshots(game);
    biplane_renderer_initialized = 1U;
}

static uint8_t BiplaneRender_DrawDirty(
    const BiplaneDuelState *game,
    BiplaneRenderRect dirty) {
    if (BiplaneRender_DrawPatch(game, dirty) == 0U) {
        BiplaneRender_DrawWholeField(game);
        return 0U;
    }
    return 1U;
}

void BiplaneDuelRender_Init(const BiplaneDuelState *game) {
    biplane_renderer_initialized = 0U;
    BiplaneRender_DrawWholeField(game);
}

void BiplaneDuelRender_Frame(const BiplaneDuelState *game,
                             BiplaneDuelEvent event) {
    BiplanePlayerSnapshot current_player;
    BiplaneZeppelinSnapshot current_zeppelin;

    if ((biplane_renderer_initialized == 0U) ||
        ((event & (BIPLANE_DUEL_EVENT_GAME_STARTED |
                   BIPLANE_DUEL_EVENT_PLAYER_HIT |
                   BIPLANE_DUEL_EVENT_RESTART_PAUSE)) != 0U)) {
        BiplaneRender_DrawWholeField(game);
        return;
    }

    current_zeppelin.x = BiplaneRender_Round(game->zeppelin.x);
    current_zeppelin.y = BiplaneRender_Round(game->zeppelin.y);
    current_zeppelin.health = game->zeppelin.health;
    current_zeppelin.active = game->zeppelin.active;
    if ((current_zeppelin.active != previous_zeppelin.active) ||
        (current_zeppelin.x != previous_zeppelin.x) ||
        (current_zeppelin.y != previous_zeppelin.y) ||
        (current_zeppelin.health != previous_zeppelin.health)) {
        BiplaneRenderRect dirty =
            (previous_zeppelin.active != 0U) ?
            BiplaneRender_ZeppelinRect(
                previous_zeppelin.x, previous_zeppelin.y) :
            BiplaneRender_ZeppelinRect(
                current_zeppelin.x, current_zeppelin.y);

        if (current_zeppelin.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_ZeppelinRect(
                    current_zeppelin.x, current_zeppelin.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_ENEMIES; i++) {
        const BiplaneDuelEnemy *enemy = &game->enemies[i];
        const BiplaneEnemySnapshot *previous =
            &previous_enemies[i];
        BiplaneEnemySnapshot current;

        current.x = BiplaneRender_Round(enemy->x);
        current.y = BiplaneRender_Round(enemy->y);
        current.facing = enemy->facing;
        current.health = enemy->health;
        current.active = enemy->active;
        if ((current.active == previous->active) &&
            (current.x == previous->x) &&
            (current.y == previous->y) &&
            (current.facing == previous->facing) &&
            (current.health == previous->health)) {
            continue;
        }

        BiplaneRenderRect dirty =
            (previous->active != 0U) ?
            BiplaneRender_EnemyRect(previous->x, previous->y) :
            BiplaneRender_EnemyRect(current.x, current.y);
        if (current.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_EnemyRect(current.x, current.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BOMBS; i++) {
        const BiplaneDuelBomb *bomb = &game->bombs[i];
        const BiplaneBulletSnapshot *previous =
            &previous_bombs[i];
        BiplaneBulletSnapshot current;

        current.x = BiplaneRender_Round(bomb->x);
        current.y = BiplaneRender_Round(bomb->y);
        current.active = bomb->active;
        if ((current.active == previous->active) &&
            (current.x == previous->x) &&
            (current.y == previous->y)) {
            continue;
        }

        BiplaneRenderRect dirty =
            (previous->active != 0U) ?
            BiplaneRender_BombRect(previous->x, previous->y) :
            BiplaneRender_BombRect(current.x, current.y);
        if (current.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_BombRect(current.x, current.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    for (uint8_t i = 0U; i < BIPLANE_DUEL_MAX_BULLETS; i++) {
        const BiplaneDuelBullet *bullet = &game->bullets[i];
        const BiplaneBulletSnapshot *previous =
            &previous_bullets[i];
        BiplaneBulletSnapshot current;

        current.x = BiplaneRender_Round(bullet->x);
        current.y = BiplaneRender_Round(bullet->y);
        current.active = bullet->active;
        if ((current.active == previous->active) &&
            (current.x == previous->x) &&
            (current.y == previous->y)) {
            continue;
        }

        BiplaneRenderRect dirty =
            (previous->active != 0U) ?
            BiplaneRender_BulletRect(previous->x, previous->y) :
            BiplaneRender_BulletRect(current.x, current.y);
        if (current.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_BulletRect(current.x, current.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_ENEMY_BULLETS;
         i++) {
        const BiplaneDuelEnemyBullet *bullet =
            &game->enemy_bullets[i];
        const BiplaneBulletSnapshot *previous =
            &previous_enemy_bullets[i];
        BiplaneBulletSnapshot current;

        current.x = BiplaneRender_Round(bullet->x);
        current.y = BiplaneRender_Round(bullet->y);
        current.active = bullet->active;
        if ((current.active == previous->active) &&
            (current.x == previous->x) &&
            (current.y == previous->y)) {
            continue;
        }

        BiplaneRenderRect dirty =
            (previous->active != 0U) ?
            BiplaneRender_BulletRect(previous->x, previous->y) :
            BiplaneRender_BulletRect(current.x, current.y);
        if (current.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_BulletRect(current.x, current.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    for (uint8_t i = 0U;
         i < BIPLANE_DUEL_MAX_EXPLOSIONS;
         i++) {
        const BiplaneDuelExplosion *explosion =
            &game->explosions[i];
        const BiplaneExplosionSnapshot *previous =
            &previous_explosions[i];
        BiplaneExplosionSnapshot current;

        current.x = BiplaneRender_Round(explosion->x);
        current.y = BiplaneRender_Round(explosion->y);
        current.radius = explosion->radius;
        current.active = explosion->active;
        if ((current.active == previous->active) &&
            (current.x == previous->x) &&
            (current.y == previous->y) &&
            (current.radius == previous->radius)) {
            continue;
        }

        BiplaneRenderRect dirty =
            (previous->active != 0U) ?
            BiplaneRender_ExplosionRect(
                previous->x, previous->y, previous->radius) :
            BiplaneRender_ExplosionRect(
                current.x, current.y, current.radius);
        if (current.active != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_ExplosionRect(
                    current.x, current.y, current.radius));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    current_player.x = BiplaneRender_Round(game->player.x);
    current_player.y = BiplaneRender_Round(game->player.y);
    current_player.facing = game->player.facing;
    current_player.visible = BiplaneRender_PlayerVisible(game);
    if ((current_player.visible != previous_player.visible) ||
        (current_player.x != previous_player.x) ||
        (current_player.y != previous_player.y) ||
        (current_player.facing != previous_player.facing)) {
        BiplaneRenderRect dirty =
            (previous_player.visible != 0U) ?
            BiplaneRender_PlayerRect(
                previous_player.x, previous_player.y) :
            BiplaneRender_PlayerRect(
                current_player.x, current_player.y);

        if (current_player.visible != 0U) {
            BiplaneRender_IncludeRect(
                &dirty,
                BiplaneRender_PlayerRect(
                    current_player.x, current_player.y));
        }
        if (BiplaneRender_DrawDirty(game, dirty) == 0U) {
            return;
        }
    }

    if ((event & BIPLANE_DUEL_EVENT_HUD_CHANGED) != 0U) {
        BiplaneRender_DrawHud(game);
    }
    BiplaneRender_SaveSnapshots(game);
}
