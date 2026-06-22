#include "marble_maze_render.h"

#include <stdio.h>
#include "display_driver.h"
#include "render_scratch.h"

#define MAZE_RENDER_PATCH_SIZE        32
#define MAZE_WALL_COLOR               DISPLAY_COLOR565(95, 150, 180)
#define MAZE_WALL_HIGHLIGHT           DISPLAY_COLOR565(160, 220, 235)
#define MAZE_BALL_COLOR               DISPLAY_ORANGE
#define MAZE_GOAL_COLOR               DISPLAY_GREEN

#if (MAZE_RENDER_PATCH_SIZE * MAZE_RENDER_PATCH_SIZE * 2U) > \
    RENDER_SCRATCH_BUFFER_SIZE
#error "Render scratch buffer is too small for marble maze patches"
#endif

typedef struct {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
} MarbleMazeRect;

static const uint16_t maze_backgrounds[] = {
    DISPLAY_COLOR565(4, 12, 19),
    DISPLAY_COLOR565(15, 7, 20),
    DISPLAY_COLOR565(4, 18, 14),
    DISPLAY_COLOR565(19, 10, 3),
    DISPLAY_COLOR565(5, 7, 24)
};

#define maze_patch_buffer render_scratch_buffer
static int16_t previous_ball_x;
static int16_t previous_ball_y;
static uint8_t maze_renderer_initialized;

static int16_t MarbleMazeRender_Round(float value) {
    return (int16_t)(value + 0.5f);
}

static uint16_t MarbleMazeRender_Background(
    const MarbleMazeState *game) {
    return maze_backgrounds[
        game->theme %
        (sizeof(maze_backgrounds) /
         sizeof(maze_backgrounds[0]))];
}

