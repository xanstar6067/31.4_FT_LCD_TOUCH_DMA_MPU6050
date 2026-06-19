#include "display_dma.h"

volatile uint8_t display_dma_transfer_complete = 1U;

void Display_DMA_MarkBusy(void) {
    display_dma_transfer_complete = 0U;
}

void Display_DMA_WaitForTransfer(void) {
    while (display_dma_transfer_complete == 0U) {
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if ((hspi != NULL) && (hspi->Instance == SPI1)) {
        display_dma_transfer_complete = 1U;
    }
}
