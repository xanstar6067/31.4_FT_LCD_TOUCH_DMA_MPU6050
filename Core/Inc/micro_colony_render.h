#ifndef INC_MICRO_COLONY_RENDER_H_
#define INC_MICRO_COLONY_RENDER_H_

#include "micro_colony_logic.h"

void MicroColonyRender_Init(MicroColonyState *game);
void MicroColonyRender_Frame(MicroColonyState *game,
                             MicroColonyEvent event);

#endif /* INC_MICRO_COLONY_RENDER_H_ */
