#include "micro_colony_logic.h"

#include <limits.h>
#include <string.h>

#define MICRO_COLONY_BOTS_PER_TICK      18U
#define MICRO_COLONY_FOOD_GROWTH_TICKS  2U
#define MICRO_COLONY_FOOD_GROWTH_CELLS  4U
#define MICRO_COLONY_START_ENERGY       230U
#define MICRO_COLONY_BIRTH_ENERGY       330U
#define MICRO_COLONY_CHILD_ENERGY       145U
#define MICRO_COLONY_BIRTH_COST         60U
#define MICRO_COLONY_EAT_GAIN           48U
#define MICRO_COLONY_BASAL_COST         1U
#define MICRO_COLONY_MOVE_COST          3U
#define MICRO_COLONY_TURN_COST          1U
#define MICRO_COLONY_BLOCKED_COST       2U
#define MICRO_COLONY_EXTINCTION_PAUSE   50U

static uint32_t MicroColony_Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static uint32_t MicroColony_Random(MicroColonyState *game) {
    uint32_t value = game->rng_state;

    if (value == 0U) {
        value = 0xA7C15B3DU;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->rng_state = value;
    return value;
}

static void MicroColony_MixSensor(
    MicroColonyState *game,
    const MPU6000_Data_t *mpu_data,
    HAL_StatusTypeDef mpu_status) {
    uint32_t value;

    if (mpu_status != HAL_OK) {
        return;
    }

    value = game->rng_state ^ HAL_GetTick();
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_x << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_x;
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_y << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_y;
    value ^= ((uint32_t)(uint16_t)mpu_data->accel_z << 16) |
             (uint32_t)(uint16_t)mpu_data->gyro_z;
    game->rng_state = MicroColony_Hash(value);
}

static uint16_t MicroColony_Index(uint8_t column,
                                  uint8_t row) {
    return (uint16_t)(
        ((uint16_t)row * MICRO_COLONY_COLUMNS) + column);
}

static uint8_t MicroColony_GetBit(const uint8_t *bits,
                                  uint16_t index) {
    return (uint8_t)(
        (bits[index >> 3] >> (index & 7U)) & 1U);
}

static void MicroColony_SetBit(uint8_t *bits,
                               uint16_t index,
                               uint8_t value) {
    uint8_t mask = (uint8_t)(1U << (index & 7U));

    if (value != 0U) {
        bits[index >> 3] |= mask;
    } else {
        bits[index >> 3] &= (uint8_t)~mask;
    }
}

static void MicroColony_MarkDirty(MicroColonyState *game,
                                  uint8_t column,
                                  uint8_t row) {
    MicroColony_SetBit(
        game->dirty,
        MicroColony_Index(column, row),
        1U);
}

uint8_t MicroColony_FoodAt(const MicroColonyState *game,
                           uint8_t column,
                           uint8_t row) {
    uint16_t index = MicroColony_Index(column, row);
    uint8_t shift = (uint8_t)((index & 3U) * 2U);

    return (uint8_t)((game->food[index >> 2] >> shift) & 0x03U);
}

static void MicroColony_SetFood(MicroColonyState *game,
                                uint8_t column,
                                uint8_t row,
                                uint8_t level) {
    uint16_t index = MicroColony_Index(column, row);
    uint8_t shift = (uint8_t)((index & 3U) * 2U);
    uint8_t mask = (uint8_t)(0x03U << shift);
    uint8_t previous =
        (uint8_t)((game->food[index >> 2] >> shift) & 0x03U);

    if (level > 3U) {
        level = 3U;
    }
    if (level == previous) {
        return;
    }

    game->food_total =
        (uint16_t)(game->food_total - previous + level);
    game->food[index >> 2] =
        (uint8_t)((game->food[index >> 2] & (uint8_t)~mask) |
                  (uint8_t)(level << shift));
    MicroColony_MarkDirty(game, column, row);
}

static void MicroColony_AddEnergy(MicroColonyBot *bot,
                                  uint16_t energy) {
    uint32_t sum = (uint32_t)bot->energy + energy;

    bot->energy =
        (sum > MICRO_COLONY_MAX_ENERGY) ?
        MICRO_COLONY_MAX_ENERGY : (uint16_t)sum;
}

static uint8_t MicroColony_SpendEnergy(MicroColonyBot *bot,
                                       uint16_t cost) {
    if (bot->energy <= cost) {
        bot->energy = 0U;
        return 0U;
    }
    bot->energy = (uint16_t)(bot->energy - cost);
    return 1U;
}

static uint8_t MicroColony_Left(uint8_t direction) {
    return (uint8_t)((direction + 3U) & 3U);
}

static uint8_t MicroColony_Right(uint8_t direction) {
    return (uint8_t)((direction + 1U) & 3U);
}

static void MicroColony_Neighbor(uint8_t column,
                                 uint8_t row,
                                 uint8_t direction,
                                 uint8_t *next_column,
                                 uint8_t *next_row) {
    *next_column = column;
    *next_row = row;

    switch (direction & 3U) {
        case 0U:
            *next_row = (row == 0U) ?
                        (MICRO_COLONY_ROWS - 1U) :
                        (uint8_t)(row - 1U);
            break;
        case 1U:
            *next_column =
                (uint8_t)((column + 1U) %
                          MICRO_COLONY_COLUMNS);
            break;
        case 2U:
            *next_row =
                (uint8_t)((row + 1U) %
                          MICRO_COLONY_ROWS);
            break;
        default:
            *next_column = (column == 0U) ?
                           (MICRO_COLONY_COLUMNS - 1U) :
                           (uint8_t)(column - 1U);
            break;
    }
}

const MicroColonyBot *MicroColony_BotAt(
    const MicroColonyState *game,
    uint8_t column,
    uint8_t row) {
    for (uint8_t i = 0U; i < MICRO_COLONY_MAX_BOTS; i++) {
        const MicroColonyBot *bot = &game->bots[i];

        if ((bot->alive != 0U) &&
            (bot->x == column) &&
            (bot->y == row)) {
            return bot;
        }
    }
    return 0;
}

static uint8_t MicroColony_CellOccupiedExcept(
    const MicroColonyState *game,
    uint8_t column,
    uint8_t row,
    uint8_t ignored_index) {
    for (uint8_t i = 0U; i < MICRO_COLONY_MAX_BOTS; i++) {
        const MicroColonyBot *bot = &game->bots[i];

        if ((i != ignored_index) &&
            (bot->alive != 0U) &&
            (bot->x == column) &&
            (bot->y == row)) {
            return 1U;
        }
    }
    return 0U;
}

static int16_t MicroColony_ScaleFood(uint8_t food) {
    return (int16_t)((uint16_t)food * 22U);
}

static int16_t MicroColony_ScaleEnergy(uint16_t energy) {
    int32_t value =
        ((int32_t)energy * 128L) /
        MICRO_COLONY_MAX_ENERGY;

    return (int16_t)(value - 64L);
}

static int16_t MicroColony_ScaleAge(
    const MicroColonyBot *bot) {
    int32_t value;

    if (bot->max_age == 0U) {
        return 0;
    }
    value = ((int32_t)bot->age * 128L) / bot->max_age;
    if (value > 128L) {
        value = 128L;
    }
    return (int16_t)(value - 64L);
}

static void MicroColony_BuildInputs(
    MicroColonyState *game,
    const MicroColonyBot *bot,
    uint8_t bot_index,
    int16_t inputs[MICRO_COLONY_BRAIN_INPUTS]) {
    uint8_t forward_x;
    uint8_t forward_y;
    uint8_t left_x;
    uint8_t left_y;
    uint8_t right_x;
    uint8_t right_y;

    MicroColony_Neighbor(bot->x, bot->y, bot->direction,
                         &forward_x, &forward_y);
    MicroColony_Neighbor(bot->x, bot->y,
                         MicroColony_Left(bot->direction),
                         &left_x, &left_y);
    MicroColony_Neighbor(bot->x, bot->y,
                         MicroColony_Right(bot->direction),
                         &right_x, &right_y);

    inputs[0] = 48;
    inputs[1] = MicroColony_ScaleFood(
        MicroColony_FoodAt(game, bot->x, bot->y));
    inputs[2] = MicroColony_ScaleFood(
        MicroColony_FoodAt(game, forward_x, forward_y));
    inputs[3] = MicroColony_ScaleFood(
        MicroColony_FoodAt(game, left_x, left_y));
    inputs[4] = MicroColony_ScaleFood(
        MicroColony_FoodAt(game, right_x, right_y));
    inputs[5] =
        (MicroColony_CellOccupiedExcept(
             game, forward_x, forward_y, bot_index) != 0U) ?
        64 : -32;
    inputs[6] = MicroColony_ScaleEnergy(bot->energy);
    inputs[7] = MicroColony_ScaleAge(bot);
    inputs[8] = (int16_t)((int32_t)(MicroColony_Random(game) &
                                    0x7FU) - 64L);
}

static MicroColonyOutput MicroColony_ChooseAction(
    MicroColonyState *game,
    const MicroColonyBot *bot,
    uint8_t bot_index) {
    int16_t inputs[MICRO_COLONY_BRAIN_INPUTS];
    int32_t scores[MICRO_COLONY_BRAIN_OUTPUTS];
    int32_t best_score = INT_MIN;
    uint8_t best_action = MICRO_COLONY_OUTPUT_LEFT;
    uint8_t forward_x;
    uint8_t forward_y;
    uint8_t food_here;

    MicroColony_BuildInputs(game, bot, bot_index, inputs);
    MicroColony_Neighbor(bot->x, bot->y, bot->direction,
                         &forward_x, &forward_y);
    food_here = MicroColony_FoodAt(game, bot->x, bot->y);

    for (uint8_t output = 0U;
         output < MICRO_COLONY_BRAIN_OUTPUTS;
         output++) {
        int32_t score = 0L;

        for (uint8_t input = 0U;
             input < MICRO_COLONY_BRAIN_INPUTS;
             input++) {
            score +=
                (int32_t)bot->weights[output][input] *
                inputs[input];
        }
        scores[output] = score;
    }

    for (uint8_t output = 0U;
         output < MICRO_COLONY_BRAIN_OUTPUTS;
         output++) {
        uint8_t valid = 1U;

        if ((output == MICRO_COLONY_OUTPUT_MOVE) &&
            (MicroColony_CellOccupiedExcept(
                 game, forward_x, forward_y,
                 bot_index) != 0U)) {
            valid = 0U;
        }
        if ((output == MICRO_COLONY_OUTPUT_EAT) &&
            (food_here == 0U)) {
            valid = 0U;
        }
        if ((output == MICRO_COLONY_OUTPUT_SPLIT) &&
            (bot->energy < MICRO_COLONY_BIRTH_ENERGY)) {
            valid = 0U;
        }
        if ((valid != 0U) && (scores[output] > best_score)) {
            best_score = scores[output];
            best_action = output;
        }
    }

    return (MicroColonyOutput)best_action;
}

static uint8_t MicroColony_FreeSlot(
    const MicroColonyState *game) {
    for (uint8_t i = 0U; i < MICRO_COLONY_MAX_BOTS; i++) {
        if (game->bots[i].alive == 0U) {
            return i;
        }
    }
    return 0xFFU;
}

static uint8_t MicroColony_FindFreeNeighbor(
    const MicroColonyState *game,
    const MicroColonyBot *bot,
    uint8_t parent_index,
    uint8_t *child_x,
    uint8_t *child_y) {
    const uint8_t directions[4] = {
        0U, 3U, 1U, 2U
    };

    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t direction =
            (uint8_t)((bot->direction + directions[i]) & 3U);

        MicroColony_Neighbor(bot->x, bot->y, direction,
                             child_x, child_y);
        if (MicroColony_CellOccupiedExcept(
                game, *child_x, *child_y,
                parent_index) == 0U) {
            return 1U;
        }
    }
    return 0U;
}

