#include "life_game_render.h"

#include <stdio.h>
#include "display_driver.h"

#define LIFE_RENDER_BACKGROUND       DISPLAY_COLOR565(2, 6, 9)
#define LIFE_RENDER_PANEL            DISPLAY_COLOR565(8, 17, 20)
#define LIFE_RENDER_BOARD            DISPLAY_COLOR565(0, 2, 3)
#define LIFE_RENDER_CELL             DISPLAY_COLOR565(70, 235, 155)
#define LIFE_RENDER_CELL_BRIGHT      DISPLAY_COLOR565(145, 255, 205)
#define LIFE_RENDER_GRID_DARK        DISPLAY_COLOR565(5, 16, 18)
#define LIFE_RENDER_BAR_BACK         DISPLAY_COLOR565(24, 35, 38)
#define LIFE_RENDER_BAR_FILL         DISPLAY_COLOR565(245, 185, 55)

static uint8_t life_renderer_initialized;
static uint32_t displayed_generation;
static uint32_t displayed_population;
static LifeGamePhase displayed_phase;
static uint16_t displayed_shake_energy;
static uint8_t displayed_mpu_online;

static uint16_t LifeGameRender_CellColor(uint16_t row) {
    return ((row & 1U) == 0U) ?
           LIFE_RENDER_CELL : LIFE_RENDER_CELL_BRIGHT;
}

