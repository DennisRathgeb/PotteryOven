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
#include "stm32f0xx_hal.h"

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
#define SW1_Pin GPIO_PIN_3
#define SW1_GPIO_Port GPIOA
#define SW2_Pin GPIO_PIN_4
#define SW2_GPIO_Port GPIOA
#define SW3_Pin GPIO_PIN_5
#define SW3_GPIO_Port GPIOA
#define BUT4_Pin GPIO_PIN_0
#define BUT4_GPIO_Port GPIOB
#define BUT4_EXTI_IRQn EXTI0_1_IRQn
#define BUT3_Pin GPIO_PIN_1
#define BUT3_GPIO_Port GPIOB
#define BUT3_EXTI_IRQn EXTI0_1_IRQn
#define SW_C_Pin GPIO_PIN_2
#define SW_C_GPIO_Port GPIOB
#define SW_C_EXTI_IRQn EXTI2_3_IRQn
#define SPI2_NSS_Pin GPIO_PIN_12
#define SPI2_NSS_GPIO_Port GPIOB
#define BUT2_Pin GPIO_PIN_6
#define BUT2_GPIO_Port GPIOF
#define BUT2_EXTI_IRQn EXTI4_15_IRQn
#define BUT1_Pin GPIO_PIN_7
#define BUT1_GPIO_Port GPIOF
#define BUT1_EXTI_IRQn EXTI4_15_IRQn
#define BUT5_Pin GPIO_PIN_15
#define BUT5_GPIO_Port GPIOA
#define BUT5_EXTI_IRQn EXTI4_15_IRQn
#define ENC_B_Pin GPIO_PIN_3
#define ENC_B_GPIO_Port GPIOB
#define ENC_B_EXTI_IRQn EXTI2_3_IRQn
#define ENC_A_Pin GPIO_PIN_4
#define ENC_A_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
