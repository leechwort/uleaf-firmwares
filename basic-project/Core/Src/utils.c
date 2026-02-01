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

/**
  * @brief  Test PSRAM in memory-mapped mode with quad read/write
  * @retval 1 on success, negative error code on failure
  */
int Test_PSRAM_MemoryMapped(void)
{
  XSPI_RegularCmdTypeDef sCommand = {0};
  XSPI_MemoryMappedTypeDef sMemMappedCfg = {0};
  HAL_StatusTypeDef status;

  #define MM_TEST_SIZE 512
  volatile uint8_t *psramPtr = (uint8_t *)OCTOSPI1_MEM_BASE;
  uint8_t testBuffer[MM_TEST_SIZE];
  uint8_t readBuffer[MM_TEST_SIZE];

  // Fill test pattern
  for (int i = 0; i < MM_TEST_SIZE; i++) {
    testBuffer[i] = (uint8_t)(i ^ 0xAA);
  }

  // Common baseline for command configuration
  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataMode           = HAL_XSPI_DATA_NONE;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DummyCycles        = 0;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  // --- Step 1: Reset PSRAM ---
  sCommand.Instruction = APS6404_RESET_EN_CMD;
  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -1;

  sCommand.Instruction = APS6404_RESET_CMD;
  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -2;

  HAL_Delay(2);

  // --- Step 2: Configure memory-mapped QUAD READ (0xEB) ---
  sCommand.OperationType = HAL_XSPI_OPTYPE_READ_CFG;
  sCommand.Instruction   = APS6404_QUAD_READ_CMD;  // 0xEB
  sCommand.AddressMode   = HAL_XSPI_ADDRESS_1_LINE;
  sCommand.DataMode      = HAL_XSPI_DATA_4_LINES;
  sCommand.DummyCycles   = APS6404_QUAD_DUMMY_CYCLES;
  sCommand.DQSMode       = HAL_XSPI_DQS_DISABLE;

  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -3;

  // --- Step 3: Configure memory-mapped QUAD WRITE (0x38) ---
  sCommand.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
  sCommand.Instruction   = APS6404_QUAD_WRITE_CMD; // 0x38
  sCommand.AddressMode   = HAL_XSPI_ADDRESS_1_LINE;
  sCommand.DataMode      = HAL_XSPI_DATA_4_LINES;
  sCommand.DummyCycles   = 0;

  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -4;

  // --- Step 4: Enable memory-mapped mode ---
  sMemMappedCfg.TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_DISABLE;
  sMemMappedCfg.TimeoutPeriodClock = 0;

  status = HAL_XSPI_MemoryMapped(&hospi1, &sMemMappedCfg);
  if (status != HAL_OK) return -5;

  // --- Step 5: Write test pattern via memory-mapped interface ---
  for (int i = 0; i < MM_TEST_SIZE; i++) {
    psramPtr[i] = testBuffer[i];
  }

  // Small delay to ensure write completion
  for (volatile int d = 0; d < 1000; d++);

  // --- Step 6: Read back via memory-mapped interface ---
  for (int i = 0; i < MM_TEST_SIZE; i++) {
    readBuffer[i] = psramPtr[i];
  }

  // --- Step 7: Abort memory-mapped mode ---
  status = HAL_XSPI_Abort(&hospi1);
  if (status != HAL_OK) return -6;

  // --- Step 8: Verify data ---
  for (int i = 0; i < MM_TEST_SIZE; i++) {
    if (readBuffer[i] != testBuffer[i]) {
      return -(100 + i);
    }
  }

  return 1;  // SUCCESS
}

/**
  * @brief  Measure read/write speed in memory-mapped mode
  * @param  writeSpeedKBps Pointer to store write speed in KB/s
  * @param  readSpeedKBps Pointer to store read speed in KB/s
  * @retval 1 on success, negative error code on failure
  */
