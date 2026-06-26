#ifndef INC_COMET_CATCH_LOGIC_H_
#define INC_COMET_CATCH_LOGIC_H_

#include <stdint.h>

#define COMET_CATCH_SCREEN_WIDTH      320
#define COMET_CATCH_SCREEN_HEIGHT     240
#define COMET_CATCH_PLAYFIELD_TOP      28

#define COMET_CATCH_MAX_OBJECTS        10U
#define COMET_CATCH_CATCHER_WIDTH      48
#define COMET_CATCH_CATCHER_HEIGHT     14
#define COMET_CATCH_STARTING_LIVES      3U
#define COMET_CATCH_ENERGY_MAX        100U

typedef enum {
    COMET_CATCH_OBJECT_STAR = 0,
    COMET_CATCH_OBJECT_SPARK
} CometCatchObjectType;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t radius;
    uint8_t type;
    uint8_t style;
    uint8_t active;
} CometCatchObject;

typedef enum {
    COMET_CATCH_PHASE_PLAYING = 0,
    COMET_CATCH_PHASE_RESTART_PAUSE
} CometCatchPhase;

typedef struct {
    CometCatchObject objects[COMET_CATCH_MAX_OBJECTS];
    uint32_t rng_state;
    uint32_t background_seed;
    uint32_t score;
    uint32_t ticks;
    float catcher_x;
    float catcher_vx;
    float filtered_accel_x;
    uint16_t phase_ticks;
    uint8_t spawn_ticks;
    uint8_t lives;
    uint8_t level;
    uint8_t combo;
    uint8_t best_combo;
    uint8_t pulse_energy;
    uint8_t pulse_active;
    uint8_t invulnerable_ticks;
    CometCatchPhase phase;
} CometCatchState;

typedef uint8_t CometCatchEvent;

#define COMET_CATCH_EVENT_NONE            0x00U
#define COMET_CATCH_EVENT_OBJECTS         0x01U
#define COMET_CATCH_EVENT_HUD             0x02U
#define COMET_CATCH_EVENT_PULSE_CHANGED   0x04U
#define COMET_CATCH_EVENT_GAME_STARTED    0x08U
#define COMET_CATCH_EVENT_RESTART_PAUSE   0x10U

void CometCatch_Init(CometCatchState *game, uint32_t seed);
CometCatchEvent CometCatch_Update(CometCatchState *game,
                                  int16_t accel_x,
                                  uint8_t touch_pressed);

#endif /* INC_COMET_CATCH_LOGIC_H_ */
