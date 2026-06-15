#ifndef INC_SYSTEM_INFO_PAGE_H_
#define INC_SYSTEM_INFO_PAGE_H_

#include <stdint.h>
#include "w25qxx.h"

typedef struct {
    W25QxxInfo external_flash;
    uint32_t system_clock_hz;
    uint32_t spi1_clock_hz;
    uint32_t mcu_uid[3];
    uint16_t internal_flash_kb;
    uint16_t device_id;
    uint16_t revision_id;
} SystemInfoPage;

void SystemInfoPage_Init(SystemInfoPage *page);
void SystemInfoPage_Refresh(SystemInfoPage *page);
void SystemInfoPage_Render(const SystemInfoPage *page);

#endif /* INC_SYSTEM_INFO_PAGE_H_ */
