#ifndef INC_TETRIS_LOGIC_H_
#define INC_TETRIS_LOGIC_H_

#include <stdint.h>

#include "virtual_buttons.h"

#define TETRIS_SCREEN_WIDTH          320U
#define TETRIS_SCREEN_HEIGHT         240U
#define TETRIS_BOARD_COLUMNS         10U
#define TETRIS_BOARD_ROWS            20U
#define TETRIS_CELL_SIZE             10U
#define TETRIS_BOARD_X               110U
#define TETRIS_BOARD_Y               24U
#define TETRIS_BOARD_WIDTH           (TETRIS_BOARD_COLUMNS * TETRIS_CELL_SIZE)
#define TETRIS_BOARD_HEIGHT          (TETRIS_BOARD_ROWS * TETRIS_CELL_SIZE)
#define TETRIS_BUTTON_COUNT          5U

typedef enum {
    TETRIS_PHASE_PLAYING = 0,
    TETRIS_PHASE_GAME_OVER
} TetrisPhase;

typedef enum {
    TETRIS_BUTTON_LEFT = 1,
    TETRIS_BUTTON_RIGHT,
    TETRIS_BUTTON_DOWN,
    TETRIS_BUTTON_HOLD,
    TETRIS_BUTTON_RESTART
} TetrisButtonId;

typedef struct {
    uint8_t type;
    uint8_t rotation;
    int8_t x;
    int8_t y;
    uint16_t color;
} TetrisPiece;

typedef struct {
    uint16_t board[TETRIS_BOARD_ROWS][TETRIS_BOARD_COLUMNS];
    VirtualButtonList buttons;
    VirtualButton button_storage[TETRIS_BUTTON_COUNT];
    TetrisPiece current;
    TetrisPiece next;
    TetrisPiece hold;
    uint32_t rng_state;
    uint32_t ticks;
    uint32_t score;
    uint16_t lines;
    uint16_t fall_ticks;
    uint8_t level;
    uint8_t hold_valid;
    uint8_t hold_used;
    uint8_t touch_was_pressed;
    TetrisPhase phase;
} TetrisState;

typedef uint8_t TetrisEvent;

#define TETRIS_EVENT_NONE             0x00U
#define TETRIS_EVENT_REDRAW           0x01U
#define TETRIS_EVENT_BOARD_CHANGED    0x02U
#define TETRIS_EVENT_HUD_CHANGED      0x04U
#define TETRIS_EVENT_BUTTONS_CHANGED  0x08U
#define TETRIS_EVENT_GAME_OVER        0x10U
#define TETRIS_EVENT_RESTART_REQUEST  0x20U

void Tetris_Init(TetrisState *game, uint32_t seed);
TetrisEvent Tetris_Update(TetrisState *game);
TetrisEvent Tetris_HandleTouch(TetrisState *game,
                               uint8_t touch_pressed,
                               uint16_t touch_x,
                               uint16_t touch_y);
uint16_t Tetris_CellColorAt(const TetrisState *game,
                            uint8_t column,
                            uint8_t row);
uint8_t Tetris_PieceBlock(const TetrisPiece *piece,
                          uint8_t block_index,
                          int8_t *x,
                          int8_t *y);
uint8_t Tetris_IsBoardTouch(uint16_t touch_x,
                            uint16_t touch_y);

#endif /* INC_TETRIS_LOGIC_H_ */
