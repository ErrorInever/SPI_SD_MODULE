#include "main.h"
#include "cmsis_gcc.h"
#include "sd_spi.h"
#include "stm32f4xx_hal.h"
#include "uart.h"
#include <stdint.h>
/*
HCLK            100mhz
CST             100mhz
APB1 per        25mhz
APB1 clock      50mhz
APB2 per        50mhz
APB2 clock      100mhz
*/
void SystemClock_Config(void);

int main(void) {
  SystemClock_Config();
  USART2_init();
  SPI_init();
  SD_init();

  uint32_t sector_number = 18394; // № sector

  uint8_t write_buffer[512];

  for(uint16_t i = 0; i < 512; i++) {
    write_buffer[i] = 0xDD;
  }
  uint8_t sector_write_res = SD_WriteSector(sector_number, write_buffer);
  HAL_Delay(10);

  if(sector_write_res == 0) {
    UART_Printf("Write Sector: SUCCESS!\r\n");
  }
  if(sector_write_res == 5) {
    UART_Printf("Write Sector: TIMEOUT!\r\n");
  }
  if(sector_write_res == 2) {
    UART_Printf("Write Sector: ERROR!\r\n");
  }
  if(sector_write_res == 3) {
    UART_Printf("Write Sector: REJECTED BY CARD!\r\n");
  }

  uint8_t buffer[512];
  uint8_t sector_res = SD_ReadSector(sector_number, buffer);

  if (sector_res == 0) {
      UART_Printf("Read Sector %d: SUCCESS!\r\n", sector_number);
      UART_Printf("Random data: %X\r\n", buffer[100]);
  } else {
      UART_Printf("Read FAILED with error code: %d\r\n", sector_res);
  }

  while (1) {
    __NOP();
  }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {

  __disable_irq();
  while (1) {
  }

}
