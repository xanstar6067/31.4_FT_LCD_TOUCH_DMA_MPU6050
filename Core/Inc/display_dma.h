#ifndef INC_DISPLAY_DMA_H_
#define INC_DISPLAY_DMA_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

extern volatile uint8_t display_dma_transfer_complete;

void Display_DMA_MarkBusy(void);
void Display_DMA_WaitForTransfer(void);

#endif /* INC_DISPLAY_DMA_H_ */
