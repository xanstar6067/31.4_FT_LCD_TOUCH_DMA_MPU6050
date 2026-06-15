#ifndef INC_MARBLE_MAZE_LOGIC_H_
#define INC_MARBLE_MAZE_LOGIC_H_

#include <stdint.h>

#define MARBLE_MAZE_SCREEN_WIDTH       320
#define MARBLE_MAZE_SCREEN_HEIGHT      240
#define MARBLE_MAZE_PLAYFIELD_TOP      28

#define MARBLE_MAZE_COLUMNS            10U
#define MARBLE_MAZE_ROWS               7U
#define MARBLE_MAZE_CELL_COUNT         \
    (MARBLE_MAZE_COLUMNS * MARBLE_MAZE_ROWS)
#define MARBLE_MAZE_CELL_SIZE          28
#define MARBLE_MAZE_WALL_THICKNESS     3
#define MARBLE_MAZE_LEFT               18
#define MARBLE_MAZE_TOP                35
#define MARBLE_MAZE_BALL_RADIUS        6
#define MARBLE_MAZE_GOAL_RADIUS        8

#define MARBLE_MAZE_WALL_NORTH         0x01U
#define MARBLE_MAZE_WALL_EAST          0x02U
#define MARBLE_MAZE_WALL_SOUTH         0x04U
#define MARBLE_MAZE_WALL_WEST          0x08U
#define MARBLE_MAZE_ALL_WALLS          0x0FU

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
} MarbleMazeBall;

typedef struct {
    MarbleMazeBall ball;
    uint32_t rng_state;
    uint32_t ticks;
    float filtered_accel_x;
    float filtered_accel_y;
    uint16_t level;
    uint8_t start_cell;
    uint8_t goal_cell;
    uint8_t theme;
    uint8_t walls[MARBLE_MAZE_CELL_COUNT];
} MarbleMazeState;

typedef uint8_t MarbleMazeEvent;

#define MARBLE_MAZE_EVENT_NONE          0x00U
#define MARBLE_MAZE_EVENT_LEVEL_STARTED 0x01U

void MarbleMaze_Init(MarbleMazeState *game, uint32_t seed);
MarbleMazeEvent MarbleMaze_Update(MarbleMazeState *game,
                                  int16_t accel_x,
                                  int16_t accel_y);
int16_t MarbleMaze_CellCenterX(uint8_t cell);
int16_t MarbleMaze_CellCenterY(uint8_t cell);

#endif /* INC_MARBLE_MAZE_LOGIC_H_ */
