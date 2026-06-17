#ifndef INC_MICRO_COLONY_LOGIC_H_
#define INC_MICRO_COLONY_LOGIC_H_

#include <stdint.h>
#include "mpu6050.h"

#define MICRO_COLONY_SCREEN_WIDTH       320
#define MICRO_COLONY_SCREEN_HEIGHT      240
#define MICRO_COLONY_HEADER_HEIGHT      30
#define MICRO_COLONY_BOARD_X            0
#define MICRO_COLONY_BOARD_Y            36
#define MICRO_COLONY_CELL_SIZE          4U
#define MICRO_COLONY_COLUMNS            80U
#define MICRO_COLONY_ROWS               50U
#define MICRO_COLONY_CELL_COUNT         (MICRO_COLONY_COLUMNS * MICRO_COLONY_ROWS)
#define MICRO_COLONY_FOOD_SIZE          ((MICRO_COLONY_CELL_COUNT + 3U) / 4U)
#define MICRO_COLONY_DIRTY_SIZE         ((MICRO_COLONY_CELL_COUNT + 7U) / 8U)

#define MICRO_COLONY_MAX_BOTS           72U
#define MICRO_COLONY_INITIAL_BOTS       18U
#define MICRO_COLONY_BRAIN_INPUTS       9U
#define MICRO_COLONY_BRAIN_OUTPUTS      5U
#define MICRO_COLONY_MAX_ENERGY         520U

typedef enum {
    MICRO_COLONY_OUTPUT_MOVE = 0,
    MICRO_COLONY_OUTPUT_LEFT,
    MICRO_COLONY_OUTPUT_RIGHT,
    MICRO_COLONY_OUTPUT_EAT,
    MICRO_COLONY_OUTPUT_SPLIT
} MicroColonyOutput;

typedef struct {
    int8_t weights[MICRO_COLONY_BRAIN_OUTPUTS]
                  [MICRO_COLONY_BRAIN_INPUTS];
    uint16_t energy;
    uint16_t age;
    uint16_t max_age;
    uint16_t generation;
    uint8_t x;
    uint8_t y;
    uint8_t direction;
    uint8_t hue;
    uint8_t mutation;
    uint8_t alive;
} MicroColonyBot;

typedef struct {
    MicroColonyBot bots[MICRO_COLONY_MAX_BOTS];
    uint8_t food[MICRO_COLONY_FOOD_SIZE];
    uint8_t dirty[MICRO_COLONY_DIRTY_SIZE];
    uint32_t rng_state;
    uint32_t ticks;
    uint32_t births;
    uint32_t deaths;
    uint32_t extinctions;
    uint16_t population;
    uint16_t food_total;
    uint8_t active_bot;
    uint8_t extinction_pause;
    uint8_t hud_divider;
} MicroColonyState;

typedef uint8_t MicroColonyEvent;

#define MICRO_COLONY_EVENT_NONE         0x00U
#define MICRO_COLONY_EVENT_FULL_REDRAW  0x01U
#define MICRO_COLONY_EVENT_CELLS        0x02U
#define MICRO_COLONY_EVENT_HUD          0x04U

void MicroColony_Init(MicroColonyState *game, uint32_t seed);
MicroColonyEvent MicroColony_Update(MicroColonyState *game,
                                    const MPU6050_Data_t *mpu_data,
                                    HAL_StatusTypeDef mpu_status);
uint8_t MicroColony_FoodAt(const MicroColonyState *game,
                           uint8_t column,
                           uint8_t row);
uint8_t MicroColony_CellDirty(const MicroColonyState *game,
                              uint8_t column,
                              uint8_t row);
const MicroColonyBot *MicroColony_BotAt(
    const MicroColonyState *game,
    uint8_t column,
    uint8_t row);
void MicroColony_ClearDirty(MicroColonyState *game);

#endif /* INC_MICRO_COLONY_LOGIC_H_ */