int Test_PSRAM_Speed(uint32_t *writeSpeedKBps, uint32_t *readSpeedKBps)
{
  XSPI_RegularCmdTypeDef sCommand = {0};
  XSPI_MemoryMappedTypeDef sMemMappedCfg = {0};
  HAL_StatusTypeDef status;

  #define SPEED_TEST_SIZE (64 * 1024)  // 64KB test
  volatile uint8_t *psramPtr = (uint8_t *)OCTOSPI1_MEM_BASE;
  uint32_t startTick, endTick, elapsedMs;

  // Common baseline for command configuration
  sCommand.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  sCommand.IOSelect           = HAL_XSPI_SELECT_IO_3_0;
  sCommand.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  sCommand.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  sCommand.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  sCommand.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
  sCommand.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  sCommand.DataMode           = HAL_XSPI_DATA_NONE;
  sCommand.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  sCommand.DummyCycles        = 0;
  sCommand.DQSMode            = HAL_XSPI_DQS_DISABLE;
  sCommand.SIOOMode           = HAL_XSPI_SIOO_INST_EVERY_CMD;

  // --- Step 1: Reset PSRAM ---
  sCommand.Instruction = APS6404_RESET_EN_CMD;
  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -1;

  sCommand.Instruction = APS6404_RESET_CMD;
  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -2;

  HAL_Delay(2);

  // --- Step 2: Configure memory-mapped QUAD READ (0xEB) ---
  sCommand.OperationType = HAL_XSPI_OPTYPE_READ_CFG;
  sCommand.Instruction   = APS6404_QUAD_READ_CMD;  // 0xEB
  sCommand.AddressMode   = HAL_XSPI_ADDRESS_1_LINE;
  sCommand.DataMode      = HAL_XSPI_DATA_4_LINES;
  sCommand.DummyCycles   = APS6404_QUAD_DUMMY_CYCLES;
  sCommand.DQSMode       = HAL_XSPI_DQS_DISABLE;

  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -3;

  // --- Step 3: Configure memory-mapped QUAD WRITE (0x38) ---
  sCommand.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
  sCommand.Instruction   = APS6404_QUAD_WRITE_CMD; // 0x38
  sCommand.AddressMode   = HAL_XSPI_ADDRESS_1_LINE;
  sCommand.DataMode      = HAL_XSPI_DATA_4_LINES;
  sCommand.DummyCycles   = 0;

  status = HAL_XSPI_Command(&hospi1, &sCommand, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -4;

  // --- Step 4: Enable memory-mapped mode ---
  sMemMappedCfg.TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_DISABLE;
  sMemMappedCfg.TimeoutPeriodClock = 0;

  status = HAL_XSPI_MemoryMapped(&hospi1, &sMemMappedCfg);
  if (status != HAL_OK) return -5;

  // --- Step 5: WRITE SPEED TEST ---
  startTick = HAL_GetTick();
  
  for (uint32_t i = 0; i < SPEED_TEST_SIZE; i++) {
    psramPtr[i] = (uint8_t)(i & 0xFF);
  }
  
  endTick = HAL_GetTick();
  elapsedMs = endTick - startTick;
  
  if (elapsedMs == 0) elapsedMs = 1;  // Avoid division by zero
  *writeSpeedKBps = (SPEED_TEST_SIZE * 1000) / (elapsedMs * 1024);

  // Small delay between tests
  HAL_Delay(10);

  // --- Step 6: READ SPEED TEST ---
  volatile uint8_t dummy;
  startTick = HAL_GetTick();
  
  for (uint32_t i = 0; i < SPEED_TEST_SIZE; i++) {
    dummy = psramPtr[i];
  }
  
  endTick = HAL_GetTick();
  elapsedMs = endTick - startTick;
  
  if (elapsedMs == 0) elapsedMs = 1;  // Avoid division by zero
  *readSpeedKBps = (SPEED_TEST_SIZE * 1000) / (elapsedMs * 1024);

  // --- Step 7: Abort memory-mapped mode ---
  status = HAL_XSPI_Abort(&hospi1);
  if (status != HAL_OK) return -6;

  (void)dummy;  // Suppress unused variable warning

  return 1;  // SUCCESS
}
