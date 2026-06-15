#ifndef INC_BALANCE_TOWER_RENDER_H_
#define INC_BALANCE_TOWER_RENDER_H_

#include "balance_tower_logic.h"

void BalanceTowerRender_Init(const BalanceTowerState *game);
void BalanceTowerRender_Frame(const BalanceTowerState *game,
                              BalanceTowerEvent event);

#endif /* INC_BALANCE_TOWER_RENDER_H_ */