static int8_t MicroColony_ClampWeight(int16_t value) {
    if (value < -24) {
        return -24;
    }
    if (value > 24) {
        return 24;
    }
    return (int8_t)value;
}

static void MicroColony_MutateChild(MicroColonyState *game,
                                    MicroColonyBot *child) {
    uint8_t mutations = 1U;

    if ((MicroColony_Random(game) & 0x03U) == 0U) {
        mutations++;
    }
    if ((MicroColony_Random(game) & 0x0FU) == 0U) {
        mutations++;
    }

    for (uint8_t i = 0U; i < mutations; i++) {
        uint8_t output =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_BRAIN_OUTPUTS);
        uint8_t input =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_BRAIN_INPUTS);
        int16_t delta =
            (int16_t)((int32_t)(MicroColony_Random(game) %
                                9U) - 4L);

        if (delta == 0) {
            delta = 1;
        }
        child->weights[output][input] =
            MicroColony_ClampWeight(
                (int16_t)child->weights[output][input] +
                delta);
        child->hue =
            (uint8_t)(child->hue +
                      (uint8_t)((delta > 0) ?
                                delta : -delta));
        child->mutation =
            (child->mutation < 250U) ?
            (uint8_t)(child->mutation + 1U) : 255U;
    }

    if ((MicroColony_Random(game) & 0x07U) == 0U) {
        int16_t age_delta =
            (int16_t)((int32_t)(MicroColony_Random(game) %
                                41U) - 20L);
        int16_t new_age =
            (int16_t)child->max_age + age_delta;

        if (new_age < 180) {
            new_age = 180;
        } else if (new_age > 620) {
            new_age = 620;
        }
        child->max_age = (uint16_t)new_age;
    }
}

