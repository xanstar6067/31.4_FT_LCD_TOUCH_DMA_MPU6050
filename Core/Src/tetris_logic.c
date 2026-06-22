#include "tetris_logic.h"

#include <string.h>

#include "display_driver.h"

#define TETRIS_REPEAT_DELAY_TICKS      6U
#define TETRIS_REPEAT_PERIOD_TICKS     2U
#define TETRIS_SOFT_DROP_DELAY_TICKS   0U
#define TETRIS_SOFT_DROP_PERIOD_TICKS  1U

#define TETRIS_BUTTON_W                58U
#define TETRIS_BUTTON_H                40U
#define TETRIS_BUTTON_MARGIN_X         24U
#define TETRIS_BUTTON_TOP_Y            138U
#define TETRIS_BUTTON_BOTTOM_Y         188U
#define TETRIS_BUTTON_RIGHT_X          (TETRIS_SCREEN_WIDTH - \
                                        TETRIS_BUTTON_MARGIN_X - \
                                        TETRIS_BUTTON_W)

static const int8_t tetris_shapes[7][4][4][2] = {
    {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}}
    },
    {
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}}
    },
    {
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {1, 2}}
    },
    {
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}}
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}}
    },
    {
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {0, 2}, {1, 2}}
    },
    {
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}}
    }
};

static const uint16_t tetris_colors[] = {
    DISPLAY_COLOR565(64, 224, 236),
    DISPLAY_COLOR565(245, 214, 76),
    DISPLAY_COLOR565(180, 104, 238),
    DISPLAY_COLOR565(80, 220, 124),
    DISPLAY_COLOR565(240, 82, 86),
    DISPLAY_COLOR565(76, 128, 246),
    DISPLAY_COLOR565(247, 152, 64),
    DISPLAY_COLOR565(250, 116, 186),
    DISPLAY_COLOR565(132, 226, 72),
    DISPLAY_COLOR565(94, 198, 255)
};

