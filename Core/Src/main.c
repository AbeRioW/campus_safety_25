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
#include "adc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "dht11.h"
#include "stm32f1xx_hal_flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Flash storage address (last page of Flash memory for STM32F103C8T6 - 64KB)
#define FLASH_STORAGE_ADDR  0x0800FC00
#define FLASH_MAGIC_NUMBER  0x12345678
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t adc_value;
char show_data[20];
uint8_t hc_sr505_count = 0;
uint8_t people_detected = 0;
uint8_t vibration_detected = 0;
uint32_t vibration_start_time = 0;
uint8_t vibration_active = 0;
uint32_t beep_start_time = 0;
uint8_t beep_active = 0;
uint32_t dht11_read_time = 0;
uint32_t mq2_read_time = 0;

// Setting mode variables
uint8_t in_setting_mode = 0;
uint8_t setting_step = 0; // 0:temp, 1:humidity, 2:mq2, 3:back
uint8_t temp_threshold = 30;
uint8_t humidity_threshold = 100;
uint16_t mq2_threshold = 240;
uint8_t setting_first_entry = 1; // Flag for first entry into setting mode

// Vibration alert data for USART3
uint8_t vibration_alert_data[] = {0xFD, 0x00, 0x0A, 0x01, 0x01, 0xB7, 0xC7, 0xB7, 0xA8, 0xB4, 0xB3, 0xC8, 0xEB};
uint8_t vibration_sent = 0; // Flag to avoid duplicate sending

// MQ2 smoke alert data for USART3
uint8_t mq2_alert_data[] = {0xFD, 0x00, 0x12, 0x01, 0x01, 0xD1, 0xCC, 0xCE, 0xED, 0xB9, 0xFD, 0xB4, 0xF3, 0xA3, 0xAC, 0xC7, 0xEB, 0xD0, 0xA1, 0xD0, 0xC4};
uint8_t mq2_sent = 0; // Flag to avoid duplicate sending

// Temperature alert data for USART3
uint8_t temp_alert_data[] = {0xFD, 0x00, 0x0A, 0x01, 0x01, 0xCE, 0xC2, 0xB6, 0xC8, 0xB9, 0xFD, 0xB8, 0xDF};
uint8_t temp_alert_sent = 0; // Flag to avoid duplicate sending

// Humidity alert data for USART3
uint8_t humidity_alert_data[] = {0xFD, 0x00, 0x0A, 0x01, 0x01, 0xCA, 0xAA, 0xB6, 0xC8, 0xB9, 0xFD, 0xB4, 0xF3};
uint8_t humidity_alert_sent = 0; // Flag to avoid duplicate sending
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Flash storage structure
typedef struct {
    uint32_t magic_number;
    uint8_t temp_threshold;
    uint8_t humidity_threshold;
    uint16_t mq2_threshold;
} FlashStorage_t;

// Function to erase Flash page
void Flash_Erase(void)
{
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = FLASH_STORAGE_ADDR;
    EraseInitStruct.NbPages = 1;
    
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    
    HAL_FLASH_Lock();
}

// Function to write threshold data to Flash
void Flash_Write_Thresholds(void)
{
    Flash_Erase();
    
    HAL_FLASH_Unlock();
    
    FlashStorage_t storage;
    storage.magic_number = FLASH_MAGIC_NUMBER;
    storage.temp_threshold = temp_threshold;
    storage.humidity_threshold = humidity_threshold;
    storage.mq2_threshold = mq2_threshold;
    
    uint32_t* data_ptr = (uint32_t*)&storage;
    uint32_t address = FLASH_STORAGE_ADDR;
    
    for(int i = 0; i < sizeof(FlashStorage_t) / 4; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *data_ptr);
        address += 4;
        data_ptr++;
    }
    
    HAL_FLASH_Lock();
}

// Function to read threshold data from Flash
void Flash_Read_Thresholds(void)
{
    FlashStorage_t* storage = (FlashStorage_t*)FLASH_STORAGE_ADDR;
    
    if(storage->magic_number == FLASH_MAGIC_NUMBER)
    {
        // Valid data found, load thresholds
        temp_threshold = storage->temp_threshold;
        humidity_threshold = storage->humidity_threshold;
        mq2_threshold = storage->mq2_threshold;
    }
    // If magic number doesn't match, use default values
}

