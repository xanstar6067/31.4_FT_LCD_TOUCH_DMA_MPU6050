#include "tetris_render.h"

#include <stdio.h>

#include "button_renderer.h"
#include "display_driver.h"

#define TETRIS_RENDER_BACKGROUND      DISPLAY_COLOR565(8, 11, 15)
#define TETRIS_RENDER_PANEL           DISPLAY_COLOR565(18, 25, 31)
#define TETRIS_RENDER_BOARD           DISPLAY_COLOR565(2, 4, 7)
#define TETRIS_RENDER_GRID            DISPLAY_COLOR565(22, 29, 35)
#define TETRIS_RENDER_BORDER          DISPLAY_COLOR565(83, 104, 118)
#define TETRIS_RENDER_TEXT_DIM        DISPLAY_COLOR565(150, 166, 174)
#define TETRIS_RENDER_OVERLAY         DISPLAY_COLOR565(28, 34, 42)
#define TETRIS_PREVIEW_CELL           8U

static uint8_t tetris_renderer_initialized;
static uint16_t tetris_rendered_cells[TETRIS_BOARD_ROWS]
                                    [TETRIS_BOARD_COLUMNS];

static uint16_t TetrisRender_CellX(uint8_t column) {
    return (uint16_t)(TETRIS_BOARD_X +
                      (column * TETRIS_CELL_SIZE));
}

static uint16_t TetrisRender_CellY(uint8_t row) {
    return (uint16_t)(TETRIS_BOARD_Y +
                      (row * TETRIS_CELL_SIZE));
}

static void TetrisRender_DrawCell(uint16_t x,
                                  uint16_t y,
                                  uint16_t color) {
    DISPLAY_FillRectangle_DMA(x,
                              y,
                              TETRIS_CELL_SIZE,
                              TETRIS_CELL_SIZE,
                              TETRIS_RENDER_GRID);
    if (color != 0U) {
        DISPLAY_FillRectangle_DMA((uint16_t)(x + 1U),
                                  (uint16_t)(y + 1U),
                                  (uint16_t)(TETRIS_CELL_SIZE - 2U),
                                  (uint16_t)(TETRIS_CELL_SIZE - 2U),
                                  color);
        DISPLAY_FillRectangle_DMA((uint16_t)(x + 2U),
                                  (uint16_t)(y + 2U),
                                  (uint16_t)(TETRIS_CELL_SIZE - 4U),
                                  1U,
                                  DISPLAY_WHITE);
    } else {
        DISPLAY_FillRectangle_DMA((uint16_t)(x + 1U),
                                  (uint16_t)(y + 1U),
                                  (uint16_t)(TETRIS_CELL_SIZE - 2U),
                                  (uint16_t)(TETRIS_CELL_SIZE - 2U),
                                  TETRIS_RENDER_BOARD);
    }
}

static void TetrisRender_DrawBoard(TetrisState *game) {
    DISPLAY_FillRectangle_DMA((uint16_t)(TETRIS_BOARD_X - 2U),
                              (uint16_t)(TETRIS_BOARD_Y - 2U),
                              (uint16_t)(TETRIS_BOARD_WIDTH + 4U),
                              (uint16_t)(TETRIS_BOARD_HEIGHT + 4U),
                              TETRIS_RENDER_BORDER);
    DISPLAY_FillRectangle_DMA(TETRIS_BOARD_X,
                              TETRIS_BOARD_Y,
                              TETRIS_BOARD_WIDTH,
                              TETRIS_BOARD_HEIGHT,
                              TETRIS_RENDER_BOARD);

    for (uint8_t row = 0U; row < TETRIS_BOARD_ROWS; row++) {
        for (uint8_t column = 0U;
             column < TETRIS_BOARD_COLUMNS;
             column++) {
            uint16_t color =
                Tetris_CellColorAt(game, column, row);

            TetrisRender_DrawCell(
                TetrisRender_CellX(column),
                TetrisRender_CellY(row),
                color);
            tetris_rendered_cells[row][column] = color;
        }
    }
}

static void TetrisRender_DrawDirtyBoard(TetrisState *game) {
    for (uint8_t row = 0U; row < TETRIS_BOARD_ROWS; row++) {
        for (uint8_t column = 0U;
             column < TETRIS_BOARD_COLUMNS;
             column++) {
            uint16_t color =
                Tetris_CellColorAt(game, column, row);

            if (tetris_rendered_cells[row][column] == color) {
                continue;
            }
            TetrisRender_DrawCell(TetrisRender_CellX(column),
                                  TetrisRender_CellY(row),
                                  color);
            tetris_rendered_cells[row][column] = color;
        }
    }
}

static void TetrisRender_DrawPreviewBox(uint16_t x,
                                        uint16_t y,
                                        const char *label) {
    DISPLAY_FillRectangle_DMA(x,
                              y,
                              50U,
                              56U,
                              TETRIS_RENDER_PANEL);
    DISPLAY_WriteString_DMA((uint16_t)(x + 5U),
                            (uint16_t)(y + 4U),
                            label,
                            Font_7x10,
                            TETRIS_RENDER_TEXT_DIM,
                            TETRIS_RENDER_PANEL);
}

