#ifndef INC_LIFE_GAME_LOGIC_H_
#define INC_LIFE_GAME_LOGIC_H_

#include <stdint.h>
#include "mpu6000.h"

#define LIFE_GAME_SCREEN_WIDTH          320
#define LIFE_GAME_SCREEN_HEIGHT         240
#define LIFE_GAME_HEADER_HEIGHT         30
#define LIFE_GAME_BOARD_X               0
#define LIFE_GAME_BOARD_Y               36
#define LIFE_GAME_CELL_SIZE             4U
#define LIFE_GAME_COLUMNS               80U
#define LIFE_GAME_ROWS                  50U
#define LIFE_GAME_CELL_COUNT            (LIFE_GAME_COLUMNS * LIFE_GAME_ROWS)
#define LIFE_GAME_BITSET_SIZE           ((LIFE_GAME_CELL_COUNT + 7U) / 8U)
#define LIFE_GAME_SHAKE_START_THRESHOLD 42000U

typedef enum {
    LIFE_GAME_PHASE_WAITING = 0,
    LIFE_GAME_PHASE_RUNNING
} LifeGamePhase;

typedef struct {
    uint8_t cells[LIFE_GAME_BITSET_SIZE];
    uint8_t next_cells[LIFE_GAME_BITSET_SIZE];
    uint8_t dirty[LIFE_GAME_BITSET_SIZE];
    uint32_t rng_state;
    uint32_t seed_accumulator;
    uint32_t generation;
    uint32_t population;
    uint32_t ticks;
    int16_t previous_gyro_x;
    int16_t previous_gyro_y;
    int16_t previous_gyro_z;
    uint16_t shake_energy;
    uint16_t wait_animation;
    uint16_t stable_ticks;
    uint8_t initialized_gyro;
    uint8_t step_ticks;
    uint8_t mpu_online;
    LifeGamePhase phase;
} LifeGameState;

typedef uint8_t LifeGameEvent;

#define LIFE_GAME_EVENT_NONE            0x00U
#define LIFE_GAME_EVENT_WAIT_STATUS     0x01U
#define LIFE_GAME_EVENT_STARTED         0x02U
#define LIFE_GAME_EVENT_STEP            0x04U
#define LIFE_GAME_EVENT_HUD_CHANGED     0x08U
#define LIFE_GAME_EVENT_TOUCH_SPAWN     0x10U

void LifeGame_Init(LifeGameState *game, uint32_t seed);
LifeGameEvent LifeGame_Update(LifeGameState *game,
                              const MPU6000_Data_t *mpu_data,
                              HAL_StatusTypeDef mpu_status);
LifeGameEvent LifeGame_HandleTouch(LifeGameState *game,
                                   uint8_t touch_pressed,
                                   uint16_t touch_x,
                                   uint16_t touch_y);
uint8_t LifeGame_CellAlive(const LifeGameState *game,
                           uint16_t column,
                           uint16_t row);
uint8_t LifeGame_CellDirty(const LifeGameState *game,
                           uint16_t column,
                           uint16_t row);
void LifeGame_ClearDirty(LifeGameState *game);

#endif /* INC_LIFE_GAME_LOGIC_H_ */
