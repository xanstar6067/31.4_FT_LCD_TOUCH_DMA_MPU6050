#ifndef INC_LIFE_GAME_RENDER_H_
#define INC_LIFE_GAME_RENDER_H_

#include "life_game_logic.h"

void LifeGameRender_Init(LifeGameState *game);
void LifeGameRender_Frame(LifeGameState *game,
                          LifeGameEvent event);

#endif /* INC_LIFE_GAME_RENDER_H_ */
