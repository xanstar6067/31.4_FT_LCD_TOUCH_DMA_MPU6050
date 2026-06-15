#include "marble_maze_logic.h"

#include <math.h>
#include <string.h>

#define MAZE_INPUT_FILTER          0.22f
#define MAZE_INPUT_DEAD_ZONE       260.0f
#define MAZE_ACCEL_SCALE           0.00011f
#define MAZE_FRICTION              0.955f
#define MAZE_MAX_SPEED             5.4f
#define MAZE_WALL_BOUNCE           0.20f
#define MAZE_THEME_COUNT           5U
#define MAZE_MAX_EXTRA_OPENINGS    7U

static uint32_t MarbleMaze_Random(MarbleMazeState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xD1B54A35U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static float MarbleMaze_Clamp(float value,
                              float minimum,
                              float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float MarbleMaze_DeadZone(float value) {
    if ((value > -MAZE_INPUT_DEAD_ZONE) &&
        (value < MAZE_INPUT_DEAD_ZONE)) {
        return 0.0f;
    }
    return (value > 0.0f) ?
           value - MAZE_INPUT_DEAD_ZONE :
           value + MAZE_INPUT_DEAD_ZONE;
}

int16_t MarbleMaze_CellCenterX(uint8_t cell) {
    uint8_t column = cell % MARBLE_MAZE_COLUMNS;

    return MARBLE_MAZE_LEFT +
           (column * MARBLE_MAZE_CELL_SIZE) +
           (MARBLE_MAZE_CELL_SIZE / 2);
}

int16_t MarbleMaze_CellCenterY(uint8_t cell) {
    uint8_t row = cell / MARBLE_MAZE_COLUMNS;

    return MARBLE_MAZE_TOP +
           (row * MARBLE_MAZE_CELL_SIZE) +
           (MARBLE_MAZE_CELL_SIZE / 2);
}

static void MarbleMaze_RemoveWall(MarbleMazeState *game,
                                  uint8_t cell,
                                  uint8_t neighbor,
                                  uint8_t direction) {
    static const uint8_t opposite[4] = {
        MARBLE_MAZE_WALL_SOUTH,
        MARBLE_MAZE_WALL_WEST,
        MARBLE_MAZE_WALL_NORTH,
        MARBLE_MAZE_WALL_EAST
    };

    game->walls[cell] &= (uint8_t)~(1U << direction);
    game->walls[neighbor] &= (uint8_t)~opposite[direction];
}

static uint8_t MarbleMaze_UnvisitedNeighbors(
    uint8_t cell,
    const uint8_t *visited,
    uint8_t *neighbors,
    uint8_t *directions) {
    uint8_t row = cell / MARBLE_MAZE_COLUMNS;
    uint8_t column = cell % MARBLE_MAZE_COLUMNS;
    uint8_t count = 0U;

    if ((row > 0U) &&
        (visited[cell - MARBLE_MAZE_COLUMNS] == 0U)) {
        neighbors[count] = cell - MARBLE_MAZE_COLUMNS;
        directions[count++] = 0U;
    }
    if ((column + 1U < MARBLE_MAZE_COLUMNS) &&
        (visited[cell + 1U] == 0U)) {
        neighbors[count] = cell + 1U;
        directions[count++] = 1U;
    }
    if ((row + 1U < MARBLE_MAZE_ROWS) &&
        (visited[cell + MARBLE_MAZE_COLUMNS] == 0U)) {
        neighbors[count] = cell + MARBLE_MAZE_COLUMNS;
        directions[count++] = 2U;
    }
    if ((column > 0U) &&
        (visited[cell - 1U] == 0U)) {
        neighbors[count] = cell - 1U;
        directions[count++] = 3U;
    }
    return count;
}

static uint8_t MarbleMaze_StartCell(MarbleMazeState *game) {
    static const uint8_t corners[4] = {
        0U,
        MARBLE_MAZE_COLUMNS - 1U,
        (MARBLE_MAZE_ROWS - 1U) * MARBLE_MAZE_COLUMNS,
        MARBLE_MAZE_CELL_COUNT - 1U
    };

    return corners[MarbleMaze_Random(game) % 4U];
}

static void MarbleMaze_AddOpenings(MarbleMazeState *game) {
    uint8_t opening_count =
        1U + (uint8_t)((game->level - 1U) / 3U);

    if (opening_count > MAZE_MAX_EXTRA_OPENINGS) {
        opening_count = MAZE_MAX_EXTRA_OPENINGS;
    }

    for (uint8_t opening = 0U;
         opening < opening_count;
         opening++) {
        for (uint8_t attempt = 0U; attempt < 32U; attempt++) {
            uint8_t cell =
                (uint8_t)(MarbleMaze_Random(game) %
                          MARBLE_MAZE_CELL_COUNT);
            uint8_t row = cell / MARBLE_MAZE_COLUMNS;
            uint8_t column = cell % MARBLE_MAZE_COLUMNS;
            uint8_t use_east =
                (uint8_t)(MarbleMaze_Random(game) & 1U);

            if ((use_east != 0U) &&
                (column + 1U < MARBLE_MAZE_COLUMNS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_EAST) != 0U)) {
                MarbleMaze_RemoveWall(game, cell,
                                      cell + 1U, 1U);
                break;
            }
            if ((row + 1U < MARBLE_MAZE_ROWS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_SOUTH) != 0U)) {
                MarbleMaze_RemoveWall(
                    game, cell,
                    cell + MARBLE_MAZE_COLUMNS, 2U);
                break;
            }
        }
    }
}

