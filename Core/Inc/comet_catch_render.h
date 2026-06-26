#ifndef INC_COMET_CATCH_RENDER_H_
#define INC_COMET_CATCH_RENDER_H_

#include "comet_catch_logic.h"

void CometCatchRender_Init(const CometCatchState *game);
void CometCatchRender_Frame(const CometCatchState *game,
                            CometCatchEvent event);

#endif /* INC_COMET_CATCH_RENDER_H_ */
