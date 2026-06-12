/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define UKEY_Pin GPIO_PIN_0
#define UKEY_GPIO_Port GPIOA
#define ELED1_Pin GPIO_PIN_1
#define ELED1_GPIO_Port GPIOA
#define ILI9341_TOUCH_IRQ_Pin GPIO_PIN_1
#define ILI9341_TOUCH_IRQ_GPIO_Port GPIOB
#define ILI9341_TOUCH_IRQ_EXTI_IRQn EXTI1_IRQn
#define ILI9341_DC_Pin GPIO_PIN_3
#define ILI9341_DC_GPIO_Port GPIOB
#define ILI9341_RES_Pin GPIO_PIN_4
#define ILI9341_RES_GPIO_Port GPIOB
#define ILI9341_CS_Pin GPIO_PIN_5
#define ILI9341_CS_GPIO_Port GPIOB
#define ILI9341_TOUCH_CS_Pin GPIO_PIN_9
#define ILI9341_TOUCH_CS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