static void MicroColony_RandomBrain(MicroColonyState *game,
                                    MicroColonyBot *bot) {
    for (uint8_t output = 0U;
         output < MICRO_COLONY_BRAIN_OUTPUTS;
         output++) {
        for (uint8_t input = 0U;
             input < MICRO_COLONY_BRAIN_INPUTS;
             input++) {
            bot->weights[output][input] =
                (int8_t)((int32_t)(MicroColony_Random(game) %
                                   25U) - 12L);
        }
    }

    bot->weights[MICRO_COLONY_OUTPUT_EAT][1] += 12;
    bot->weights[MICRO_COLONY_OUTPUT_MOVE][2] += 8;
    bot->weights[MICRO_COLONY_OUTPUT_LEFT][3] += 6;
    bot->weights[MICRO_COLONY_OUTPUT_RIGHT][4] += 6;
    bot->weights[MICRO_COLONY_OUTPUT_SPLIT][6] += 10;
    bot->weights[MICRO_COLONY_OUTPUT_SPLIT][7] -= 6;
}

static void MicroColony_SpawnFounder(
    MicroColonyState *game,
    uint8_t index) {
    MicroColonyBot *bot = &game->bots[index];
    uint8_t tries = 80U;

    memset(bot, 0, sizeof(*bot));
    do {
        bot->x =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_COLUMNS);
        bot->y =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_ROWS);
        tries--;
    } while ((tries > 0U) &&
             (MicroColony_BotAt(game, bot->x, bot->y) != 0));

    bot->direction =
        (uint8_t)(MicroColony_Random(game) & 3U);
    bot->hue = (uint8_t)(MicroColony_Random(game) & 0xFFU);
    bot->energy = MICRO_COLONY_START_ENERGY;
    bot->max_age =
        (uint16_t)(260U +
                   (MicroColony_Random(game) % 180U));
    bot->alive = 1U;
    MicroColony_RandomBrain(game, bot);
    game->population++;
    MicroColony_MarkDirty(game, bot->x, bot->y);
}

