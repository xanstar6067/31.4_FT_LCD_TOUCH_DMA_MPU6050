#ifndef INC_SHAKE_FLIGHT_LOGIC_H_
#define INC_SHAKE_FLIGHT_LOGIC_H_

#include <stdint.h>

#define SHAKE_FLIGHT_SCREEN_WIDTH        320
#define SHAKE_FLIGHT_SCREEN_HEIGHT       240
#define SHAKE_FLIGHT_PLAYFIELD_TOP       28
#define SHAKE_FLIGHT_GROUND_Y            220

#define SHAKE_FLIGHT_BIRD_X              78
#define SHAKE_FLIGHT_BIRD_RADIUS         7
#define SHAKE_FLIGHT_PIPE_COUNT          3U
#define SHAKE_FLIGHT_PIPE_WIDTH          30
#define SHAKE_FLIGHT_PIPE_SPACING        112

typedef struct {
    float x;
    int16_t gap_y;
    uint8_t passed;
} ShakeFlightPipe;

typedef enum {
    SHAKE_FLIGHT_PHASE_PLAYING = 0,
    SHAKE_FLIGHT_PHASE_RECOVERY
} ShakeFlightPhase;

typedef struct {
    ShakeFlightPipe pipes[SHAKE_FLIGHT_PIPE_COUNT];
    uint32_t rng_state;
    uint32_t ticks;
    uint32_t score;
    float bird_y;
    float bird_vy;
    float filtered_shake;
    int16_t previous_accel_z;
    uint16_t recovery_ticks;
    uint16_t shake_cooldown;
    uint8_t level;
    uint8_t initialized_accel;
    ShakeFlightPhase phase;
} ShakeFlightState;

typedef uint8_t ShakeFlightEvent;

#define SHAKE_FLIGHT_EVENT_NONE          0x00U
#define SHAKE_FLIGHT_EVENT_HUD_CHANGED   0x01U
#define SHAKE_FLIGHT_EVENT_ROUND_STARTED 0x02U
#define SHAKE_FLIGHT_EVENT_CRASH         0x04U

void ShakeFlight_Init(ShakeFlightState *game, uint32_t seed);
ShakeFlightEvent ShakeFlight_Update(ShakeFlightState *game,
                                    int16_t accel_z);
uint8_t ShakeFlight_GapHeight(const ShakeFlightState *game);
float ShakeFlight_PipeSpeed(const ShakeFlightState *game);

#endif /* INC_SHAKE_FLIGHT_LOGIC_H_ */