#ifdef __cplusplus
extern "C" {
#endif

void UpdateSettingDisplay(void)
{
    // Only clear screen when entering setting mode for the first time
    if(setting_first_entry)
    {
        OLED_Clear();
        OLED_ShowString(30, 0, (uint8_t*)"Setting", 16, 1);
        setting_first_entry = 0;
    }
    
    // Update temp line
    if(setting_step == 0)
    {
        sprintf(show_data, "> temp %dC  ", temp_threshold);
        OLED_ShowString(0, 18, (uint8_t*)show_data, 8, 1);
    }
    else
    {
        sprintf(show_data, "  temp %dC  ", temp_threshold);
        OLED_ShowString(0, 18, (uint8_t*)show_data, 8, 1);
    }
    
    // Update humidity line
    if(setting_step == 1)
    {
        sprintf(show_data, "> humidity %dRH ", humidity_threshold);
        OLED_ShowString(0, 30, (uint8_t*)show_data, 8, 1);
    }
    else
    {
        sprintf(show_data, "  humidity %dRH ", humidity_threshold);
        OLED_ShowString(0, 30, (uint8_t*)show_data, 8, 1);
    }
    
    // Update MQ2 line
    if(setting_step == 2)
    {
        sprintf(show_data, "> MQ2 %d   ", mq2_threshold);
        OLED_ShowString(0, 40, (uint8_t*)show_data, 8, 1);
    }
    else
    {
        sprintf(show_data, "  MQ2 %d   ", mq2_threshold);
        OLED_ShowString(0, 40, (uint8_t*)show_data, 8, 1);
    }
    
    // Update Back line
    if(setting_step == 3)
    {
        OLED_ShowString(0, 50, (uint8_t*)"> Back    ", 8, 1);
    }
    else
    {
        OLED_ShowString(0, 50, (uint8_t*)"  Back    ", 8, 1);
    }
    
    OLED_Refresh();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // Debounce: ignore interrupts within 200ms
    if(current_time - last_interrupt_time < 200)
    {
        return;
    }
    last_interrupt_time = current_time;
    
    if(GPIO_Pin == KEY1_Pin)
    {
        if(!in_setting_mode)
        {
            in_setting_mode = 1;
            setting_step = 0;
            setting_first_entry = 1;  // Set flag to clear screen on first entry
            UpdateSettingDisplay();
        }
        else
        {
            setting_step++;
            if(setting_step > 3)
            {
                // Exit setting mode
                in_setting_mode = 0;
                setting_step = 0;
                OLED_Clear();
                // Reset flag for next entry
                setting_first_entry = 1;
                // Save thresholds to Flash
                Flash_Write_Thresholds();
            }
            else
            {
                UpdateSettingDisplay();
            }
        }
    }
    else if(GPIO_Pin == KEY2_Pin && in_setting_mode)
    {
        if(setting_step == 0) // Temp setting
        {
            temp_threshold++;
            if(temp_threshold > 50)
            {
                temp_threshold = 15;
            }
            UpdateSettingDisplay();
        }
        else if(setting_step == 1) // Humidity setting
        {
            humidity_threshold++;
            if(humidity_threshold > 120)
            {
                humidity_threshold = 90;
            }
            UpdateSettingDisplay();
        }
        else if(setting_step == 2) // MQ2 setting
        {
            mq2_threshold++;
            if(mq2_threshold > 300)
            {
                mq2_threshold = 200;
            }
            UpdateSettingDisplay();
        }
    }
    else if(GPIO_Pin == KEY3_Pin && in_setting_mode)
    {
        if(setting_step == 0) // Temp setting
        {
            temp_threshold--;
            if(temp_threshold < 15)
            {
                temp_threshold = 50;
            }
            UpdateSettingDisplay();
        }
        else if(setting_step == 1) // Humidity setting
        {
            humidity_threshold--;
            if(humidity_threshold < 90)
            {
                humidity_threshold = 120;
            }
            UpdateSettingDisplay();
        }
        else if(setting_step == 2) // MQ2 setting
        {
            mq2_threshold--;
            if(mq2_threshold < 200)
            {
                mq2_threshold = 300;
            }
            UpdateSettingDisplay();
        }
    }
}

#ifdef __cplusplus
}
#endif
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	DHT11_Data_t dht_data;
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
  MX_ADC1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	OLED_Init();
	// Read thresholds from Flash
	Flash_Read_Thresholds();