static uint8_t MarbleMaze_FarthestCell(const MarbleMazeState *game,
                                       uint8_t start) {
    uint8_t distance[MARBLE_MAZE_CELL_COUNT];
    uint8_t queue[MARBLE_MAZE_CELL_COUNT];
    uint8_t head = 0U;
    uint8_t tail = 0U;
    uint8_t farthest = start;

    memset(distance, 0xFF, sizeof(distance));
    distance[start] = 0U;
    queue[tail++] = start;

    while (head < tail) {
        uint8_t cell = queue[head++];
        uint8_t row = cell / MARBLE_MAZE_COLUMNS;
        uint8_t column = cell % MARBLE_MAZE_COLUMNS;
        uint8_t next[4];
        uint8_t count = 0U;

        if ((row > 0U) &&
            ((game->walls[cell] &
              MARBLE_MAZE_WALL_NORTH) == 0U)) {
            next[count++] = cell - MARBLE_MAZE_COLUMNS;
        }
        if ((column + 1U < MARBLE_MAZE_COLUMNS) &&
            ((game->walls[cell] &
              MARBLE_MAZE_WALL_EAST) == 0U)) {
            next[count++] = cell + 1U;
        }
        if ((row + 1U < MARBLE_MAZE_ROWS) &&
            ((game->walls[cell] &
              MARBLE_MAZE_WALL_SOUTH) == 0U)) {
            next[count++] = cell + MARBLE_MAZE_COLUMNS;
        }
        if ((column > 0U) &&
            ((game->walls[cell] &
              MARBLE_MAZE_WALL_WEST) == 0U)) {
            next[count++] = cell - 1U;
        }

        for (uint8_t index = 0U; index < count; index++) {
            uint8_t neighbor = next[index];

            if (distance[neighbor] != 0xFFU) {
                continue;
            }
            distance[neighbor] = distance[cell] + 1U;
            queue[tail++] = neighbor;
            if (distance[neighbor] > distance[farthest]) {
                farthest = neighbor;
            }
        }
    }
    return farthest;
}

