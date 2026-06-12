#ifndef INC_GAME_RENDER_H_
#define INC_GAME_RENDER_H_

#include "game_logic.h"

void Render_Init(const GameState *game);
void Render_Frame(const GameState *game, GameEvent event);

#endif /* INC_GAME_RENDER_H_ */