static uint8_t MarbleMazeRender_PointInCircle(
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

static uint8_t MarbleMazeRender_PointInRect(
    int16_t x,
    int16_t y,
    int16_t rectangle_x,
    int16_t rectangle_y,
    int16_t width,
    int16_t height) {
    return (x >= rectangle_x) &&
           (x < (rectangle_x + width)) &&
           (y >= rectangle_y) &&
           (y < (rectangle_y + height));
}

static uint16_t MarbleMazeRender_StaticPixel(
    const MarbleMazeState *game,
    int16_t x,
    int16_t y) {
    int16_t goal_x = MarbleMaze_CellCenterX(game->goal_cell);
    int16_t goal_y = MarbleMaze_CellCenterY(game->goal_cell);

    if (MarbleMazeRender_PointInCircle(
            x, y, goal_x, goal_y,
            MARBLE_MAZE_GOAL_RADIUS) != 0U) {
        if (MarbleMazeRender_PointInCircle(
                x, y, goal_x, goal_y, 3) != 0U) {
            return DISPLAY_WHITE;
        }
        return MAZE_GOAL_COLOR;
    }

    for (uint8_t row = 0U; row < MARBLE_MAZE_ROWS; row++) {
        for (uint8_t column = 0U;
             column < MARBLE_MAZE_COLUMNS;
             column++) {
            uint8_t cell =
                (row * MARBLE_MAZE_COLUMNS) + column;
            int16_t cell_x =
                MARBLE_MAZE_LEFT +
                (column * MARBLE_MAZE_CELL_SIZE);
            int16_t cell_y =
                MARBLE_MAZE_TOP +
                (row * MARBLE_MAZE_CELL_SIZE);

            if (((game->walls[cell] &
                  MARBLE_MAZE_WALL_NORTH) != 0U) &&
                (MarbleMazeRender_PointInRect(
                    x, y, cell_x, cell_y,
                    MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return (y == cell_y) ?
                       MAZE_WALL_HIGHLIGHT : MAZE_WALL_COLOR;
            }
            if (((game->walls[cell] &
                  MARBLE_MAZE_WALL_WEST) != 0U) &&
                (MarbleMazeRender_PointInRect(
                    x, y, cell_x, cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE) != 0U)) {
                return (x == cell_x) ?
                       MAZE_WALL_HIGHLIGHT : MAZE_WALL_COLOR;
            }
            if ((column + 1U == MARBLE_MAZE_COLUMNS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_EAST) != 0U) &&
                (MarbleMazeRender_PointInRect(
                    x, y,
                    cell_x + MARBLE_MAZE_CELL_SIZE,
                    cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return MAZE_WALL_COLOR;
            }
            if ((row + 1U == MARBLE_MAZE_ROWS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_SOUTH) != 0U) &&
                (MarbleMazeRender_PointInRect(
                    x, y, cell_x,
                    cell_y + MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return MAZE_WALL_COLOR;
            }
        }
    }

    return MarbleMazeRender_Background(game);
}

static uint16_t MarbleMazeRender_ComposedPixel(
    const MarbleMazeState *game,
    int16_t ball_x,
    int16_t ball_y,
    int16_t x,
    int16_t y) {
    if (MarbleMazeRender_PointInCircle(
            x, y, ball_x, ball_y,
            MARBLE_MAZE_BALL_RADIUS) != 0U) {
        if (MarbleMazeRender_PointInCircle(
                x, y, ball_x - 2, ball_y - 2, 2) != 0U) {
            return DISPLAY_WHITE;
        }
        return MAZE_BALL_COLOR;
    }
    return MarbleMazeRender_StaticPixel(game, x, y);
}

static void MarbleMazeRender_ClipRect(MarbleMazeRect *rect) {
    if (rect->x0 < MARBLE_MAZE_LEFT) {
        rect->x0 = MARBLE_MAZE_LEFT;
    }
    if (rect->y0 < MARBLE_MAZE_TOP) {
        rect->y0 = MARBLE_MAZE_TOP;
    }
    if (rect->x1 >= MARBLE_MAZE_SCREEN_WIDTH) {
        rect->x1 = MARBLE_MAZE_SCREEN_WIDTH - 1;
    }
    if (rect->y1 >= MARBLE_MAZE_SCREEN_HEIGHT) {
        rect->y1 = MARBLE_MAZE_SCREEN_HEIGHT - 1;
    }
}

static uint8_t MarbleMazeRender_DrawPatch(
    const MarbleMazeState *game,
    int16_t ball_x,
    int16_t ball_y,
    MarbleMazeRect rect) {
    uint16_t width;
    uint16_t height;
    uint32_t offset = 0U;

    MarbleMazeRender_ClipRect(&rect);
    width = (uint16_t)(rect.x1 - rect.x0 + 1);
    height = (uint16_t)(rect.y1 - rect.y0 + 1);

    if ((width > MAZE_RENDER_PATCH_SIZE) ||
        (height > MAZE_RENDER_PATCH_SIZE)) {
        return 0U;
    }

    for (int16_t y = rect.y0; y <= rect.y1; y++) {
        for (int16_t x = rect.x0; x <= rect.x1; x++) {
            uint16_t color =
                MarbleMazeRender_ComposedPixel(
                    game, ball_x, ball_y, x, y);

            maze_patch_buffer[offset++] = (uint8_t)(color >> 8);
            maze_patch_buffer[offset++] = (uint8_t)(color & 0xFFU);
        }
    }

    DISPLAY_DrawImage_DMA_1D(
        (uint16_t)rect.x0,
        (uint16_t)rect.y0,
        width,
        height,
        maze_patch_buffer);
    return 1U;
}

static void MarbleMazeRender_DrawHud(
    const MarbleMazeState *game) {
    char text[20];

    DISPLAY_FillRectangle_DMA(
        0, 0, MARBLE_MAZE_SCREEN_WIDTH,
        MARBLE_MAZE_PLAYFIELD_TOP, DISPLAY_BLACK);
    DISPLAY_FillRectangle_DMA(
        0, MARBLE_MAZE_PLAYFIELD_TOP - 1,
        MARBLE_MAZE_SCREEN_WIDTH, 1, DISPLAY_GRAY);
    DISPLAY_WriteString_DMA(
        4, 4, "MARBLE MAZE", Font_11x18,
        DISPLAY_WHITE, DISPLAY_BLACK);
    snprintf(text, sizeof(text), "LEVEL %u",
             (unsigned int)game->level);
    DISPLAY_WriteString_DMA(
        154, 4, text, Font_11x18,
        DISPLAY_CYAN, DISPLAY_BLACK);
    DISPLAY_WriteString_DMA(
        264, 9, "GOAL", Font_7x10,
        DISPLAY_GREEN, DISPLAY_BLACK);
}

static void MarbleMazeRender_DrawWalls(
    const MarbleMazeState *game) {
    for (uint8_t row = 0U; row < MARBLE_MAZE_ROWS; row++) {
        for (uint8_t column = 0U;
             column < MARBLE_MAZE_COLUMNS;
             column++) {
            uint8_t cell =
                (row * MARBLE_MAZE_COLUMNS) + column;
            uint16_t cell_x =
                MARBLE_MAZE_LEFT +
                (column * MARBLE_MAZE_CELL_SIZE);
            uint16_t cell_y =
                MARBLE_MAZE_TOP +
                (row * MARBLE_MAZE_CELL_SIZE);

            if ((game->walls[cell] &
                 MARBLE_MAZE_WALL_NORTH) != 0U) {
                DISPLAY_FillRectangle_DMA(
                    cell_x, cell_y,
                    MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MAZE_WALL_COLOR);
                DISPLAY_FillRectangle_DMA(
                    cell_x, cell_y,
                    MARBLE_MAZE_CELL_SIZE,
                    1, MAZE_WALL_HIGHLIGHT);
            }
            if ((game->walls[cell] &
                 MARBLE_MAZE_WALL_WEST) != 0U) {
                DISPLAY_FillRectangle_DMA(
                    cell_x, cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE,
                    MAZE_WALL_COLOR);
                DISPLAY_FillRectangle_DMA(
                    cell_x, cell_y, 1,
                    MARBLE_MAZE_CELL_SIZE,
                    MAZE_WALL_HIGHLIGHT);
            }
            if ((column + 1U == MARBLE_MAZE_COLUMNS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_EAST) != 0U)) {
                DISPLAY_FillRectangle_DMA(
                    cell_x + MARBLE_MAZE_CELL_SIZE,
                    cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS,
                    MAZE_WALL_COLOR);
            }
            if ((row + 1U == MARBLE_MAZE_ROWS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_SOUTH) != 0U)) {
                DISPLAY_FillRectangle_DMA(
                    cell_x,
                    cell_y + MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MAZE_WALL_COLOR);
            }
        }
    }
}

static void MarbleMazeRender_DrawWholeLevel(
    const MarbleMazeState *game) {
    int16_t ball_x = MarbleMazeRender_Round(game->ball.x);
    int16_t ball_y = MarbleMazeRender_Round(game->ball.y);
    int16_t goal_x = MarbleMaze_CellCenterX(game->goal_cell);
    int16_t goal_y = MarbleMaze_CellCenterY(game->goal_cell);
    MarbleMazeRect ball_rect = {
        ball_x - MARBLE_MAZE_BALL_RADIUS,
        ball_y - MARBLE_MAZE_BALL_RADIUS,
        ball_x + MARBLE_MAZE_BALL_RADIUS,
        ball_y + MARBLE_MAZE_BALL_RADIUS
    };

    DISPLAY_FillScreen_DMA(
        MarbleMazeRender_Background(game));
    MarbleMazeRender_DrawWalls(game);
    DISPLAY_FillCircle_DMA(
        (uint16_t)goal_x, (uint16_t)goal_y,
        MARBLE_MAZE_GOAL_RADIUS, MAZE_GOAL_COLOR);
    DISPLAY_FillCircle_DMA(
        (uint16_t)goal_x, (uint16_t)goal_y,
        3, DISPLAY_WHITE);
    MarbleMazeRender_DrawHud(game);
    MarbleMazeRender_DrawPatch(
        game, ball_x, ball_y, ball_rect);

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
    maze_renderer_initialized = 1U;
}

void MarbleMazeRender_Init(const MarbleMazeState *game) {
    maze_renderer_initialized = 0U;
    MarbleMazeRender_DrawWholeLevel(game);
}

void MarbleMazeRender_Frame(const MarbleMazeState *game,
                            MarbleMazeEvent event) {
    int16_t ball_x;
    int16_t ball_y;
    MarbleMazeRect dirty;

    if ((maze_renderer_initialized == 0U) ||
        ((event & MARBLE_MAZE_EVENT_LEVEL_STARTED) != 0U)) {
        MarbleMazeRender_DrawWholeLevel(game);
        return;
    }

    ball_x = MarbleMazeRender_Round(game->ball.x);
    ball_y = MarbleMazeRender_Round(game->ball.y);
    if ((ball_x == previous_ball_x) &&
        (ball_y == previous_ball_y)) {
        return;
    }

    dirty.x0 =
        ((ball_x < previous_ball_x) ?
         ball_x : previous_ball_x) -
        MARBLE_MAZE_BALL_RADIUS - 1;
    dirty.x1 =
        ((ball_x > previous_ball_x) ?
         ball_x : previous_ball_x) +
        MARBLE_MAZE_BALL_RADIUS + 1;
    dirty.y0 =
        ((ball_y < previous_ball_y) ?
         ball_y : previous_ball_y) -
        MARBLE_MAZE_BALL_RADIUS - 1;
    dirty.y1 =
        ((ball_y > previous_ball_y) ?
         ball_y : previous_ball_y) +
        MARBLE_MAZE_BALL_RADIUS + 1;

    if (MarbleMazeRender_DrawPatch(
            game, ball_x, ball_y, dirty) == 0U) {
        MarbleMazeRender_DrawWholeLevel(game);
        return;
    }

    previous_ball_x = ball_x;
    previous_ball_y = ball_y;
}
