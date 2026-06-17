#ifndef INC_BIPLANE_DUEL_RENDER_H_
#define INC_BIPLANE_DUEL_RENDER_H_

#include "biplane_duel_logic.h"

void BiplaneDuelRender_Init(const BiplaneDuelState *game);
void BiplaneDuelRender_Frame(const BiplaneDuelState *game,
                             BiplaneDuelEvent event);

#endif /* INC_BIPLANE_DUEL_RENDER_H_ */