static void MicroColony_SeedFood(MicroColonyState *game) {
    memset(game->food, 0, sizeof(game->food));
    game->food_total = 0U;

    for (uint8_t row = 0U; row < MICRO_COLONY_ROWS; row++) {
        for (uint8_t column = 0U;
             column < MICRO_COLONY_COLUMNS;
             column++) {
            uint8_t roll =
                (uint8_t)(MicroColony_Random(game) & 0xFFU);
            uint8_t level = 0U;

            if (roll < 80U) {
                level = 1U;
            }
            if (roll < 24U) {
                level = 2U;
            }
            if (roll < 5U) {
                level = 3U;
            }
            MicroColony_SetFood(game, column, row, level);
        }
    }
}

static void MicroColony_SeedWorld(MicroColonyState *game,
                                  uint8_t keep_counters) {
    uint32_t births = game->births;
    uint32_t deaths = game->deaths;
    uint32_t extinctions = game->extinctions;

    memset(game->bots, 0, sizeof(game->bots));
    memset(game->dirty, 0xFF, sizeof(game->dirty));
    game->population = 0U;
    game->active_bot = 0U;
    game->extinction_pause = 0U;
    if (keep_counters == 0U) {
        game->ticks = 0U;
        game->births = 0U;
        game->deaths = 0U;
        game->extinctions = 0U;
    } else {
        game->births = births;
        game->deaths = deaths;
        game->extinctions = extinctions;
    }

    MicroColony_SeedFood(game);
    for (uint8_t i = 0U; i < MICRO_COLONY_INITIAL_BOTS; i++) {
        MicroColony_SpawnFounder(game, i);
    }
}