static void MarbleMaze_Generate(MarbleMazeState *game) {
    uint8_t visited[MARBLE_MAZE_CELL_COUNT] = {0};
    uint8_t stack[MARBLE_MAZE_CELL_COUNT];
    uint8_t stack_size = 0U;
    uint8_t current;

    memset(game->walls, MARBLE_MAZE_ALL_WALLS,
           sizeof(game->walls));
    game->start_cell = MarbleMaze_StartCell(game);
    current = game->start_cell;
    visited[current] = 1U;
    stack[stack_size++] = current;

    while (stack_size > 0U) {
        uint8_t neighbors[4];
        uint8_t directions[4];
        uint8_t count =
            MarbleMaze_UnvisitedNeighbors(current,
                                           visited,
                                           neighbors,
                                           directions);

        if (count == 0U) {
            stack_size--;
            if (stack_size > 0U) {
                current = stack[stack_size - 1U];
            }
            continue;
        }

        {
            uint8_t choice =
                (uint8_t)(MarbleMaze_Random(game) % count);
            uint8_t neighbor = neighbors[choice];

            MarbleMaze_RemoveWall(game, current,
                                  neighbor, directions[choice]);
            visited[neighbor] = 1U;
            current = neighbor;
            stack[stack_size++] = current;
        }
    }

    MarbleMaze_AddOpenings(game);
    game->goal_cell =
        MarbleMaze_FarthestCell(game, game->start_cell);
    game->theme =
        (uint8_t)(MarbleMaze_Random(game) % MAZE_THEME_COUNT);
}

static void MarbleMaze_StartLevel(MarbleMazeState *game,
                                  uint16_t level) {
    game->level = level;
    game->ticks = 0U;
    game->filtered_accel_x = 0.0f;
    game->filtered_accel_y = 0.0f;
    MarbleMaze_Generate(game);
    game->ball.x = MarbleMaze_CellCenterX(game->start_cell);
    game->ball.y = MarbleMaze_CellCenterY(game->start_cell);
    game->ball.vx = 0.0f;
    game->ball.vy = 0.0f;
}

static uint8_t MarbleMaze_CircleRectCollision(
    float circle_x,
    float circle_y,
    float radius,
    float rectangle_x,
    float rectangle_y,
    float rectangle_width,
    float rectangle_height) {
    float nearest_x =
        MarbleMaze_Clamp(circle_x,
                         rectangle_x,
                         rectangle_x + rectangle_width);
    float nearest_y =
        MarbleMaze_Clamp(circle_y,
                         rectangle_y,
                         rectangle_y + rectangle_height);
    float dx = circle_x - nearest_x;
    float dy = circle_y - nearest_y;

    return ((dx * dx) + (dy * dy)) < (radius * radius);
}

