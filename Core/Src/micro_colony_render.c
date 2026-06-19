#include "micro_colony_render.h"

#include <stdio.h>
#include "display_driver.h"

#define MICRO_RENDER_BACKGROUND     ILI9341_COLOR565(2, 5, 7)
#define MICRO_RENDER_BOARD_EMPTY    ILI9341_COLOR565(0, 2, 2)
#define MICRO_RENDER_FOOD_1         ILI9341_COLOR565(4, 30, 9)
#define MICRO_RENDER_FOOD_2         ILI9341_COLOR565(12, 60, 18)
#define MICRO_RENDER_FOOD_3         ILI9341_COLOR565(35, 105, 32)
#define MICRO_RENDER_GRID           ILI9341_COLOR565(5, 14, 16)
#define MICRO_RENDER_HEADER         ILI9341_COLOR565(3, 10, 15)

static uint8_t micro_renderer_initialized;
static uint16_t displayed_population;
static uint16_t displayed_food;
static uint32_t displayed_births;
static uint32_t displayed_deaths;
static uint32_t displayed_extinctions;

static uint8_t MicroColonyRender_Scale8(uint8_t value,
                                        uint8_t amount) {
    return (uint8_t)(((uint16_t)value * amount) / 255U);
}

static uint16_t MicroColonyRender_HueColor(
    const MicroColonyBot *bot) {
    uint8_t hue = bot->hue;
    uint8_t region = hue / 43U;
    uint8_t remainder = (uint8_t)((hue - (region * 43U)) * 6U);
    uint8_t value =
        (uint8_t)(85U +
                  (((uint32_t)bot->energy * 155U) /
                   MICRO_COLONY_MAX_ENERGY));
    uint8_t base =
        (uint8_t)(18U + (bot->mutation > 80U ?
                         80U : bot->mutation) / 3U);
    uint8_t rising = MicroColonyRender_Scale8(value, remainder);
    uint8_t falling =
        MicroColonyRender_Scale8(value,
                                 (uint8_t)(255U - remainder));
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if ((bot->max_age != 0U) &&
        (bot->age > ((bot->max_age * 3U) / 4U))) {
        value = MicroColonyRender_Scale8(value, 190U);
        rising = MicroColonyRender_Scale8(rising, 190U);
        falling = MicroColonyRender_Scale8(falling, 190U);
    }

    switch (region) {
        case 0U:
            r = value;
            g = rising;
            b = base;
            break;
        case 1U:
            r = falling;
            g = value;
            b = base;
            break;
        case 2U:
            r = base;
            g = value;
            b = rising;
            break;
        case 3U:
            r = base;
            g = falling;
            b = value;
            break;
        case 4U:
            r = rising;
            g = base;
            b = value;
            break;
        default:
            r = value;
            g = base;
            b = falling;
            break;
    }

    return ILI9341_COLOR565(r, g, b);
}

static uint16_t MicroColonyRender_FoodColor(uint8_t food) {
    switch (food) {
        case 1U:
            return MICRO_RENDER_FOOD_1;
        case 2U:
            return MICRO_RENDER_FOOD_2;
        case 3U:
            return MICRO_RENDER_FOOD_3;
        default:
            return MICRO_RENDER_BOARD_EMPTY;
    }
}

static uint16_t MicroColonyRender_CellColor(
    const MicroColonyState *game,
    uint8_t column,
    uint8_t row) {
    const MicroColonyBot *bot =
        MicroColony_BotAt(game, column, row);

    if (bot != 0) {
        return MicroColonyRender_HueColor(bot);
    }
    return MicroColonyRender_FoodColor(
        MicroColony_FoodAt(game, column, row));
}

static void MicroColonyRender_FormatCounter(char *text,
                                            uint8_t size,
                                            uint32_t value) {
    if (value < 100000UL) {
        snprintf(text, size, "%lu", (unsigned long)value);
    } else if (value < 10000000UL) {
        snprintf(text, size, "%luK",
                 (unsigned long)(value / 1000UL));
    } else {
        snprintf(text, size, "%luM",
                 (unsigned long)(value / 1000000UL));
    }
}

static void MicroColonyRender_DrawCounter(uint16_t x,
                                          uint16_t y,
                                          uint16_t width,
                                          uint32_t value,
                                          uint16_t color) {
    char text[8];

    MicroColonyRender_FormatCounter(
        text, sizeof(text), value);
    ILI9341_FillRectangle_DMA(x, y, width, 10,
                              MICRO_RENDER_HEADER);
    ILI9341_WriteString_DMA(x, y, text, Font_7x10,
                            color, MICRO_RENDER_HEADER);
}

static void MicroColonyRender_DrawHeaderStatic(void) {
    ILI9341_FillRectangle_DMA(
        0, 0, MICRO_COLONY_SCREEN_WIDTH,
        MICRO_COLONY_HEADER_HEIGHT, MICRO_RENDER_HEADER);
    ILI9341_FillRectangle_DMA(
        0, MICRO_COLONY_HEADER_HEIGHT - 1U,
        MICRO_COLONY_SCREEN_WIDTH, 1, ILI9341_GRAY);
    ILI9341_WriteString_DMA(
        4, 4, "MICRO COLONY", Font_11x18,
        ILI9341_WHITE, MICRO_RENDER_HEADER);

    ILI9341_WriteString_DMA(
        144, 4, "P:", Font_7x10,
        ILI9341_CYAN, MICRO_RENDER_HEADER);
    ILI9341_WriteString_DMA(
        205, 4, "F:", Font_7x10,
        ILI9341_CYAN, MICRO_RENDER_HEADER);
    ILI9341_WriteString_DMA(
        144, 17, "B:", Font_7x10,
        ILI9341_GREEN, MICRO_RENDER_HEADER);
    ILI9341_WriteString_DMA(
        205, 17, "D:", Font_7x10,
        ILI9341_GREEN, MICRO_RENDER_HEADER);
    ILI9341_WriteString_DMA(
        266, 17, "E:", Font_7x10,
        ILI9341_GREEN, MICRO_RENDER_HEADER);
}

