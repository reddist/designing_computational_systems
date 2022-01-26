/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdint.h"
#include "string.h"
#include "ctype.h"
#include "sdk_uart.h"
#include "kb.h"
#include "oled.h"
#include "fonts.h"

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// --DRIVERS--

void display_str(char* str, char* str2, FontDef font, OLED_COLOR color) {
	oled_Fill(Black);
	oled_SetCursor(0,0);
	oled_WriteString(str,font,color);
	oled_SetCursor(0,20);
	oled_WriteString(str2,font,color);
	oled_UpdateScreen();
}

uint8_t is_button_pressed(){
	return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_RESET;
}

void transmit_msg(const char *msg){
	UART_Transmit(msg);
}

uint32_t get_time(){
	return HAL_GetTick();
}

//--UTILS--

struct Security {
	uint8_t pass_length;
	uint8_t password[12];
	uint8_t pos;
};

struct Security_Pattern {
	uint32_t time_activated;
	uint8_t max_length;
	uint8_t min_length;
	uint8_t password[12];
	uint8_t pos;
};

struct State {
	struct Security* current_security;
	struct Security_Pattern* edit_security;
	uint8_t edit_mode;
};

char print_buf[128];

void apply_edited_pass(struct State* state) {
	for (int i = 0; i < state->edit_security->pos; i++) {
		state->current_security->password[i] = state->edit_security->password[i];
	}
	state->edit_security->time_activated = get_time();
	state->current_security->pass_length = state->edit_security->pos;
	state->current_security->pos = 0;
	state->edit_security->pos = 0;
}

void reset_pass(struct State* state) {
	state->current_security->pos = 0;
	state->edit_security->time_activated = get_time();
}

void cancel_edit(struct State* state) {
	state->edit_mode = 0;
	state->edit_security->time_activated = get_time();
	state->edit_security->pos = 0;
	snprintf(print_buf, sizeof(print_buf), "Password edit canceled\r\n");
	UART_Transmit((uint8_t*) print_buf);
	display_str("Password edit", "canceled",Font_7x10,White);
}

void try_commit_edit(struct State* state) {
	uint8_t pos = state->edit_security->pos;
	if (pos < state->edit_security->min_length) {
		display_str("Min password", "length - 8",Font_7x10,White);
	} else {
		apply_edited_pass(state);
		state->edit_mode = 0;
		display_str("Password is", "updated",Font_7x10,White);
	}
}

void handle_input_edit(struct State* state, uint8_t key) {
	uint8_t pos = state->edit_security->pos;
	if (pos < state->edit_security->max_length) {
		state->edit_security->password[pos++] = key;
		state->edit_security->pos++;
		oled_Fill(Black);
		oled_SetCursor(0,0);
		for (int i = 0; i < pos; i++) {
			oled_WriteChar('*', Font_11x18, White);
		}
		oled_UpdateScreen();
	} else {
		display_str("Pass max length", "is exceeded", Font_7x10,White);
	}
}

void handle_edit_key(struct State* state, uint8_t key) {
	switch (key) {
		case 10:
			cancel_edit(state);
			break;
		case 12:
			try_commit_edit(state);
			break;
		default:
			handle_input_edit(state, key);
			break;
	}
}

void switch_to_edit(struct State* state) {
	state->edit_mode = 1;
	snprintf(print_buf, sizeof(print_buf), "Password edit started\r\n");
	UART_Transmit((uint8_t*) print_buf);
	display_str("Enter a new", "password",Font_7x10,White);
}

void check_password(struct State* state) {
	if (state->current_security->pos != state->current_security->pass_length) {
		display_str("Password is", "incorrect",Font_7x10,White);
		state->current_security->pos = 0;
		return;
	}
	for (int i = 0; i < state->current_security->pass_length; i++) {
		if (state->current_security->password[i] != state->edit_security->password[i]) {
			display_str("Password is", "incorrect",Font_7x10,White);
			// reset input state
			reset_pass(state);
			return;
		}
	}
	display_str("Security is", "unlocked",Font_7x10,White);
	reset_pass(state);
}

void handle_input_unlock(struct State* state, uint8_t key) {
	uint8_t pos = state->current_security->pos;
	if (pos == 0) {
		state->edit_security->time_activated = get_time();
	}
	if (pos < state->edit_security->max_length) {
		state->current_security->password[pos++] = key;
		state->current_security->pos++;
		oled_Fill(Black);
		oled_SetCursor(0,0);
		for (int i = 0; i < pos; i++) {
			oled_WriteChar('*', Font_11x18, White);
		}
		oled_UpdateScreen();
	} else {
		display_str("Pass max length", "is exceeded",Font_7x10,White);
	}
}

void handle_unlock_key(struct State* state, uint8_t key) {
	switch (key) {
		case 10:
			switch_to_edit(state);
			break;
		case 12:
			check_password(state);
			break;
		default:
			handle_input_unlock(state, key);
			break;
	}
}

void handle_command(struct State* state, uint8_t key) {
	// map key 11 as '0'
	key = key == 11 ? 0 : key;
	if (state->edit_mode) {
		handle_edit_key(state, key);
	} else {
		handle_unlock_key(state, key);
	}
}


void KB_Test(struct State* state) {
 static const uint8_t rows[] = { 0xFE, 0xFD, 0xFB, 0xF7 };
 static uint8_t old_keys[] = { 0, 0, 0, 0 };
 static const uint8_t key_chars[] = {1,2,3,4,5,6,7,8,9,10,11,12};

 for(int current_row=0; current_row<4; current_row++){
	 uint8_t current_key = Check_Row(rows[current_row]);
	  uint8_t *old_key = &old_keys[current_row];

	  for (int i = 0; i < 3; ++i) {
	   int mask = 1 << i;
	   if ((current_key & mask) && !(*old_key & mask)) {
	 	  snprintf(print_buf, sizeof(print_buf), "Pressed key = %d\r\n", key_chars[i + 3 * current_row]);
	 	  			UART_Transmit((uint8_t*) print_buf);
	    handle_command(state, key_chars[i + 3 * current_row]);
	   }
	  }

	  *old_key = current_key;
 }

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
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  oled_Init();
  print_buf[0] = "\0";

  struct Security security = {};
  struct Security_Pattern pattern = {};
  pattern.max_length = 12;
  pattern.min_length = 8;
  pattern.pos = 8;
  for(int i = 0; i<8; i++){
	  pattern.password[i] = i + 1;
  }
  struct State state = {};
  state.current_security = &security;
  state.edit_security = &pattern;
  state.edit_mode = 0;
  apply_edited_pass(&state);
  display_str("Enter the", "password",Font_7x10,White);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
    {
		KB_Test(&state);
		HAL_Delay(15);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

#ifdef  USE_FULL_ASSERT
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

