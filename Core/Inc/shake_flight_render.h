#ifndef INC_SHAKE_FLIGHT_RENDER_H_
#define INC_SHAKE_FLIGHT_RENDER_H_

#include "shake_flight_logic.h"

void ShakeFlightRender_Init(const ShakeFlightState *game);
void ShakeFlightRender_Frame(const ShakeFlightState *game,
                             ShakeFlightEvent event);

#endif /* INC_SHAKE_FLIGHT_RENDER_H_ */