static void MicroColony_GrowFood(MicroColonyState *game) {
    if ((game->ticks % MICRO_COLONY_FOOD_GROWTH_TICKS) != 0U) {
        return;
    }

    for (uint8_t i = 0U;
         i < MICRO_COLONY_FOOD_GROWTH_CELLS;
         i++) {
        uint8_t column =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_COLUMNS);
        uint8_t row =
            (uint8_t)(MicroColony_Random(game) %
                      MICRO_COLONY_ROWS);
        uint8_t level =
            MicroColony_FoodAt(game, column, row);

        if (level < 3U) {
            MicroColony_SetFood(
                game, column, row, (uint8_t)(level + 1U));
        }
    }
}

static void MicroColony_KillBot(MicroColonyState *game,
                                MicroColonyBot *bot) {
    if (bot->alive == 0U) {
        return;
    }
    MicroColony_MarkDirty(game, bot->x, bot->y);
    bot->alive = 0U;
    if (game->population > 0U) {
        game->population--;
    }
    game->deaths++;
    if ((game->population == 0U) &&
        (game->extinction_pause == 0U)) {
        game->extinction_pause = MICRO_COLONY_EXTINCTION_PAUSE;
        game->extinctions++;
    }
}

static void MicroColony_MoveBot(MicroColonyState *game,
                                MicroColonyBot *bot,
                                uint8_t bot_index) {
    uint8_t next_x;
    uint8_t next_y;

    MicroColony_Neighbor(bot->x, bot->y, bot->direction,
                         &next_x, &next_y);
    if (MicroColony_CellOccupiedExcept(
            game, next_x, next_y, bot_index) != 0U) {
        MicroColony_SpendEnergy(bot, MICRO_COLONY_BLOCKED_COST);
        bot->direction =
            (MicroColony_Random(game) & 1U) ?
            MicroColony_Left(bot->direction) :
            MicroColony_Right(bot->direction);
        return;
    }

    MicroColony_MarkDirty(game, bot->x, bot->y);
    bot->x = next_x;
    bot->y = next_y;
    MicroColony_MarkDirty(game, bot->x, bot->y);
    MicroColony_SpendEnergy(bot, MICRO_COLONY_MOVE_COST);
}

static void MicroColony_Eat(MicroColonyState *game,
                            MicroColonyBot *bot) {
    uint8_t level =
        MicroColony_FoodAt(game, bot->x, bot->y);

    if (level == 0U) {
        MicroColony_SpendEnergy(bot, MICRO_COLONY_TURN_COST);
        return;
    }

    MicroColony_SetFood(game, bot->x, bot->y,
                        (uint8_t)(level - 1U));
    MicroColony_AddEnergy(bot, MICRO_COLONY_EAT_GAIN);
}

static void MicroColony_Split(MicroColonyState *game,
                              MicroColonyBot *bot,
                              uint8_t bot_index) {
    uint8_t slot;
    uint8_t child_x;
    uint8_t child_y;
    MicroColonyBot *child;

    if (bot->energy <
        (MICRO_COLONY_CHILD_ENERGY +
         MICRO_COLONY_BIRTH_COST)) {
        MicroColony_SpendEnergy(bot, MICRO_COLONY_TURN_COST);
        return;
    }

    slot = MicroColony_FreeSlot(game);
    if ((slot == 0xFFU) ||
        (MicroColony_FindFreeNeighbor(
             game, bot, bot_index,
             &child_x, &child_y) == 0U)) {
        MicroColony_SpendEnergy(bot, MICRO_COLONY_BLOCKED_COST);
        return;
    }

    child = &game->bots[slot];
    *child = *bot;
    child->x = child_x;
    child->y = child_y;
    child->age = 0U;
    child->generation =
        (child->generation < 65535U) ?
        (uint16_t)(child->generation + 1U) : 65535U;
    child->energy = MICRO_COLONY_CHILD_ENERGY;
    child->direction =
        (uint8_t)(MicroColony_Random(game) & 3U);
    MicroColony_MutateChild(game, child);

    MicroColony_SpendEnergy(
        bot,
        MICRO_COLONY_CHILD_ENERGY +
        MICRO_COLONY_BIRTH_COST);
    game->population++;
    game->births++;
    MicroColony_MarkDirty(game, bot->x, bot->y);
    MicroColony_MarkDirty(game, child->x, child->y);
}

