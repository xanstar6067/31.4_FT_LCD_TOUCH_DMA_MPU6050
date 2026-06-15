#ifndef INC_ORB_HUNT_RENDER_H_
#define INC_ORB_HUNT_RENDER_H_

#include "orb_hunt_logic.h"

void OrbHuntRender_Init(const OrbHuntState *game);
void OrbHuntRender_Frame(const OrbHuntState *game, OrbHuntEvent event);

#endif /* INC_ORB_HUNT_RENDER_H_ */
