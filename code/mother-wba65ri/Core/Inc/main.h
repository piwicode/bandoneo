/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32wbaxx_hal.h"

#include "stm32wbaxx_nucleo.h"

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
#define VCP1_TX_Pin GPIO_PIN_12
#define VCP1_TX_GPIO_Port GPIOB
#define VCP1_RX_Pin GPIO_PIN_8
#define VCP1_RX_GPIO_Port GPIOA
#define USB_FS_N_Pin GPIO_PIN_7
#define USB_FS_N_GPIO_Port GPIOD
#define USB_FS_P_Pin GPIO_PIN_6
#define USB_FS_P_GPIO_Port GPIOD
#define OSC32_OUT_Pin GPIO_PIN_15
#define OSC32_OUT_GPIO_Port GPIOC
#define OSC32_IN_Pin GPIO_PIN_14
#define OSC32_IN_GPIO_Port GPIOC
#define JTDO_SWO_Pin GPIO_PIN_3
#define JTDO_SWO_GPIO_Port GPIOB
#define VCP2_RTS_Pin GPIO_PIN_15
#define VCP2_RTS_GPIO_Port GPIOA
#define JTCLK_SWCLK_Pin GPIO_PIN_14
#define JTCLK_SWCLK_GPIO_Port GPIOA
#define JTMS_SWDIO_Pin GPIO_PIN_13
#define JTMS_SWDIO_GPIO_Port GPIOA
#define VCP2_TX_Pin GPIO_PIN_12
#define VCP2_TX_GPIO_Port GPIOA
#define VCP2_RX_Pin GPIO_PIN_11
#define VCP2_RX_GPIO_Port GPIOA
#define VCP2_CTS_Pin GPIO_PIN_15
#define VCP2_CTS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
