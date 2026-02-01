/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : utils.h
  * @brief          : Header for utils.c file.
  *                   This file contains the common defines and function prototypes
  *                   for PSRAM testing utilities.
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

#ifndef __UTILS_H
#define __UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported defines ----------------------------------------------------------*/
#define APS6404_READ_ID_CMD       0x9F
#define APS6404_RESET_EN_CMD      0x66
#define APS6404_RESET_CMD         0x99
#define APS6404_ID_DUMMY_CYCLES   0
#define APS6404_ID_TOTAL_BYTES    10
#define APS6404_ID_DATA_BYTE0     0
#define APS6404_KGD_DATA_BYTE0    1
#define APS6404_ENTER_QUAD_CMD    0x35
#define APS6404_EXIT_QUAD_CMD     0xF5
#define APS6404_QUAD_WRITE_CMD    0x38
#define APS6404_QUAD_READ_CMD     0xEB
#define APS6404_QUAD_DUMMY_CYCLES 6

#define EXPECTED_MANUFACTURER_ID  0x0D
#define EXPECTED_KGD_ID           0x5D
#define EXPECTED_KGD_ID_ALT       0x50

/* Exported function prototypes ---------------------------------------------*/
int Test_PSRAM_Connection(void);
int Test_PSRAM_QuadMode(void);

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_H */