static uint32_t Tetris_Random(TetrisState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xB5297A4DU;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static uint16_t Tetris_RandomColor(TetrisState *game) {
    uint32_t index =
        Tetris_Random(game) %
        (sizeof(tetris_colors) / sizeof(tetris_colors[0]));

    return tetris_colors[index];
}

uint8_t Tetris_PieceBlock(const TetrisPiece *piece,
                          uint8_t block_index,
                          int8_t *x,
                          int8_t *y) {
    if ((piece == NULL) ||
        (piece->type >= 7U) ||
        (piece->rotation >= 4U) ||
        (block_index >= 4U)) {
        return 0U;
    }

    *x = (int8_t)(piece->x +
                  tetris_shapes[piece->type]
                               [piece->rotation]
                               [block_index][0]);
    *y = (int8_t)(piece->y +
                  tetris_shapes[piece->type]
                               [piece->rotation]
                               [block_index][1]);
    return 1U;
}

static TetrisPiece Tetris_CreatePiece(TetrisState *game) {
    TetrisPiece piece;

    piece.type = (uint8_t)(Tetris_Random(game) % 7U);
    piece.rotation = 0U;
    piece.x = 3;
    piece.y = -2;
    piece.color = Tetris_RandomColor(game);
    return piece;
}

static uint8_t Tetris_Collides(const TetrisState *game,
                               const TetrisPiece *piece) {
    for (uint8_t i = 0U; i < 4U; i++) {
        int8_t x;
        int8_t y;

        (void)Tetris_PieceBlock(piece, i, &x, &y);
        if ((x < 0) ||
            (x >= (int8_t)TETRIS_BOARD_COLUMNS) ||
            (y >= (int8_t)TETRIS_BOARD_ROWS)) {
            return 1U;
        }
        if ((y >= 0) &&
            (game->board[(uint8_t)y][(uint8_t)x] != 0U)) {
            return 1U;
        }
    }
    return 0U;
}

static uint8_t Tetris_MovePiece(TetrisState *game,
                                int8_t dx,
                                int8_t dy) {
    TetrisPiece moved = game->current;

    moved.x = (int8_t)(moved.x + dx);
    moved.y = (int8_t)(moved.y + dy);
    if (Tetris_Collides(game, &moved) != 0U) {
        return 0U;
    }

    game->current = moved;
    return 1U;
}

static uint8_t Tetris_RotatePiece(TetrisState *game) {
    static const int8_t kicks[] = {0, -1, 1, -2, 2};
    TetrisPiece rotated = game->current;

    rotated.rotation = (uint8_t)((rotated.rotation + 1U) & 3U);
    for (uint8_t i = 0U; i < sizeof(kicks); i++) {
        TetrisPiece kicked = rotated;

        kicked.x = (int8_t)(kicked.x + kicks[i]);
        if (Tetris_Collides(game, &kicked) == 0U) {
            game->current = kicked;
            return 1U;
        }
    }
    return 0U;
}

static uint8_t Tetris_FallInterval(const TetrisState *game) {
    int16_t interval = 25 - ((int16_t)game->level * 2);

    if (interval < 6) {
        interval = 6;
    }
    return (uint8_t)interval;
}

static uint8_t Tetris_ClearLines(TetrisState *game) {
    uint8_t cleared = 0U;

    for (int8_t row = (int8_t)TETRIS_BOARD_ROWS - 1;
         row >= 0;
         row--) {
        uint8_t full = 1U;

        for (uint8_t column = 0U;
             column < TETRIS_BOARD_COLUMNS;
             column++) {
            if (game->board[(uint8_t)row][column] == 0U) {
                full = 0U;
                break;
            }
        }

        if (full == 0U) {
            continue;
        }

        for (int8_t copy_row = row; copy_row > 0; copy_row--) {
            memcpy(game->board[(uint8_t)copy_row],
                   game->board[(uint8_t)(copy_row - 1)],
                   sizeof(game->board[0]));
        }
        memset(game->board[0], 0, sizeof(game->board[0]));
        cleared++;
        row++;
    }

    return cleared;
}

static void Tetris_SetGameOver(TetrisState *game) {
    game->phase = TETRIS_PHASE_GAME_OVER;
    VirtualButtonList_SetEnabled(&game->buttons,
                                 TETRIS_BUTTON_RESTART,
                                 1U);
    VirtualButtonList_SetVisible(&game->buttons,
                                 TETRIS_BUTTON_RESTART,
                                 1U);
}

static TetrisEvent Tetris_SpawnNext(TetrisState *game) {
    game->current = game->next;
    game->current.x = 3;
    game->current.y = -2;
    game->current.rotation = 0U;
    game->next = Tetris_CreatePiece(game);
    game->hold_used = 0U;
    game->fall_ticks = 0U;

    if (Tetris_Collides(game, &game->current) != 0U) {
        Tetris_SetGameOver(game);
        return TETRIS_EVENT_GAME_OVER |
               TETRIS_EVENT_BOARD_CHANGED |
               TETRIS_EVENT_HUD_CHANGED |
               TETRIS_EVENT_BUTTONS_CHANGED;
    }

    return TETRIS_EVENT_BOARD_CHANGED |
           TETRIS_EVENT_HUD_CHANGED;
}

static TetrisEvent Tetris_LockPiece(TetrisState *game) {
    uint8_t cleared;
    static const uint16_t line_score[] = {
        0U, 100U, 300U, 500U, 800U
    };

    for (uint8_t i = 0U; i < 4U; i++) {
        int8_t x;
        int8_t y;

        (void)Tetris_PieceBlock(&game->current, i, &x, &y);
        if (y < 0) {
            Tetris_SetGameOver(game);
            return TETRIS_EVENT_GAME_OVER |
                   TETRIS_EVENT_BOARD_CHANGED |
                   TETRIS_EVENT_HUD_CHANGED |
                   TETRIS_EVENT_BUTTONS_CHANGED;
        }
        if ((x >= 0) &&
            (x < (int8_t)TETRIS_BOARD_COLUMNS) &&
            (y >= 0) &&
            (y < (int8_t)TETRIS_BOARD_ROWS)) {
            game->board[(uint8_t)y][(uint8_t)x] =
                game->current.color;
        }
    }

    cleared = Tetris_ClearLines(game);
    if (cleared > 0U) {
        game->lines = (uint16_t)(game->lines + cleared);
        game->level = (uint8_t)(1U + (game->lines / 10U));
        if (game->level > 12U) {
            game->level = 12U;
        }
        game->score +=
            (uint32_t)line_score[cleared] *
            (uint32_t)game->level;
    } else {
        game->score += 5U;
    }

    return Tetris_SpawnNext(game) |
           TETRIS_EVENT_BOARD_CHANGED |
           TETRIS_EVENT_HUD_CHANGED;
}

static TetrisEvent Tetris_DropOne(TetrisState *game) {
    if (Tetris_MovePiece(game, 0, 1) != 0U) {
        return TETRIS_EVENT_BOARD_CHANGED;
    }
    return Tetris_LockPiece(game);
}

static TetrisEvent Tetris_HoldPiece(TetrisState *game) {
    TetrisPiece old_hold;

    if ((game->hold_used != 0U) ||
        (game->phase != TETRIS_PHASE_PLAYING)) {
        return TETRIS_EVENT_NONE;
    }

    game->hold_used = 1U;
    if (game->hold_valid == 0U) {
        game->hold = game->current;
        game->hold.rotation = 0U;
        game->hold.x = 3;
        game->hold.y = -2;
        game->hold_valid = 1U;
        return Tetris_SpawnNext(game) |
               TETRIS_EVENT_HUD_CHANGED;
    }

    old_hold = game->hold;
    game->hold = game->current;
    game->hold.rotation = 0U;
    game->hold.x = 3;
    game->hold.y = -2;
    game->current = old_hold;
    game->current.rotation = 0U;
    game->current.x = 3;
    game->current.y = -2;
    game->fall_ticks = 0U;
    if (Tetris_Collides(game, &game->current) != 0U) {
        Tetris_SetGameOver(game);
        return TETRIS_EVENT_GAME_OVER |
               TETRIS_EVENT_BOARD_CHANGED |
               TETRIS_EVENT_HUD_CHANGED |
               TETRIS_EVENT_BUTTONS_CHANGED;
    }

    return TETRIS_EVENT_BOARD_CHANGED |
           TETRIS_EVENT_HUD_CHANGED;
}

static void Tetris_InitButtons(TetrisState *game) {
    VirtualButtonList_InitWithStorage(&game->buttons,
                                      game->button_storage,
                                      TETRIS_BUTTON_COUNT);
    (void)VirtualButtonList_AddEx(
        &game->buttons,
        TETRIS_BUTTON_LEFT,
        "<",
        TETRIS_BUTTON_MARGIN_X,
        TETRIS_BUTTON_TOP_Y,
        TETRIS_BUTTON_MARGIN_X + TETRIS_BUTTON_W - 1U,
        TETRIS_BUTTON_TOP_Y + TETRIS_BUTTON_H - 1U,
        BUTTON_TYPE_OUTLINED,
        DISPLAY_COLOR565(23, 34, 42),
        DISPLAY_COLOR565(72, 120, 138),
        DISPLAY_COLOR565(43, 80, 94),
        DISPLAY_WHITE,
        NULL);
    (void)VirtualButtonList_AddEx(
        &game->buttons,
        TETRIS_BUTTON_RIGHT,
        ">",
        TETRIS_BUTTON_RIGHT_X,
        TETRIS_BUTTON_TOP_Y,
        TETRIS_BUTTON_RIGHT_X + TETRIS_BUTTON_W - 1U,
        TETRIS_BUTTON_TOP_Y + TETRIS_BUTTON_H - 1U,
        BUTTON_TYPE_OUTLINED,
        DISPLAY_COLOR565(23, 34, 42),
        DISPLAY_COLOR565(72, 120, 138),
        DISPLAY_COLOR565(43, 80, 94),
        DISPLAY_WHITE,
        NULL);
    (void)VirtualButtonList_AddEx(
        &game->buttons,
        TETRIS_BUTTON_HOLD,
        "HOLD",
        TETRIS_BUTTON_RIGHT_X,
        TETRIS_BUTTON_BOTTOM_Y,
        TETRIS_BUTTON_RIGHT_X + TETRIS_BUTTON_W - 1U,
        TETRIS_BUTTON_BOTTOM_Y + TETRIS_BUTTON_H - 1U,
        BUTTON_TYPE_OUTLINED,
        DISPLAY_COLOR565(23, 34, 42),
        DISPLAY_COLOR565(72, 120, 138),
        DISPLAY_COLOR565(43, 80, 94),
        DISPLAY_WHITE,
        NULL);
    (void)VirtualButtonList_AddEx(
        &game->buttons,
        TETRIS_BUTTON_DOWN,
        "DOWN",
        TETRIS_BUTTON_MARGIN_X,
        TETRIS_BUTTON_BOTTOM_Y,
        TETRIS_BUTTON_MARGIN_X + TETRIS_BUTTON_W - 1U,
        TETRIS_BUTTON_BOTTOM_Y + TETRIS_BUTTON_H - 1U,
        BUTTON_TYPE_OUTLINED,
        DISPLAY_COLOR565(23, 34, 42),
        DISPLAY_COLOR565(72, 120, 138),
        DISPLAY_COLOR565(43, 80, 94),
        DISPLAY_WHITE,
        NULL);
    (void)VirtualButtonList_AddEx(
        &game->buttons,
        TETRIS_BUTTON_RESTART,
        "NEW",
        118U,
        106U,
        202U,
        144U,
        BUTTON_TYPE_OUTLINED,
        DISPLAY_COLOR565(38, 52, 62),
        DISPLAY_COLOR565(245, 188, 70),
        DISPLAY_COLOR565(86, 70, 46),
        DISPLAY_WHITE,
        NULL);
    VirtualButtonList_SetEnabled(&game->buttons,
                                 TETRIS_BUTTON_RESTART,
                                 0U);
    VirtualButtonList_SetVisible(&game->buttons,
                                 TETRIS_BUTTON_RESTART,
                                 0U);
}

void Tetris_Init(TetrisState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state = (seed != 0U) ? seed : 0x6C8E9CF5U;
    game->level = 1U;
    game->phase = TETRIS_PHASE_PLAYING;
    Tetris_InitButtons(game);
    game->current = Tetris_CreatePiece(game);
    game->next = Tetris_CreatePiece(game);
    (void)Tetris_SpawnNext(game);
}

TetrisEvent Tetris_Update(TetrisState *game) {
    TetrisEvent event = TETRIS_EVENT_NONE;

    game->ticks++;
    if (game->phase != TETRIS_PHASE_PLAYING) {
        return event;
    }

    game->fall_ticks++;
    if (game->fall_ticks >= Tetris_FallInterval(game)) {
        game->fall_ticks = 0U;
        event |= Tetris_DropOne(game);
    }

    return event;
}

uint8_t Tetris_IsBoardTouch(uint16_t touch_x,
                            uint16_t touch_y) {
    return ((touch_x >= TETRIS_BOARD_X) &&
            (touch_x < (TETRIS_BOARD_X + TETRIS_BOARD_WIDTH)) &&
            (touch_y >= TETRIS_BOARD_Y) &&
            (touch_y < (TETRIS_BOARD_Y + TETRIS_BOARD_HEIGHT))) ?
           1U : 0U;
}

TetrisEvent Tetris_HandleTouch(TetrisState *game,
                               uint8_t touch_pressed,
                               uint16_t touch_x,
                               uint16_t touch_y) {
    TetrisEvent event = TETRIS_EVENT_NONE;
    uint8_t touch_started =
        ((touch_pressed != 0U) &&
         (game->touch_was_pressed == 0U)) ? 1U : 0U;

    if (VirtualButtonList_UpdateTouch(&game->buttons,
                                      touch_pressed,
                                      touch_x,
                                      touch_y)) {
        event |= TETRIS_EVENT_BUTTONS_CHANGED;
    }

    if ((game->phase == TETRIS_PHASE_GAME_OVER) &&
        VirtualButtonList_WasPressed(&game->buttons,
                                     TETRIS_BUTTON_RESTART)) {
        game->touch_was_pressed = touch_pressed;
        return event | TETRIS_EVENT_RESTART_REQUEST;
    }

    if (game->phase != TETRIS_PHASE_PLAYING) {
        game->touch_was_pressed = touch_pressed;
        return event;
    }

    if (VirtualButtonList_ShouldFire(&game->buttons,
                                     TETRIS_BUTTON_LEFT,
                                     TETRIS_REPEAT_DELAY_TICKS,
                                     TETRIS_REPEAT_PERIOD_TICKS) &&
        (Tetris_MovePiece(game, -1, 0) != 0U)) {
        event |= TETRIS_EVENT_BOARD_CHANGED;
    }

    if (VirtualButtonList_ShouldFire(&game->buttons,
                                     TETRIS_BUTTON_RIGHT,
                                     TETRIS_REPEAT_DELAY_TICKS,
                                     TETRIS_REPEAT_PERIOD_TICKS) &&
        (Tetris_MovePiece(game, 1, 0) != 0U)) {
        event |= TETRIS_EVENT_BOARD_CHANGED;
    }

    if (VirtualButtonList_ShouldFire(&game->buttons,
                                     TETRIS_BUTTON_DOWN,
                                     TETRIS_SOFT_DROP_DELAY_TICKS,
                                     TETRIS_SOFT_DROP_PERIOD_TICKS)) {
        event |= Tetris_DropOne(game);
        game->fall_ticks = 0U;
    }

    if (VirtualButtonList_WasPressed(&game->buttons,
                                     TETRIS_BUTTON_HOLD)) {
        event |= Tetris_HoldPiece(game);
    }

    if ((touch_started != 0U) &&
        (game->buttons.active_id == 0U) &&
        (Tetris_IsBoardTouch(touch_x, touch_y) != 0U) &&
        (Tetris_RotatePiece(game) != 0U)) {
        event |= TETRIS_EVENT_BOARD_CHANGED;
    }

    game->touch_was_pressed = touch_pressed;
    return event;
}

uint16_t Tetris_CellColorAt(const TetrisState *game,
                            uint8_t column,
                            uint8_t row) {
    for (uint8_t i = 0U; i < 4U; i++) {
        int8_t x;
        int8_t y;

        (void)Tetris_PieceBlock(&game->current, i, &x, &y);
        if ((x == (int8_t)column) &&
            (y == (int8_t)row) &&
            (game->phase == TETRIS_PHASE_PLAYING)) {
            return game->current.color;
        }
    }
    return game->board[row][column];
}