static void LifeGameRender_DrawHeaderStatic(void) {
    DISPLAY_FillRectangle_DMA(
        0, 0, LIFE_GAME_SCREEN_WIDTH,
        LIFE_GAME_HEADER_HEIGHT, DISPLAY_BLACK);
    DISPLAY_FillRectangle_DMA(
        0, LIFE_GAME_HEADER_HEIGHT - 1U,
        LIFE_GAME_SCREEN_WIDTH, 1, DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(
        4, 4, "CONWAY LIFE", Font_11x18,
        DISPLAY_WHITE, DISPLAY_BLACK);
}

static void LifeGameRender_DrawHudStats(
    const LifeGameState *game,
    uint8_t force) {
    char text[30];

    if ((force == 0U) &&
        (displayed_generation == game->generation) &&
        (displayed_population == game->population) &&
        (displayed_phase == game->phase)) {
        return;
    }

    DISPLAY_FillRectangle_DMA(
        142, 4, 178, 20, DISPLAY_BLACK);
    if (game->phase == LIFE_GAME_PHASE_RUNNING) {
        snprintf(text, sizeof(text), "G:%lu P:%lu",
                 (unsigned long)game->generation,
                 (unsigned long)game->population);
        DISPLAY_WriteString_DMA(
            146, 9, text, Font_7x10,
            DISPLAY_CYAN, DISPLAY_BLACK);
    } else {
        DISPLAY_WriteString_DMA(
            220, 9, "SHAKE", Font_7x10,
            LIFE_RENDER_BAR_FILL, DISPLAY_BLACK);
    }

    displayed_generation = game->generation;
    displayed_population = game->population;
    displayed_phase = game->phase;
}

static void LifeGameRender_DrawWaitMeter(
    const LifeGameState *game,
    uint8_t force) {
    char text[24];
    uint32_t width;
    uint16_t color;

    if ((force == 0U) &&
        (displayed_shake_energy == game->shake_energy) &&
        (displayed_mpu_online == game->mpu_online)) {
        return;
    }

    width =
        ((uint32_t)game->shake_energy * 210U) /
        LIFE_GAME_SHAKE_START_THRESHOLD;
    if (width > 210U) {
        width = 210U;
    }

    DISPLAY_FillRectangle_DMA(54, 151, 212, 12,
                              LIFE_RENDER_BAR_BACK);
    DISPLAY_FillRectangle_DMA(54, 151, 212, 1,
                              DISPLAY_GRAY);
    DISPLAY_FillRectangle_DMA(54, 162, 212, 1,
                              DISPLAY_GRAY);
    if (width > 0U) {
        color = (width >= 210U) ?
                DISPLAY_GREEN : LIFE_RENDER_BAR_FILL;
        DISPLAY_FillRectangle_DMA(
            55, 153, (uint16_t)width, 8, color);
    }

    DISPLAY_FillRectangle_DMA(
        78, 174, 178, 12, LIFE_RENDER_BACKGROUND);
    if (game->mpu_online != 0U) {
        snprintf(text, sizeof(text), "GYRO ENERGY:%5u",
                 (unsigned int)game->shake_energy);
        DISPLAY_WriteString_DMA(
            82, 176, text, Font_7x10,
            DISPLAY_WHITE, LIFE_RENDER_BACKGROUND);
    } else {
        DISPLAY_WriteString_DMA(
            96, 176, "MPU6000 OFFLINE",
            Font_7x10, DISPLAY_RED,
            LIFE_RENDER_BACKGROUND);
    }

    displayed_shake_energy = game->shake_energy;
    displayed_mpu_online = game->mpu_online;
}

static void LifeGameRender_DrawWaitScreen(
    LifeGameState *game) {
    DISPLAY_FillScreen_DMA(LIFE_RENDER_BACKGROUND);
    LifeGameRender_DrawHeaderStatic();
    LifeGameRender_DrawHudStats(game, 1U);

    DISPLAY_FillRectangle_DMA(24, 52, 272, 116,
                              LIFE_RENDER_PANEL);
    DISPLAY_WriteString_DMA(
        47, 72, "SHAKE MPU6000", Font_16x26,
        DISPLAY_WHITE, LIFE_RENDER_PANEL);
    DISPLAY_WriteString_DMA(
        62, 105, "TO SEED THE CELLS", Font_11x18,
        DISPLAY_CYAN, LIFE_RENDER_PANEL);
    DISPLAY_WriteString_DMA(
        72, 132, "strong gyro shake starts",
        Font_7x10, DISPLAY_GRAY, LIFE_RENDER_PANEL);

    displayed_shake_energy = 0xFFFFU;
    displayed_mpu_online = 0xFFU;
    LifeGameRender_DrawWaitMeter(game, 1U);

    DISPLAY_WriteString_DMA(
        9, 218, "UKEY SHORT:NEXT   LONG:RESET",
        Font_7x10, DISPLAY_WHITE,
        LIFE_RENDER_BACKGROUND);
}

static void LifeGameRender_DrawBoardBackground(void) {
    DISPLAY_FillRectangle_DMA(
        LIFE_GAME_BOARD_X, LIFE_GAME_BOARD_Y,
        LIFE_GAME_SCREEN_WIDTH,
        LIFE_GAME_ROWS * LIFE_GAME_CELL_SIZE,
        LIFE_RENDER_BOARD);

    for (uint16_t row = 0U; row < LIFE_GAME_ROWS; row += 5U) {
        DISPLAY_FillRectangle_DMA(
            LIFE_GAME_BOARD_X,
            LIFE_GAME_BOARD_Y + (row * LIFE_GAME_CELL_SIZE),
            LIFE_GAME_SCREEN_WIDTH, 1,
            LIFE_RENDER_GRID_DARK);
    }
}

static void LifeGameRender_DrawAliveRuns(
    const LifeGameState *game) {
    for (uint16_t row = 0U; row < LIFE_GAME_ROWS; row++) {
        uint16_t column = 0U;

        while (column < LIFE_GAME_COLUMNS) {
            uint16_t run = 0U;

            while ((column < LIFE_GAME_COLUMNS) &&
                   (LifeGame_CellAlive(game, column, row) == 0U)) {
                column++;
            }
            while (((column + run) < LIFE_GAME_COLUMNS) &&
                   (LifeGame_CellAlive(game,
                                       column + run,
                                       row) != 0U)) {
                run++;
            }
            if (run != 0U) {
                DISPLAY_FillRectangle_DMA(
                    LIFE_GAME_BOARD_X +
                    (column * LIFE_GAME_CELL_SIZE),
                    LIFE_GAME_BOARD_Y +
                    (row * LIFE_GAME_CELL_SIZE),
                    run * LIFE_GAME_CELL_SIZE,
                    LIFE_GAME_CELL_SIZE,
                    LifeGameRender_CellColor(row));
                column += run;
            }
        }
    }
}

static void LifeGameRender_DrawWholeField(
    LifeGameState *game) {
    DISPLAY_FillScreen_DMA(LIFE_RENDER_BACKGROUND);
    LifeGameRender_DrawHeaderStatic();
    LifeGameRender_DrawHudStats(game, 1U);
    LifeGameRender_DrawBoardBackground();
    LifeGameRender_DrawAliveRuns(game);
    LifeGame_ClearDirty(game);
}

static void LifeGameRender_DrawDirtyCells(
    LifeGameState *game) {
    for (uint16_t row = 0U; row < LIFE_GAME_ROWS; row++) {
        uint16_t column = 0U;

        while (column < LIFE_GAME_COLUMNS) {
            uint8_t alive;
            uint16_t run = 0U;

            while ((column < LIFE_GAME_COLUMNS) &&
                   (LifeGame_CellDirty(game,
                                       column,
                                       row) == 0U)) {
                column++;
            }
            if (column >= LIFE_GAME_COLUMNS) {
                break;
            }

            alive = LifeGame_CellAlive(game, column, row);
            while (((column + run) < LIFE_GAME_COLUMNS) &&
                   (LifeGame_CellDirty(game,
                                       column + run,
                                       row) != 0U) &&
                   (LifeGame_CellAlive(game,
                                       column + run,
                                       row) == alive)) {
                run++;
            }

            DISPLAY_FillRectangle_DMA(
                LIFE_GAME_BOARD_X +
                (column * LIFE_GAME_CELL_SIZE),
                LIFE_GAME_BOARD_Y +
                (row * LIFE_GAME_CELL_SIZE),
                run * LIFE_GAME_CELL_SIZE,
                LIFE_GAME_CELL_SIZE,
                (alive != 0U) ?
                LifeGameRender_CellColor(row) :
                LIFE_RENDER_BOARD);
            column += run;
        }
    }

    LifeGame_ClearDirty(game);
}

void LifeGameRender_Init(LifeGameState *game) {
    life_renderer_initialized = 1U;
    displayed_generation = 0xFFFFFFFFU;
    displayed_population = 0xFFFFFFFFU;
    displayed_phase = (LifeGamePhase)0xFFU;
    displayed_shake_energy = 0xFFFFU;
    displayed_mpu_online = 0xFFU;

    if (game->phase == LIFE_GAME_PHASE_WAITING) {
        LifeGameRender_DrawWaitScreen(game);
    } else {
        LifeGameRender_DrawWholeField(game);
    }
}

void LifeGameRender_Frame(LifeGameState *game,
                          LifeGameEvent event) {
    if (life_renderer_initialized == 0U) {
        LifeGameRender_Init(game);
        return;
    }

    if (game->phase == LIFE_GAME_PHASE_WAITING) {
        if ((event & LIFE_GAME_EVENT_WAIT_STATUS) != 0U) {
            LifeGameRender_DrawWaitMeter(game, 0U);
        }
        return;
    }

    if ((event & LIFE_GAME_EVENT_STARTED) != 0U) {
        LifeGameRender_DrawWholeField(game);
        return;
    }

    if ((event & LIFE_GAME_EVENT_STEP) != 0U) {
        LifeGameRender_DrawDirtyCells(game);
    }
    if ((event & LIFE_GAME_EVENT_HUD_CHANGED) != 0U) {
        LifeGameRender_DrawHudStats(game, 0U);
    }
}
