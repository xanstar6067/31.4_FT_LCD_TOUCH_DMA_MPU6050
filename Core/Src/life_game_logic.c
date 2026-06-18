#include "life_game_logic.h"

#include <string.h>

#define LIFE_GAME_STEP_DIVIDER          3U
#define LIFE_GAME_INITIAL_DENSITY       88U
#define LIFE_GAME_WAIT_STATUS_TICKS     4U

static uint32_t LifeGame_Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static uint32_t LifeGame_Random(LifeGameState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xA3C59AC3U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static uint32_t LifeGame_Abs16(int16_t value) {
    int32_t wide = value;

    if (wide < 0) {
        wide = -wide;
    }
    return (uint32_t)wide;
}

static uint32_t LifeGame_AbsDiff16(int16_t a, int16_t b) {
    int32_t difference = (int32_t)a - (int32_t)b;

    if (difference < 0) {
        difference = -difference;
    }
    return (uint32_t)difference;
}

static uint8_t LifeGame_GetBit(const uint8_t *bits,
                               uint16_t index) {
    return (uint8_t)(
        (bits[index >> 3] >> (index & 7U)) & 1U);
}

static void LifeGame_SetBit(uint8_t *bits,
                            uint16_t index,
                            uint8_t value) {
    uint8_t mask = (uint8_t)(1U << (index & 7U));

    if (value != 0U) {
        bits[index >> 3] |= mask;
    } else {
        bits[index >> 3] &= (uint8_t)~mask;
    }
}

static void LifeGame_MarkDirty(LifeGameState *game,
                               uint16_t index) {
    game->dirty[index >> 3] |=
        (uint8_t)(1U << (index & 7U));
}

static void LifeGame_MixSensorSeed(
    LifeGameState *game,
    const MPU6000_Data_t *mpu_data) {
    uint32_t value = game->seed_accumulator;

    value ^= HAL_GetTick() + 0x9E3779B9U + (value << 6) +
             (value >> 2);
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_x << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_y;
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_y << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_z;
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_z << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_x;
    value ^= game->ticks * 0x85EBCA6BU;

    game->seed_accumulator = LifeGame_Hash(value);
    game->rng_state ^= game->seed_accumulator;
    game->rng_state = LifeGame_Hash(game->rng_state);
}

static uint8_t LifeGame_UpdateShake(
    LifeGameState *game,
    const MPU6000_Data_t *mpu_data,
    HAL_StatusTypeDef mpu_status) {
    uint32_t raw_energy;
    uint32_t gyro_motion;
    uint32_t gyro_jerk;

    game->mpu_online = (mpu_status == HAL_OK) ? 1U : 0U;
    if (mpu_status != HAL_OK) {
        game->shake_energy =
            (uint16_t)(((uint32_t)game->shake_energy * 3U) / 4U);
        return 0U;
    }

    LifeGame_MixSensorSeed(game, mpu_data);
    if (game->initialized_gyro == 0U) {
        game->previous_gyro_x = mpu_data->gyro_x;
        game->previous_gyro_y = mpu_data->gyro_y;
        game->previous_gyro_z = mpu_data->gyro_z;
        game->initialized_gyro = 1U;
        return 0U;
    }

    gyro_jerk =
        LifeGame_AbsDiff16(mpu_data->gyro_x,
                           game->previous_gyro_x) +
        LifeGame_AbsDiff16(mpu_data->gyro_y,
                           game->previous_gyro_y) +
        LifeGame_AbsDiff16(mpu_data->gyro_z,
                           game->previous_gyro_z);
    gyro_motion =
        LifeGame_Abs16(mpu_data->gyro_x) +
        LifeGame_Abs16(mpu_data->gyro_y) +
        LifeGame_Abs16(mpu_data->gyro_z);
    raw_energy = gyro_jerk + (gyro_motion / 4U);
    if (raw_energy > 65535U) {
        raw_energy = 65535U;
    }

    game->shake_energy =
        (uint16_t)((((uint32_t)game->shake_energy * 3U) +
                    raw_energy) / 4U);
    game->previous_gyro_x = mpu_data->gyro_x;
    game->previous_gyro_y = mpu_data->gyro_y;
    game->previous_gyro_z = mpu_data->gyro_z;

    return (game->shake_energy >=
            LIFE_GAME_SHAKE_START_THRESHOLD) ? 1U : 0U;
}

static void LifeGame_ClearBoards(LifeGameState *game) {
    memset(game->cells, 0, sizeof(game->cells));
    memset(game->next_cells, 0, sizeof(game->next_cells));
    memset(game->dirty, 0, sizeof(game->dirty));
}

static void LifeGame_SeedBoard(LifeGameState *game) {
    uint32_t population = 0U;

    LifeGame_ClearBoards(game);
    for (uint16_t index = 0U;
         index < LIFE_GAME_CELL_COUNT;
         index++) {
        uint8_t alive =
            ((LifeGame_Random(game) & 0xFFU) <
             LIFE_GAME_INITIAL_DENSITY) ? 1U : 0U;

        if (alive != 0U) {
            LifeGame_SetBit(game->cells, index, 1U);
            population++;
        }
    }

    game->generation = 0U;
    game->population = population;
    game->stable_ticks = 0U;
    game->step_ticks = LIFE_GAME_STEP_DIVIDER;
    game->phase = LIFE_GAME_PHASE_RUNNING;
}

static uint8_t LifeGame_CountNeighbors(
    const LifeGameState *game,
    uint16_t column,
    uint16_t row) {
    uint16_t north = (row == 0U) ?
                     (LIFE_GAME_ROWS - 1U) : (row - 1U);
    uint16_t south = (row + 1U == LIFE_GAME_ROWS) ?
                     0U : (row + 1U);
    uint16_t west = (column == 0U) ?
                    (LIFE_GAME_COLUMNS - 1U) : (column - 1U);
    uint16_t east = (column + 1U == LIFE_GAME_COLUMNS) ?
                    0U : (column + 1U);
    uint8_t count = 0U;

    count += LifeGame_CellAlive(game, west, north);
    count += LifeGame_CellAlive(game, column, north);
    count += LifeGame_CellAlive(game, east, north);
    count += LifeGame_CellAlive(game, west, row);
    count += LifeGame_CellAlive(game, east, row);
    count += LifeGame_CellAlive(game, west, south);
    count += LifeGame_CellAlive(game, column, south);
    count += LifeGame_CellAlive(game, east, south);
    return count;
}

static LifeGameEvent LifeGame_Step(LifeGameState *game) {
    uint32_t population = 0U;
    uint16_t changed = 0U;

    memset(game->next_cells, 0, sizeof(game->next_cells));
    memset(game->dirty, 0, sizeof(game->dirty));

    for (uint16_t row = 0U; row < LIFE_GAME_ROWS; row++) {
        for (uint16_t column = 0U;
             column < LIFE_GAME_COLUMNS;
             column++) {
            uint16_t index =
                (uint16_t)((row * LIFE_GAME_COLUMNS) + column);
            uint8_t alive = LifeGame_GetBit(game->cells, index);
            uint8_t neighbors =
                LifeGame_CountNeighbors(game, column, row);
            uint8_t next_alive =
                (neighbors == 3U) ||
                ((alive != 0U) && (neighbors == 2U));

            if (next_alive != 0U) {
                LifeGame_SetBit(game->next_cells, index, 1U);
                population++;
            }
            if (next_alive != alive) {
                LifeGame_MarkDirty(game, index);
                changed++;
            }
        }
    }

    memcpy(game->cells, game->next_cells, sizeof(game->cells));
    game->generation++;
    game->population = population;
    if (changed == 0U) {
        game->stable_ticks++;
    } else {
        game->stable_ticks = 0U;
    }
    return LIFE_GAME_EVENT_STEP |
           LIFE_GAME_EVENT_HUD_CHANGED;
}

void LifeGame_Init(LifeGameState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state =
        (seed != 0U) ? seed : 0x4F1BBCDCU;
    game->seed_accumulator =
        LifeGame_Hash(game->rng_state ^ HAL_GetTick());
    game->phase = LIFE_GAME_PHASE_WAITING;
    game->mpu_online = 1U;
    LifeGame_ClearBoards(game);
}

LifeGameEvent LifeGame_Update(LifeGameState *game,
                              const MPU6000_Data_t *mpu_data,
                              HAL_StatusTypeDef mpu_status) {
    LifeGameEvent event = LIFE_GAME_EVENT_NONE;

    game->ticks++;
    if (game->phase == LIFE_GAME_PHASE_WAITING) {
        game->wait_animation++;
        if (LifeGame_UpdateShake(
                game, mpu_data, mpu_status) != 0U) {
            LifeGame_MixSensorSeed(game, mpu_data);
            LifeGame_SeedBoard(game);
            return LIFE_GAME_EVENT_STARTED |
                   LIFE_GAME_EVENT_HUD_CHANGED;
        }
        if ((game->wait_animation %
             LIFE_GAME_WAIT_STATUS_TICKS) == 0U) {
            event |= LIFE_GAME_EVENT_WAIT_STATUS;
        }
        return event;
    }

    if (game->step_ticks > 0U) {
        game->step_ticks--;
    }
    if (game->step_ticks != 0U) {
        return event;
    }
    game->step_ticks = LIFE_GAME_STEP_DIVIDER;
    return LifeGame_Step(game);
}

uint8_t LifeGame_CellAlive(const LifeGameState *game,
                           uint16_t column,
                           uint16_t row) {
    uint16_t index =
        (uint16_t)((row * LIFE_GAME_COLUMNS) + column);

    return LifeGame_GetBit(game->cells, index);
}

uint8_t LifeGame_CellDirty(const LifeGameState *game,
                           uint16_t column,
                           uint16_t row) {
    uint16_t index =
        (uint16_t)((row * LIFE_GAME_COLUMNS) + column);

    return LifeGame_GetBit(game->dirty, index);
}

void LifeGame_ClearDirty(LifeGameState *game) {
    memset(game->dirty, 0, sizeof(game->dirty));
}
