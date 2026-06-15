#ifndef INC_TILT_BREAKER_RENDER_H_
#define INC_TILT_BREAKER_RENDER_H_

#include "tilt_breaker_logic.h"

void TiltBreakerRender_Init(const TiltBreakerState *game);
void TiltBreakerRender_Frame(const TiltBreakerState *game,
                             TiltBreakerEvent event);

#endif /* INC_TILT_BREAKER_RENDER_H_ */
