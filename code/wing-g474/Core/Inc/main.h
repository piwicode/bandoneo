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
#define ID0_Pin GPIO_PIN_13
#define ID0_GPIO_Port GPIOC
#define SEL0_Pin GPIO_PIN_14
#define SEL0_GPIO_Port GPIOC
#define SEL1_Pin GPIO_PIN_15
#define SEL1_GPIO_Port GPIOC
#define ID1_Pin GPIO_PIN_0
#define ID1_GPIO_Port GPIOF
#define ID2_Pin GPIO_PIN_1
#define ID2_GPIO_Port GPIOF
#define A_IN0_Pin GPIO_PIN_0
#define A_IN0_GPIO_Port GPIOA
#define A_IN1_Pin GPIO_PIN_1
#define A_IN1_GPIO_Port GPIOA
#define SPI_NSS_Pin GPIO_PIN_4
#define SPI_NSS_GPIO_Port GPIOA
#define SPI_SCK_Pin GPIO_PIN_5
#define SPI_SCK_GPIO_Port GPIOA
#define A_IN2_Pin GPIO_PIN_6
#define A_IN2_GPIO_Port GPIOA
#define A_IN3_Pin GPIO_PIN_7
#define A_IN3_GPIO_Port GPIOA
#define A_IN4_Pin GPIO_PIN_0
#define A_IN4_GPIO_Port GPIOB
#define A_IN5_Pin GPIO_PIN_1
#define A_IN5_GPIO_Port GPIOB
#define HALL_NEN_Pin GPIO_PIN_11
#define HALL_NEN_GPIO_Port GPIOB
#define LED_FN0_Pin GPIO_PIN_12
#define LED_FN0_GPIO_Port GPIOB
#define SW_FN0_Pin GPIO_PIN_13
#define SW_FN0_GPIO_Port GPIOB
#define A_IN9_Pin GPIO_PIN_14
#define A_IN9_GPIO_Port GPIOB
#define A_IN8_Pin GPIO_PIN_15
#define A_IN8_GPIO_Port GPIOB
#define A_IN7_Pin GPIO_PIN_8
#define A_IN7_GPIO_Port GPIOA
#define A_IN6_Pin GPIO_PIN_9
#define A_IN6_GPIO_Port GPIOA
#define PGM_USART_RX_Pin GPIO_PIN_10
#define PGM_USART_RX_GPIO_Port GPIOA
#define PGM_DETECT_Pin GPIO_PIN_15
#define PGM_DETECT_GPIO_Port GPIOA
#define SPI_MISO_Pin GPIO_PIN_4
#define SPI_MISO_GPIO_Port GPIOB
#define SPI_MOSI_Pin GPIO_PIN_5
#define SPI_MOSI_GPIO_Port GPIOB
#define PGM_USART_TX_Pin GPIO_PIN_6
#define PGM_USART_TX_GPIO_Port GPIOB
#define READY_Pin GPIO_PIN_7
#define READY_GPIO_Port GPIOB
#define BOOT0_Pin GPIO_PIN_8
#define BOOT0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