static void MicroColonyRender_DrawHeaderStats(
    const MicroColonyState *game,
    uint8_t force) {
    if ((force == 0U) &&
        (displayed_population == game->population) &&
        (displayed_food == game->food_total) &&
        (displayed_births == game->births) &&
        (displayed_deaths == game->deaths) &&
        (displayed_extinctions == game->extinctions)) {
        return;
    }

    if ((force != 0U) ||
        (displayed_population != game->population)) {
        MicroColonyRender_DrawCounter(
            158, 4, 38, game->population, ILI9341_CYAN);
        displayed_population = game->population;
    }
    if ((force != 0U) ||
        (displayed_food != game->food_total)) {
        MicroColonyRender_DrawCounter(
            219, 4, 42, game->food_total, ILI9341_CYAN);
        displayed_food = game->food_total;
    }
    if ((force != 0U) ||
        (displayed_births != game->births)) {
        MicroColonyRender_DrawCounter(
            158, 17, 42, game->births, ILI9341_GREEN);
        displayed_births = game->births;
    }
    if ((force != 0U) ||
        (displayed_deaths != game->deaths)) {
        MicroColonyRender_DrawCounter(
            219, 17, 42, game->deaths, ILI9341_GREEN);
        displayed_deaths = game->deaths;
    }
    if ((force != 0U) ||
        (displayed_extinctions != game->extinctions)) {
        MicroColonyRender_DrawCounter(
            280, 17, 38, game->extinctions, ILI9341_GREEN);
        displayed_extinctions = game->extinctions;
    }
}

static void MicroColonyRender_DrawGrid(void) {
    for (uint8_t row = 0U; row <= MICRO_COLONY_ROWS; row += 10U) {
        ILI9341_FillRectangle_DMA(
            MICRO_COLONY_BOARD_X,
            MICRO_COLONY_BOARD_Y +
            (row * MICRO_COLONY_CELL_SIZE),
            MICRO_COLONY_SCREEN_WIDTH, 1,
            MICRO_RENDER_GRID);
    }
}

static void MicroColonyRender_DrawCells(
    MicroColonyState *game,
    uint8_t dirty_only) {
    for (uint8_t row = 0U; row < MICRO_COLONY_ROWS; row++) {
        uint8_t column = 0U;

        while (column < MICRO_COLONY_COLUMNS) {
            uint16_t color;
            uint8_t run = 0U;

            if (dirty_only != 0U) {
                while ((column < MICRO_COLONY_COLUMNS) &&
                       (MicroColony_CellDirty(
                            game, column, row) == 0U)) {
                    column++;
                }
                if (column >= MICRO_COLONY_COLUMNS) {
                    break;
                }
            }

            color = MicroColonyRender_CellColor(
                game, column, row);
            while (((column + run) < MICRO_COLONY_COLUMNS) &&
                   ((dirty_only == 0U) ||
                    (MicroColony_CellDirty(
                         game, column + run, row) != 0U)) &&
                   (MicroColonyRender_CellColor(
                        game, column + run, row) == color)) {
                run++;
            }

            ILI9341_FillRectangle_DMA(
                MICRO_COLONY_BOARD_X +
                (column * MICRO_COLONY_CELL_SIZE),
                MICRO_COLONY_BOARD_Y +
                (row * MICRO_COLONY_CELL_SIZE),
                run * MICRO_COLONY_CELL_SIZE,
                MICRO_COLONY_CELL_SIZE,
                color);
            column = (uint8_t)(column + run);
        }
    }

    if (dirty_only == 0U) {
        MicroColonyRender_DrawGrid();
    }
    MicroColony_ClearDirty(game);
}

static void MicroColonyRender_DrawWhole(
    MicroColonyState *game) {
    ILI9341_FillScreen_DMA(MICRO_RENDER_BACKGROUND);
    MicroColonyRender_DrawHeaderStatic();
    MicroColonyRender_DrawHeaderStats(game, 1U);
    MicroColonyRender_DrawCells(game, 0U);
}

void MicroColonyRender_Init(MicroColonyState *game) {
    micro_renderer_initialized = 1U;
    displayed_population = 0xFFFFU;
    displayed_food = 0xFFFFU;
    displayed_births = 0xFFFFFFFFU;
    displayed_deaths = 0xFFFFFFFFU;
    displayed_extinctions = 0xFFFFFFFFU;
    MicroColonyRender_DrawWhole(game);
}

void MicroColonyRender_Frame(MicroColonyState *game,
                             MicroColonyEvent event) {
    if ((micro_renderer_initialized == 0U) ||
        ((event & MICRO_COLONY_EVENT_FULL_REDRAW) != 0U)) {
        MicroColonyRender_Init(game);
        return;
    }

    if ((event & MICRO_COLONY_EVENT_CELLS) != 0U) {
        MicroColonyRender_DrawCells(game, 1U);
    }
    if ((event & MICRO_COLONY_EVENT_HUD) != 0U) {
        MicroColonyRender_DrawHeaderStats(game, 0U);
    }
}
