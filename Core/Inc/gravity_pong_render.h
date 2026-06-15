#ifndef INC_GRAVITY_PONG_RENDER_H_
#define INC_GRAVITY_PONG_RENDER_H_

#include "gravity_pong_logic.h"

void GravityPongRender_Init(const GravityPongState *game);
void GravityPongRender_Frame(const GravityPongState *game,
                             GravityPongEvent event);

#endif /* INC_GRAVITY_PONG_RENDER_H_ */
