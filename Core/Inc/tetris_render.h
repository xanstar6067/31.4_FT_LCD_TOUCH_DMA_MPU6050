#ifndef INC_TETRIS_RENDER_H_
#define INC_TETRIS_RENDER_H_

#include "tetris_logic.h"

void TetrisRender_Init(TetrisState *game);
void TetrisRender_Frame(TetrisState *game,
                        TetrisEvent event);

#endif /* INC_TETRIS_RENDER_H_ */