static void TetrisRender_DrawPiecePreview(const TetrisPiece *piece,
                                          uint16_t x,
                                          uint16_t y,
                                          uint8_t enabled) {
    TetrisPiece preview;

    DISPLAY_FillRectangle_DMA((uint16_t)(x + 5U),
                              (uint16_t)(y + 17U),
                              40U,
                              32U,
                              TETRIS_RENDER_PANEL);
    if (enabled == 0U) {
        return;
    }

    preview = *piece;
    preview.x = 0;
    preview.y = 0;
    preview.rotation = 0U;
    for (uint8_t i = 0U; i < 4U; i++) {
        int8_t block_x;
        int8_t block_y;

        (void)Tetris_PieceBlock(&preview, i, &block_x, &block_y);
        DISPLAY_FillRectangle_DMA(
            (uint16_t)(x + 9U +
                       ((uint8_t)block_x * TETRIS_PREVIEW_CELL)),
            (uint16_t)(y + 18U +
                       ((uint8_t)block_y * TETRIS_PREVIEW_CELL)),
            (uint16_t)(TETRIS_PREVIEW_CELL - 1U),
            (uint16_t)(TETRIS_PREVIEW_CELL - 1U),
            piece->color);
    }
}

static void TetrisRender_DrawHud(TetrisState *game) {
    char text[24];

    DISPLAY_FillRectangle_DMA(0U,
                              0U,
                              TETRIS_SCREEN_WIDTH,
                              22U,
                              DISPLAY_BLACK);
    DISPLAY_WriteString_DMA(8U,
                            3U,
                            "TETRIS",
                            Font_11x18,
                            DISPLAY_WHITE,
                            DISPLAY_BLACK);

    snprintf(text, sizeof(text), "S:%lu",
             (unsigned long)game->score);
    DISPLAY_WriteString_DMA(104U,
                            6U,
                            text,
                            Font_7x10,
                            DISPLAY_CYAN,
                            DISPLAY_BLACK);

    snprintf(text, sizeof(text), "L:%u  R:%u",
             (unsigned int)game->level,
             (unsigned int)game->lines);
    DISPLAY_WriteString_DMA(208U,
                            6U,
                            text,
                            Font_7x10,
                            DISPLAY_YELLOW,
                            DISPLAY_BLACK);

    TetrisRender_DrawPreviewBox(28U, 42U, "NEXT");
    TetrisRender_DrawPiecePreview(&game->next, 28U, 42U, 1U);
    TetrisRender_DrawPreviewBox(242U, 42U, "HOLD");
    TetrisRender_DrawPiecePreview(&game->hold,
                                  242U,
                                  42U,
                                  game->hold_valid);
}

static void TetrisRender_DrawGameOver(TetrisState *game) {
    DISPLAY_FillRectangle_DMA(88U,
                              78U,
                              144U,
                              82U,
                              TETRIS_RENDER_OVERLAY);
    DISPLAY_WriteString_DMA(105U,
                            86U,
                            "GAME OVER",
                            Font_11x18,
                            DISPLAY_WHITE,
                            TETRIS_RENDER_OVERLAY);
    ButtonRenderer_Draw(
        VirtualButtonList_Find(&game->buttons,
                               TETRIS_BUTTON_RESTART));
}

static void TetrisRender_DrawControls(TetrisState *game) {
    ButtonRenderer_DrawList(&game->buttons);
}

static void TetrisRender_DrawHint(void) {
    DISPLAY_FillRectangle_DMA(96U,
                              228U,
                              128U,
                              11U,
                              TETRIS_RENDER_BACKGROUND);
    DISPLAY_WriteString_DMA(100U,
                            229U,
                            "TAP BOARD: ROTATE",
                            Font_7x10,
                            TETRIS_RENDER_TEXT_DIM,
                            TETRIS_RENDER_BACKGROUND);
}

static void TetrisRender_DrawWhole(TetrisState *game) {
    DISPLAY_FillScreen_DMA(TETRIS_RENDER_BACKGROUND);
    TetrisRender_DrawHud(game);
    TetrisRender_DrawBoard(game);
    TetrisRender_DrawControls(game);
    TetrisRender_DrawHint();
    if (game->phase == TETRIS_PHASE_GAME_OVER) {
        TetrisRender_DrawGameOver(game);
    }
    tetris_renderer_initialized = 1U;
}

void TetrisRender_Init(TetrisState *game) {
    tetris_renderer_initialized = 0U;
    TetrisRender_DrawWhole(game);
}

void TetrisRender_Frame(TetrisState *game,
                        TetrisEvent event) {
    if ((tetris_renderer_initialized == 0U) ||
        ((event & TETRIS_EVENT_REDRAW) != 0U)) {
        TetrisRender_DrawWhole(game);
        return;
    }

    if ((event & TETRIS_EVENT_BOARD_CHANGED) != 0U) {
        TetrisRender_DrawDirtyBoard(game);
    }
    if ((event & TETRIS_EVENT_HUD_CHANGED) != 0U) {
        TetrisRender_DrawHud(game);
    }
    if ((event & TETRIS_EVENT_BUTTONS_CHANGED) != 0U) {
        TetrisRender_DrawControls(game);
    }
    if ((event & TETRIS_EVENT_GAME_OVER) != 0U) {
        TetrisRender_DrawGameOver(game);
    }
}
