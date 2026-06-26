#include "system_info_page.h"

#include <stdio.h>
#include "display_driver.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

#define SYSTEM_INFO_BACKGROUND DISPLAY_COLOR565(4, 9, 16)
#define SYSTEM_INFO_PANEL      DISPLAY_COLOR565(10, 22, 34)

static uint32_t SystemInfoPage_GetSpiClock(void) {
    uint32_t divider = 2U;

    switch (hspi1.Init.BaudRatePrescaler) {
        case SPI_BAUDRATEPRESCALER_4:
            divider = 4U;
            break;
        case SPI_BAUDRATEPRESCALER_8:
            divider = 8U;
            break;
        case SPI_BAUDRATEPRESCALER_16:
            divider = 16U;
            break;
        case SPI_BAUDRATEPRESCALER_32:
            divider = 32U;
            break;
        case SPI_BAUDRATEPRESCALER_64:
            divider = 64U;
            break;
        case SPI_BAUDRATEPRESCALER_128:
            divider = 128U;
            break;
        case SPI_BAUDRATEPRESCALER_256:
            divider = 256U;
            break;
        default:
            break;
    }
    return HAL_RCC_GetPCLK2Freq() / divider;
}

static void SystemInfoPage_FormatFlashName(
    const W25QxxInfo *flash,
    char *text,
    size_t text_size) {
    uint32_t capacity_mbit =
        flash->capacity_bytes / (128U * 1024U);

    if (flash->result != W25QXX_RESULT_OK) {
        snprintf(text, text_size, "NOT FOUND ON PA4");
    } else if (flash->manufacturer_id == 0xEFU) {
        snprintf(text, text_size, "WINBOND W25Q%lu",
                 (unsigned long)capacity_mbit);
    } else {
        snprintf(text, text_size, "SPI NOR %lu MBIT",
                 (unsigned long)capacity_mbit);
    }
}

void SystemInfoPage_Refresh(SystemInfoPage *page) {
    page->system_clock_hz = HAL_RCC_GetHCLKFreq();
    page->spi1_clock_hz = SystemInfoPage_GetSpiClock();
    page->mcu_uid[0] = *(uint32_t *)(UID_BASE + 0U);
    page->mcu_uid[1] = *(uint32_t *)(UID_BASE + 4U);
    page->mcu_uid[2] = *(uint32_t *)(UID_BASE + 8U);
    page->internal_flash_kb = *(uint16_t *)FLASHSIZE_BASE;
    page->device_id =
        (uint16_t)(DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk);
    page->revision_id =
        (uint16_t)((DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID_Msk) >>
                   DBGMCU_IDCODE_REV_ID_Pos);
    W25Qxx_ReadInfo(&hspi1, &page->external_flash);
}

void SystemInfoPage_Init(SystemInfoPage *page) {
    SystemInfoPage_Refresh(page);
}

void SystemInfoPage_Render(const SystemInfoPage *page) {
    char text[48];
    const W25QxxInfo *flash = &page->external_flash;
    uint16_t flash_color =
        (flash->result == W25QXX_RESULT_OK) ?
        DISPLAY_GREEN : DISPLAY_RED;

    DISPLAY_FillScreen_DMA(SYSTEM_INFO_BACKGROUND);
    DISPLAY_FillRectangle_DMA(0, 0, 320, 40,
                              DISPLAY_DARK_BLUE);
    DISPLAY_WriteString_DMA(8, 7, "SYSTEM INFO", Font_16x26,
                            DISPLAY_WHITE, DISPLAY_DARK_BLUE);
    DISPLAY_WriteString_DMA(270, 15, "PAGE 0", Font_7x10,
                            DISPLAY_CYAN, DISPLAY_DARK_BLUE);

    DISPLAY_FillRectangle_DMA(6, 46, 308, 70,
                              SYSTEM_INFO_PANEL);
    DISPLAY_WriteString_DMA(12, 51, "WEACT BLACK PILL",
                            Font_11x18,
                            DISPLAY_CYAN, SYSTEM_INFO_PANEL);
    snprintf(text, sizeof(text),
             "STM32F411CEU6  CPU:%luMHZ  RAM:128KB",
             (unsigned long)(page->system_clock_hz / 1000000U));
    DISPLAY_WriteString_DMA(12, 74, text, Font_7x10,
                            DISPLAY_WHITE, SYSTEM_INFO_PANEL);
    snprintf(text, sizeof(text),
             "FLASH:%uKB  DEV:%03X  REV:%04X",
             (unsigned int)page->internal_flash_kb,
             (unsigned int)page->device_id,
             (unsigned int)page->revision_id);
    DISPLAY_WriteString_DMA(12, 88, text, Font_7x10,
                            DISPLAY_WHITE, SYSTEM_INFO_PANEL);
    snprintf(text, sizeof(text), "UID:%08lX-%08lX-%08lX",
             (unsigned long)page->mcu_uid[0],
             (unsigned long)page->mcu_uid[1],
             (unsigned long)page->mcu_uid[2]);
    DISPLAY_WriteString_DMA(12, 102, text, Font_7x10,
                            DISPLAY_GRAY, SYSTEM_INFO_PANEL);

    DISPLAY_FillRectangle_DMA(6, 122, 308, 82,
                              SYSTEM_INFO_PANEL);
    DISPLAY_WriteString_DMA(12, 127, "EXTERNAL SPI FLASH",
                            Font_11x18,
                            DISPLAY_YELLOW, SYSTEM_INFO_PANEL);
    SystemInfoPage_FormatFlashName(flash, text, sizeof(text));
    DISPLAY_WriteString_DMA(12, 149, text, Font_11x18,
                            flash_color, SYSTEM_INFO_PANEL);

    if (flash->result == W25QXX_RESULT_OK) {
        snprintf(text, sizeof(text),
                 "JEDEC:%02X %02X %02X  SIZE:%luKB",
                 flash->manufacturer_id,
                 flash->memory_type,
                 flash->capacity_id,
                 (unsigned long)(flash->capacity_bytes / 1024U));
        DISPLAY_WriteString_DMA(12, 171, text, Font_7x10,
                                DISPLAY_WHITE,
                                SYSTEM_INFO_PANEL);
        snprintf(text, sizeof(text),
                 "SR:%02X %02X %02X UID:%02X%02X%02X%02X%02X%02X%02X%02X",
                 flash->status_register_1,
                 flash->status_register_2,
                 flash->status_register_3,
                 flash->unique_id[0], flash->unique_id[1],
                 flash->unique_id[2], flash->unique_id[3],
                 flash->unique_id[4], flash->unique_id[5],
                 flash->unique_id[6], flash->unique_id[7]);
        DISPLAY_WriteString_DMA(12, 185, text, Font_7x10,
                                DISPLAY_GRAY,
                                SYSTEM_INFO_PANEL);
    } else {
        DISPLAY_WriteString_DMA(
            12, 176, "CHECK CS:PA4  MISO:PA6  POWER:3V3",
            Font_7x10, DISPLAY_ORANGE, SYSTEM_INFO_PANEL);
    }

    snprintf(text, sizeof(text),
             "SPI1:%luMHZ  TFT CS:PB5  FLASH CS:PA4",
             (unsigned long)(page->spi1_clock_hz / 1000000U));
    DISPLAY_WriteString_DMA(8, 211, text, Font_7x10,
                            DISPLAY_CYAN,
                            SYSTEM_INFO_BACKGROUND);
    DISPLAY_WriteString_DMA(
        8, 226, "UKEY SHORT:MENU   LONG:REFRESH",
        Font_7x10, DISPLAY_WHITE, SYSTEM_INFO_BACKGROUND);
}