//  OLED_ShowString(0,0,(uint8_t*)"hello",8,1);
//	OLED_Refresh();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	if (!in_setting_mode)
	{
		if(HAL_GetTick() - dht11_read_time >= 2000)
		{
			DHT11_READ_DATA(&dht_data);
			dht11_read_time = HAL_GetTick();
			
			// Check if temperature exceeds threshold
			if(dht_data.temp_int > temp_threshold)
			{
				if(!temp_alert_sent)
				{
					HAL_UART_Transmit(&huart3, temp_alert_data, sizeof(temp_alert_data), 100);
					temp_alert_sent = 1;
				}
			}
			else
			{
				temp_alert_sent = 0; // Reset flag when temperature drops below threshold
			}
			
			// Check if humidity exceeds threshold
			if(dht_data.humidity_int > humidity_threshold)
			{
				if(!humidity_alert_sent)
				{
					HAL_UART_Transmit(&huart3, humidity_alert_data, sizeof(humidity_alert_data), 100);
					humidity_alert_sent = 1;
				}
			}
			else
			{
				humidity_alert_sent = 0; // Reset flag when humidity drops below threshold
			}
		}
		
		if(HAL_GetTick() - mq2_read_time >= 500)
		{
			HAL_ADC_Start(&hadc1);
			HAL_ADC_PollForConversion(&hadc1, 100);
			adc_value = HAL_ADC_GetValue(&hadc1);
			HAL_ADC_Stop(&hadc1);
			mq2_read_time = HAL_GetTick();
			
			// Check if MQ2 exceeds threshold
			if(adc_value > mq2_threshold)
			{
				if(!mq2_sent)
				{
					HAL_UART_Transmit(&huart3, mq2_alert_data, sizeof(mq2_alert_data), 100);
					mq2_sent = 1;
				}
			}
			else
			{
				mq2_sent = 0; // Reset flag when value drops below threshold
			}
		}
		
		sprintf(show_data, "T:%d.%dC H:%d.%d%%", dht_data.temp_int, dht_data.temp_dec, dht_data.humidity_int, dht_data.humidity_dec);
		OLED_ShowString(0, 0, (uint8_t*)show_data, 8, 1);
		
		sprintf(show_data, "MQ2:%lu   ", adc_value);
		OLED_ShowString(0, 10, (uint8_t*)show_data, 8, 1);
		
		if(HAL_GPIO_ReadPin(HC_SR505_GPIO_Port, HC_SR505_Pin) == GPIO_PIN_SET)
		{
			hc_sr505_count++;
			if(hc_sr505_count >= 10)
			{
				people_detected = 1;
			}
		}
		else
		{
			hc_sr505_count = 0;
			people_detected = 0;
		}
		
		if(people_detected)
		{
			OLED_ShowString(0, 20, (uint8_t*)"People:Yes", 8, 1);
		}
		else
		{
			OLED_ShowString(0, 20, (uint8_t*)"People:No ", 8, 1);
		}
		
		if(HAL_GPIO_ReadPin(SW_1801P_GPIO_Port, SW_1801P_Pin) == GPIO_PIN_RESET)
		{
			vibration_detected = 1;
			if(!vibration_active)
			{
				vibration_active = 1;
				vibration_start_time = HAL_GetTick();
				// Send vibration alert via USART3
				if(!vibration_sent)
				{
					HAL_UART_Transmit(&huart3, vibration_alert_data, sizeof(vibration_alert_data), 100);
					vibration_sent = 1;
				}
			}
		}
		
		if(vibration_active)
		{
			if(HAL_GetTick() - vibration_start_time >= 3000)
			{
				vibration_active = 0;
				vibration_sent = 0; // Reset flag after vibration period ends
			}
		}
		
		if(vibration_active)
		{
			OLED_ShowString(0, 30, (uint8_t*)"Vibr:Yes", 8, 1);
		}
		else
		{
			OLED_ShowString(0, 30, (uint8_t*)"Vibr:No ", 8, 1);
		}
		
		if(people_detected && !beep_active)
		{
			HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
			beep_active = 1;
			beep_start_time = HAL_GetTick();
		}
		
		if(beep_active)
		{
			if(HAL_GetTick() - beep_start_time >= 5000)
			{
				HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
				beep_active = 0;
			}
		}
		
		OLED_Refresh();
		
		HAL_Delay(100);
	}
	else
	{
		// In setting mode, just refresh display
		HAL_Delay(100);
	}
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
