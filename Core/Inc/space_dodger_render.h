#ifndef INC_SPACE_DODGER_RENDER_H_
#define INC_SPACE_DODGER_RENDER_H_

#include "space_dodger_logic.h"

void SpaceDodgerRender_Init(const SpaceDodgerState *game);
void SpaceDodgerRender_Frame(const SpaceDodgerState *game,
                             SpaceDodgerEvent event);

#endif /* INC_SPACE_DODGER_RENDER_H_ */
