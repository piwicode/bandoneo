/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32g4xx_hal.h"

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
#define EXP_PEDAL_INT_Pin GPIO_PIN_14
#define EXP_PEDAL_INT_GPIO_Port GPIOC
#define SUS_PEDAL_INT_Pin GPIO_PIN_15
#define SUS_PEDAL_INT_GPIO_Port GPIOC
#define R_SPI_NSS_Pin GPIO_PIN_0
#define R_SPI_NSS_GPIO_Port GPIOF
#define R_SPI_SCK_Pin GPIO_PIN_1
#define R_SPI_SCK_GPIO_Port GPIOF
#define EXP_PEDAL_Pin GPIO_PIN_0
#define EXP_PEDAL_GPIO_Port GPIOA
#define SUS_PEDAL_Pin GPIO_PIN_1
#define SUS_PEDAL_GPIO_Port GPIOA
#define HALL_NEN_Pin GPIO_PIN_2
#define HALL_NEN_GPIO_Port GPIOA
#define L_BOOT0_Pin GPIO_PIN_3
#define L_BOOT0_GPIO_Port GPIOA
#define L_SPI_NSS_Pin GPIO_PIN_4
#define L_SPI_NSS_GPIO_Port GPIOA
#define L_SPI_SCK_Pin GPIO_PIN_5
#define L_SPI_SCK_GPIO_Port GPIOA
#define L_SPI_MISO_Pin GPIO_PIN_6
#define L_SPI_MISO_GPIO_Port GPIOA
#define L_SPI_MOSI_Pin GPIO_PIN_7
#define L_SPI_MOSI_GPIO_Port GPIOA
#define L_READY_Pin GPIO_PIN_0
#define L_READY_GPIO_Port GPIOB
#define HALL0_Pin GPIO_PIN_1
#define HALL0_GPIO_Port GPIOB
#define L_NRST_Pin GPIO_PIN_2
#define L_NRST_GPIO_Port GPIOB
#define R_NRST_Pin GPIO_PIN_11
#define R_NRST_GPIO_Port GPIOB
#define HALL1_Pin GPIO_PIN_12
#define HALL1_GPIO_Port GPIOB
#define R_BOOT0_Pin GPIO_PIN_13
#define R_BOOT0_GPIO_Port GPIOB
#define R_SPI_MISO_Pin GPIO_PIN_14
#define R_SPI_MISO_GPIO_Port GPIOB
#define R_SPI_MOSI_Pin GPIO_PIN_15
#define R_SPI_MOSI_GPIO_Port GPIOB
#define R_READY_Pin GPIO_PIN_8
#define R_READY_GPIO_Port GPIOA
#define USB_DM_Pin GPIO_PIN_11
#define USB_DM_GPIO_Port GPIOA
#define USB_DP_Pin GPIO_PIN_12
#define USB_DP_GPIO_Port GPIOA
#define PGM_DETECT_Pin GPIO_PIN_15
#define PGM_DETECT_GPIO_Port GPIOA
#define SW_FN2_Pin GPIO_PIN_4
#define SW_FN2_GPIO_Port GPIOB
#define LED_FN2_Pin GPIO_PIN_5
#define LED_FN2_GPIO_Port GPIOB
#define SW_FN1_Pin GPIO_PIN_6
#define SW_FN1_GPIO_Port GPIOB
#define LED_FN1_Pin GPIO_PIN_7
#define LED_FN1_GPIO_Port GPIOB
#define SW_FN0_BOOT0_Pin GPIO_PIN_8
#define SW_FN0_BOOT0_GPIO_Port GPIOB
#define LED_FN0_Pin GPIO_PIN_9
#define LED_FN0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
