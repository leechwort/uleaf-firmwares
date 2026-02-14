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

#define OCTOSPI1_MEM_BASE         0x90000000UL

/* W25Q64 Flash Commands */
#define W25Q64_CMD_WRITE_ENABLE   0x06
#define W25Q64_CMD_WRITE_DISABLE  0x04
#define W25Q64_CMD_READ_STATUS1   0x05
#define W25Q64_CMD_READ_STATUS2   0x35
#define W25Q64_CMD_WRITE_STATUS   0x01
#define W25Q64_CMD_PAGE_PROGRAM   0x02
#define W25Q64_CMD_QUAD_PAGE_PROG 0x32
#define W25Q64_CMD_SECTOR_ERASE   0x20
#define W25Q64_CMD_BLOCK_ERASE_32K 0x52
#define W25Q64_CMD_BLOCK_ERASE_64K 0xD8
#define W25Q64_CMD_CHIP_ERASE     0xC7
#define W25Q64_CMD_READ_DATA      0x03
#define W25Q64_CMD_FAST_READ      0x0B
#define W25Q64_CMD_READ_JEDEC_ID  0x9F
#define W25Q64_CMD_POWER_DOWN     0xB9
#define W25Q64_CMD_RELEASE_PD     0xAB
#define W25Q64_CMD_DEVICE_ID      0xAB
#define W25Q64_CMD_MANUFACTURER_ID 0x90

#define W25Q64_MANUFACTURER_ID    0xEF
#define W25Q64_DEVICE_ID          0x16  // 64Mbit
#define W25Q64_PAGE_SIZE          256
#define W25Q64_SECTOR_SIZE        4096
#define W25Q64_BLOCK_SIZE         65536

/* W25Q64 CS Pin */
#define W25Q64_CS_PIN             GPIO_PIN_11
#define W25Q64_CS_PORT            GPIOA
#define W25Q64_CS_LOW()           HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_RESET)
#define W25Q64_CS_HIGH()          HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET)

/* Exported function prototypes ---------------------------------------------*/
int Test_PSRAM_Connection(void);
int Test_PSRAM_QuadMode(void);
int Test_PSRAM_MemoryMapped(void);
int Test_PSRAM_Speed(uint32_t *writeSpeedKBps, uint32_t *readSpeedKBps);
int PSRAM_MemoryMapped_Init(void);
int Test_W25Q64_Flash(void);

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_H */