static uint8_t MarbleMaze_HitsWall(const MarbleMazeState *game,
                                   float x,
                                   float y) {
    for (uint8_t row = 0U; row < MARBLE_MAZE_ROWS; row++) {
        for (uint8_t column = 0U;
             column < MARBLE_MAZE_COLUMNS;
             column++) {
            uint8_t cell =
                (row * MARBLE_MAZE_COLUMNS) + column;
            float cell_x =
                MARBLE_MAZE_LEFT +
                (column * MARBLE_MAZE_CELL_SIZE);
            float cell_y =
                MARBLE_MAZE_TOP +
                (row * MARBLE_MAZE_CELL_SIZE);

            if (((game->walls[cell] &
                  MARBLE_MAZE_WALL_NORTH) != 0U) &&
                (MarbleMaze_CircleRectCollision(
                    x, y, MARBLE_MAZE_BALL_RADIUS,
                    cell_x, cell_y,
                    MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return 1U;
            }
            if (((game->walls[cell] &
                  MARBLE_MAZE_WALL_WEST) != 0U) &&
                (MarbleMaze_CircleRectCollision(
                    x, y, MARBLE_MAZE_BALL_RADIUS,
                    cell_x, cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE) != 0U)) {
                return 1U;
            }
            if ((column + 1U == MARBLE_MAZE_COLUMNS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_EAST) != 0U) &&
                (MarbleMaze_CircleRectCollision(
                    x, y, MARBLE_MAZE_BALL_RADIUS,
                    cell_x + MARBLE_MAZE_CELL_SIZE,
                    cell_y,
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return 1U;
            }
            if ((row + 1U == MARBLE_MAZE_ROWS) &&
                ((game->walls[cell] &
                  MARBLE_MAZE_WALL_SOUTH) != 0U) &&
                (MarbleMaze_CircleRectCollision(
                    x, y, MARBLE_MAZE_BALL_RADIUS,
                    cell_x,
                    cell_y + MARBLE_MAZE_CELL_SIZE,
                    MARBLE_MAZE_CELL_SIZE +
                    MARBLE_MAZE_WALL_THICKNESS,
                    MARBLE_MAZE_WALL_THICKNESS) != 0U)) {
                return 1U;
            }
        }
    }
    return 0U;
}

static void MarbleMaze_MoveBall(MarbleMazeState *game) {
    float maximum_speed =
        fmaxf(fabsf(game->ball.vx), fabsf(game->ball.vy));
    uint8_t steps = (uint8_t)ceilf(maximum_speed);
    float step_x;
    float step_y;

    if (steps == 0U) {
        steps = 1U;
    }
    step_x = game->ball.vx / steps;
    step_y = game->ball.vy / steps;

    for (uint8_t step = 0U; step < steps; step++) {
        float candidate_x = game->ball.x + step_x;

        if (MarbleMaze_HitsWall(game,
                                candidate_x,
                                game->ball.y) == 0U) {
            game->ball.x = candidate_x;
        } else {
            game->ball.vx = -game->ball.vx * MAZE_WALL_BOUNCE;
            step_x = 0.0f;
        }

        {
            float candidate_y = game->ball.y + step_y;

            if (MarbleMaze_HitsWall(game,
                                    game->ball.x,
                                    candidate_y) == 0U) {
                game->ball.y = candidate_y;
            } else {
                game->ball.vy = -game->ball.vy * MAZE_WALL_BOUNCE;
                step_y = 0.0f;
            }
        }
    }
}

void MarbleMaze_Init(MarbleMazeState *game, uint32_t seed) {
    game->rng_state = (seed != 0U) ? seed : 0x94D049BBU;
    MarbleMaze_StartLevel(game, 1U);
}

MarbleMazeEvent MarbleMaze_Update(MarbleMazeState *game,
                                  int16_t accel_x,
                                  int16_t accel_y) {
    float control_x;
    float control_y;
    float goal_x;
    float goal_y;
    float dx;
    float dy;
    float goal_distance =
        MARBLE_MAZE_BALL_RADIUS + MARBLE_MAZE_GOAL_RADIUS - 2.0f;

    game->filtered_accel_x +=
        ((float)accel_x - game->filtered_accel_x) *
        MAZE_INPUT_FILTER;
    game->filtered_accel_y +=
        ((float)accel_y - game->filtered_accel_y) *
        MAZE_INPUT_FILTER;

    control_x = MarbleMaze_DeadZone(game->filtered_accel_x);
    control_y = MarbleMaze_DeadZone(game->filtered_accel_y);
    game->ball.vx += control_x * MAZE_ACCEL_SCALE;
    game->ball.vy += control_y * MAZE_ACCEL_SCALE;
    game->ball.vx *= MAZE_FRICTION;
    game->ball.vy *= MAZE_FRICTION;
    game->ball.vx = MarbleMaze_Clamp(game->ball.vx,
                                     -MAZE_MAX_SPEED,
                                     MAZE_MAX_SPEED);
    game->ball.vy = MarbleMaze_Clamp(game->ball.vy,
                                     -MAZE_MAX_SPEED,
                                     MAZE_MAX_SPEED);

    MarbleMaze_MoveBall(game);
    game->ticks++;

    goal_x = MarbleMaze_CellCenterX(game->goal_cell);
    goal_y = MarbleMaze_CellCenterY(game->goal_cell);
    dx = game->ball.x - goal_x;
    dy = game->ball.y - goal_y;
    if (((dx * dx) + (dy * dy)) <=
        (goal_distance * goal_distance)) {
        MarbleMaze_StartLevel(game, game->level + 1U);
        return MARBLE_MAZE_EVENT_LEVEL_STARTED;
    }

    return MARBLE_MAZE_EVENT_NONE;
}
