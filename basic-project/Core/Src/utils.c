/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : utils.c
  * @brief          : PSRAM testing utilities implementation
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

/* Includes ------------------------------------------------------------------*/
#include "utils.h"
#include <string.h>

/* External variables --------------------------------------------------------*/
extern XSPI_HandleTypeDef hospi1;

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Test PSRAM connection in single-SPI mode
  * @retval 1 on success, negative error code on failure
  */
int Test_PSRAM_Connection(void)
{
  XSPI_RegularCmdTypeDef cmd = {0};
  HAL_StatusTypeDef status;
  uint8_t idBytes[APS6404_ID_TOTAL_BYTES] = {0};
  const uint8_t testPattern = 0xA5;
  uint8_t readBack = 0;

  // Common, single-lane baseline for all commands
  cmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  cmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
  cmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  cmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  cmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  cmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  cmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
  cmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  cmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  cmd.DataMode           = HAL_XSPI_DATA_NONE;
  cmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  cmd.DummyCycles        = 0;
  cmd.DQSMode            = HAL_XSPI_DQS_DISABLE;
  cmd.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  // --- Step 1: Software reset sequence (0x66, 0x99) ---
  cmd.Instruction = APS6404_RESET_EN_CMD;
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -1;

  cmd.Instruction = APS6404_RESET_CMD;
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -1;

  HAL_Delay(2); // allow device to complete reset

  // --- Step 2: Read JEDEC ID (0x9F) ---
  cmd.Instruction = APS6404_READ_ID_CMD;
  cmd.AddressMode = HAL_XSPI_ADDRESS_1_LINE;
  cmd.AddressWidth = HAL_XSPI_ADDRESS_24_BITS;
  cmd.Address     = 0x000000;
  cmd.DataMode    = HAL_XSPI_DATA_1_LINE;
  cmd.DataLength  = APS6404_ID_TOTAL_BYTES;
  cmd.DummyCycles = APS6404_ID_DUMMY_CYCLES;
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -2;

  status = HAL_XSPI_Receive(&hospi1, idBytes, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -3;

  cmd.AddressMode = HAL_XSPI_ADDRESS_NONE;
  cmd.DataMode    = HAL_XSPI_DATA_NONE;
  cmd.DataLength  = 0;
  cmd.DummyCycles = 0;

  if (idBytes[APS6404_ID_DATA_BYTE0] != EXPECTED_MANUFACTURER_ID)
  {
    return -4; // Wrong MF ID
  }

  if (!((idBytes[APS6404_KGD_DATA_BYTE0] == EXPECTED_KGD_ID) ||
        (idBytes[APS6404_KGD_DATA_BYTE0] == EXPECTED_KGD_ID_ALT)))
  {
    return -4; // Wrong KGD
  }

  // --- Step 3: 0x02 single-write followed by 0x03 single-read ---
  cmd.Instruction = 0x02;
  cmd.AddressMode = HAL_XSPI_ADDRESS_1_LINE;
  cmd.Address     = 0x000000;
  cmd.DataMode    = HAL_XSPI_DATA_1_LINE;
  cmd.DataLength  = 1;

  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -5;

  status = HAL_XSPI_Transmit(&hospi1, &testPattern, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -6;

  cmd.Instruction = 0x03;

  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -7;

  status = HAL_XSPI_Receive(&hospi1, &readBack, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -8;

  if (readBack != testPattern) return -9;

  return 1; // success
}

/**
  * @brief  Test PSRAM in quad-SPI mode with 1-1-4 read/write
  * @retval 1 on success, negative error code on failure
  */
int Test_PSRAM_QuadMode(void)
{
  XSPI_RegularCmdTypeDef cmd = {0};
  HAL_StatusTypeDef status;

  #define TEST_SIZE 256
  uint8_t testBuffer[TEST_SIZE];
  uint8_t readBuffer[TEST_SIZE];
  uint32_t testAddress = 0x000000;

  // Fill test pattern
  for (int i = 0; i < TEST_SIZE; i++) {
    testBuffer[i] = (uint8_t)i;
  }
  memset(readBuffer, 0, TEST_SIZE);

  // Common baseline
  cmd.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  cmd.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
  cmd.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  cmd.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  cmd.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  cmd.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  cmd.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
  cmd.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  cmd.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  cmd.DataMode           = HAL_XSPI_DATA_NONE;
  cmd.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  cmd.DummyCycles        = 0;
  cmd.DQSMode            = HAL_XSPI_DQS_DISABLE;
  cmd.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  // --- Reset ---
  cmd.Instruction = 0x66;
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -1;

  cmd.Instruction = 0x99;
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -2;

  HAL_Delay(2);

  // --- QUAD WRITE (0x38): 1-1-4 mode ---
  cmd.Instruction = 0x38;
  cmd.AddressMode = HAL_XSPI_ADDRESS_1_LINE;   // 1-line address
  cmd.Address     = testAddress;
  cmd.DataMode    = HAL_XSPI_DATA_4_LINES;     // 4-line data
  cmd.DataLength  = TEST_SIZE;
  cmd.DummyCycles = 0;

  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -3;

  status = HAL_XSPI_Transmit(&hospi1, testBuffer, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -4;

  HAL_Delay(1);

  // --- QUAD READ (0xEB): 1-1-4 mode ---
  cmd.Instruction = 0xEB;
  cmd.AddressMode = HAL_XSPI_ADDRESS_1_LINE;   // 1-line address
  cmd.Address     = testAddress;
  cmd.DataMode    = HAL_XSPI_DATA_4_LINES;     // 4-line data
  cmd.DataLength  = TEST_SIZE;
  cmd.DummyCycles = 6;

  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -5;

  status = HAL_XSPI_Receive(&hospi1, readBuffer, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -6;

  // --- Verify ---
  for (int i = 0; i < TEST_SIZE; i++) {
    if (readBuffer[i] != testBuffer[i]) {
      return -(100 + i);
    }
  }

  return 1;  // SUCCESS
}
