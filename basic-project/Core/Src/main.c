/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

XSPI_HandleTypeDef hospi1;

#define APS6404_READ_ID_CMD       0x9F
#define APS6404_RESET_EN_CMD      0x66
#define APS6404_RESET_CMD         0x99
#define APS6404_ID_DUMMY_CYCLES   0   // clocks inserted before device drives data
#define APS6404_ID_TOTAL_BYTES    10   // includes dummy bytes returned by the device
#define APS6404_ID_DATA_BYTE0     0    // offset in stream where MFID appears
#define APS6404_KGD_DATA_BYTE0    1

/* USER CODE BEGIN PV */


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_OCTOSPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Expected ID for AP Memory 64Mbit devices
// Manufacturer ID: 0x0D
// Known Good Die (KGD): 0x5D
#define EXPECTED_MANUFACTURER_ID 0x0D
#define EXPECTED_KGD_ID          0x5D
#define EXPECTED_KGD_ID_ALT      0x50

/* USER CODE BEGIN 0 */
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
  cmd.AddressMode   = HAL_XSPI_ADDRESS_1_LINE;
  cmd.AddressWidth  = HAL_XSPI_ADDRESS_24_BITS;
  cmd.Address       = 0x000000;
  cmd.DataMode      = HAL_XSPI_DATA_1_LINE;
  cmd.DataLength    = 1;

  cmd.Instruction   = 0x02; // write
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -5;

  status = HAL_XSPI_Transmit(&hospi1, &testPattern, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -6;

  cmd.Instruction = 0x03; // read
  status = HAL_XSPI_Command(&hospi1, &cmd, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -7;

  status = HAL_XSPI_Receive(&hospi1, &readBack, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
  if (status != HAL_OK) return -8;

  if (readBack != testPattern) return -9;

  return 1; // success
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_OCTOSPI1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	int result = Test_PSRAM_Connection();

	  if (result == 1) {
		  // SUCCESS: Setup is correct!
		  // Proceed to Memory Mapping (CSP_OSPI_Init from previous answer)
	  } else {
		  // FAILURE: Check result code
		  // -4 = Chip connected but returned wrong ID (Check pData)
		  // -2/-3 = Timeouts (Check Clock/NCS pin)
	  }
	  HAL_Delay(250);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV64;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief OCTOSPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OCTOSPI1_Init(void)
{

  /* USER CODE BEGIN OCTOSPI1_Init 0 */

  /* USER CODE END OCTOSPI1_Init 0 */

  /* USER CODE BEGIN OCTOSPI1_Init 1 */

  /* USER CODE END OCTOSPI1_Init 1 */
  /* OCTOSPI1 parameter configuration*/
  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThresholdByte = 1;
  hospi1.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hospi1.Init.MemoryType = HAL_XSPI_MEMTYPE_APMEM;
  hospi1.Init.MemorySize = HAL_XSPI_SIZE_64MB;
  hospi1.Init.ChipSelectHighTimeCycle = 4;
  hospi1.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = 3;
  hospi1.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hospi1.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_DISABLE;
  hospi1.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE;
  hospi1.Init.DelayBlockBypass = HAL_XSPI_DELAY_BLOCK_BYPASS;
  hospi1.Init.Refresh = 0;
  if (HAL_XSPI_Init(&hospi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OCTOSPI1_Init 2 */

  /* USER CODE END OCTOSPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
