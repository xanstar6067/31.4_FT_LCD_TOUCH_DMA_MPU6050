#ifndef INC_MARBLE_MAZE_RENDER_H_
#define INC_MARBLE_MAZE_RENDER_H_

#include "marble_maze_logic.h"

void MarbleMazeRender_Init(const MarbleMazeState *game);
void MarbleMazeRender_Frame(const MarbleMazeState *game,
                            MarbleMazeEvent event);

#endif /* INC_MARBLE_MAZE_RENDER_H_ */
