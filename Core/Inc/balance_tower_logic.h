#ifndef INC_BALANCE_TOWER_LOGIC_H_
#define INC_BALANCE_TOWER_LOGIC_H_

#include <stdint.h>

#define BALANCE_TOWER_SCREEN_WIDTH       320
#define BALANCE_TOWER_SCREEN_HEIGHT      240
#define BALANCE_TOWER_PLAYFIELD_TOP      28

#define BALANCE_TOWER_PLATFORM_CENTER_X  160
#define BALANCE_TOWER_PLATFORM_Y         211
#define BALANCE_TOWER_PLATFORM_WIDTH     150
#define BALANCE_TOWER_PLATFORM_THICKNESS 5

#define BALANCE_TOWER_MAX_BLOCKS         8U
#define BALANCE_TOWER_BLOCK_HEIGHT       16
#define BALANCE_TOWER_BLOCK_GAP          2

typedef enum {
    BALANCE_TOWER_PHASE_CALIBRATING = 0,
    BALANCE_TOWER_PHASE_PLAYING,
    BALANCE_TOWER_PHASE_RECOVERY
} BalanceTowerPhase;

typedef struct {
    uint32_t rng_state;
    uint32_t ticks;
    uint32_t score;
    float platform_angle;
    float platform_velocity;
    float tower_lean;
    float tower_velocity;
    float filtered_gyro;
    float gyro_bias_sum;
    float gyro_bias;
    float disturbance;
    uint16_t calibration_samples;
    uint16_t phase_ticks;
    uint16_t stable_ticks;
    uint8_t level;
    uint8_t block_count;
    uint8_t block_widths[BALANCE_TOWER_MAX_BLOCKS];
    uint8_t block_styles[BALANCE_TOWER_MAX_BLOCKS];
    int8_t block_offsets[BALANCE_TOWER_MAX_BLOCKS];
    BalanceTowerPhase phase;
} BalanceTowerState;

typedef uint8_t BalanceTowerEvent;

#define BALANCE_TOWER_EVENT_NONE              0x00U
#define BALANCE_TOWER_EVENT_HUD_CHANGED       0x01U
#define BALANCE_TOWER_EVENT_ROUND_STARTED     0x02U
#define BALANCE_TOWER_EVENT_LEVEL_STARTED     0x04U
#define BALANCE_TOWER_EVENT_RECOVERY_STARTED  0x08U

void BalanceTower_Init(BalanceTowerState *game, uint32_t seed);
BalanceTowerEvent BalanceTower_Update(BalanceTowerState *game,
                                      int16_t gyro_z);
void BalanceTower_GetBlockRect(const BalanceTowerState *game,
                               uint8_t index,
                               int16_t *x,
                               int16_t *y,
                               int16_t *width,
                               int16_t *height);

#endif /* INC_BALANCE_TOWER_LOGIC_H_ */
