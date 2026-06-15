#ifndef INC_ARCADE_COLLECTION_H_
#define INC_ARCADE_COLLECTION_H_

#include <stdint.h>
#include "gravity_pong_logic.h"
#include "orb_hunt_logic.h"
#include "system_info_page.h"
#include "tilt_breaker_logic.h"

typedef enum {
    ARCADE_PAGE_SYSTEM_INFO = 0,
    ARCADE_PAGE_ORB_HUNT,
    ARCADE_PAGE_GRAVITY_PONG,
    ARCADE_PAGE_TILT_BREAKER,
    ARCADE_PAGE_COUNT
} ArcadePageId;

typedef struct {
    ArcadePageId active_page;
    uint32_t seed;
    SystemInfoPage system_info;
    OrbHuntState orb_hunt;
    GravityPongState gravity_pong;
    TiltBreakerState tilt_breaker;
} ArcadeCollection;

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed);
void ArcadeCollection_Update(ArcadeCollection *arcade,
                             int16_t accel_x,
                             int16_t accel_y);
void ArcadeCollection_NextPage(ArcadeCollection *arcade);
void ArcadeCollection_RestartActive(ArcadeCollection *arcade);

#endif /* INC_ARCADE_COLLECTION_H_ */