static void MicroColony_ProcessBot(MicroColonyState *game,
                                   uint8_t index) {
    MicroColonyBot *bot = &game->bots[index];
    MicroColonyOutput action;

    if (bot->alive == 0U) {
        return;
    }

    bot->age++;
    if ((MicroColony_SpendEnergy(
             bot, MICRO_COLONY_BASAL_COST) == 0U) ||
        (bot->age >= bot->max_age)) {
        MicroColony_KillBot(game, bot);
        return;
    }

    action = MicroColony_ChooseAction(game, bot, index);
    switch (action) {
        case MICRO_COLONY_OUTPUT_MOVE:
            MicroColony_MoveBot(game, bot, index);
            break;
        case MICRO_COLONY_OUTPUT_RIGHT:
            bot->direction = MicroColony_Right(bot->direction);
            MicroColony_SpendEnergy(bot,
                                    MICRO_COLONY_TURN_COST);
            break;
        case MICRO_COLONY_OUTPUT_EAT:
            MicroColony_Eat(game, bot);
            break;
        case MICRO_COLONY_OUTPUT_SPLIT:
            MicroColony_Split(game, bot, index);
            break;
        case MICRO_COLONY_OUTPUT_LEFT:
        default:
            bot->direction = MicroColony_Left(bot->direction);
            MicroColony_SpendEnergy(bot,
                                    MICRO_COLONY_TURN_COST);
            break;
    }

    if ((bot->alive != 0U) &&
        ((bot->energy == 0U) || (bot->age >= bot->max_age))) {
        MicroColony_KillBot(game, bot);
        return;
    }
    if (bot->alive != 0U) {
        MicroColony_MarkDirty(game, bot->x, bot->y);
    }
}

void MicroColony_Init(MicroColonyState *game, uint32_t seed) {
    memset(game, 0, sizeof(*game));
    game->rng_state =
        MicroColony_Hash(
            ((seed != 0U) ? seed : 0xC0910A11U) ^
            HAL_GetTick());
    MicroColony_SeedWorld(game, 0U);
}

MicroColonyEvent MicroColony_Update(MicroColonyState *game,
                                    const MPU6000_Data_t *mpu_data,
                                    HAL_StatusTypeDef mpu_status) {
    MicroColonyEvent event =
        MICRO_COLONY_EVENT_CELLS;

    game->ticks++;
    if ((game->ticks & 0x07U) == 0U) {
        MicroColony_MixSensor(game, mpu_data, mpu_status);
    }

    if (game->population == 0U) {
        if (game->extinction_pause > 0U) {
            game->extinction_pause--;
            return MICRO_COLONY_EVENT_HUD;
        }
        game->rng_state =
            MicroColony_Hash(game->rng_state ^ HAL_GetTick());
        MicroColony_SeedWorld(game, 1U);
        return MICRO_COLONY_EVENT_FULL_REDRAW |
               MICRO_COLONY_EVENT_HUD;
    }

    MicroColony_GrowFood(game);
    for (uint8_t i = 0U; i < MICRO_COLONY_BOTS_PER_TICK; i++) {
        MicroColony_ProcessBot(game, game->active_bot);
        game->active_bot =
            (uint8_t)((game->active_bot + 1U) %
                      MICRO_COLONY_MAX_BOTS);
    }

    game->hud_divider++;
    if (game->hud_divider >= 8U) {
        game->hud_divider = 0U;
        event |= MICRO_COLONY_EVENT_HUD;
    }
    return event;
}

uint8_t MicroColony_CellDirty(const MicroColonyState *game,
                              uint8_t column,
                              uint8_t row) {
    return MicroColony_GetBit(
        game->dirty,
        MicroColony_Index(column, row));
}

void MicroColony_ClearDirty(MicroColonyState *game) {
    memset(game->dirty, 0, sizeof(game->dirty));
}
