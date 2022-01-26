/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  uint8_t length;
  char value[8];
  uint8_t is_correct;
  uint8_t current_pos;
  uint8_t number_of_mistakes;
  uint32_t input_time_start;
  uint32_t input_time_limit;
} Password;

typedef enum
{
  INPUT_CHANGE_PASS_COMMAND = 0,
  INPUT_LOWERCASE_ALPHA = 1,
  INPUT_UPPERCASE_ALPHA = 2,
  INPUT_ENTER = 3,
  INPUT_UNKNOWN = 4
} INPUT_TYPE;

typedef enum
{
  PASSWORD_CHANGE_NONE = 0,
  PASSWORD_CHANGE_INPUT = 1,
  PASSWORD_CHANGE_CONFIRM = 2
} PASSWORD_CHANGE_STATE;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MAX_PASSWORD_LEN 8

#define DEFAULT_PASSWORD "password"
#define DEFAULT_PASSWORD_LEN strlen(DEFAULT_PASSWORD)

#define ERR_PASS_EMPTY "\r\nPassword must have at least 1 character\r\n"
#define MSG_CONFIRM_PASS "\r\nApply new password (y/n)? [n] "
#define MSG_PASS_CHANGED "\r\nPassword has been changed\r\n"
#define MSG_PASS_UNCHANGED "\r\nPassword has not been changed\r\n"
#define MSG_ENTER_NEW_PASS "\r\nNew password>"
#define MSG_INTERRUPT_MODE_ENABLED "\r\nInterrupt mode - on\r\n"
#define MSG_INTERRUPT_MODE_DISABLED "\r\nInterrupt mode - off\r\n"

#define COM_CHANGE_PASS '+'

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define MIN_SEC_TO_MS(minutes, seconds) ((minutes) * 60 * 1000 + (seconds) * 1000)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

char init_password_value[8];
uint8_t init_password_len;

char new_password_value[8];
uint8_t new_password_len;

const uint32_t DEFAULT_PASSWORD_INPUT_TIME_LIMIT = MIN_SEC_TO_MS(1, 0);

uint8_t char_readed = 0;
uint8_t char_written = 0;

uint8_t interruption_mode_enabled = 0;
PASSWORD_CHANGE_STATE password_change_mode = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void blink_yellow()
{
  for (uint8_t i = 0; i < 10; i++)
  {
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);
    HAL_Delay(50);
  }
}

void blink_red()
{
  for (uint8_t i = 0; i < 10; i++)
  {
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
    HAL_Delay(50);
  }
}

void light_red()
{
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_Delay(MIN_SEC_TO_MS(0, 2));
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
}

void light_green()
{
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_Delay(MIN_SEC_TO_MS(0, 2));
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
}

void send_msg(const char *msg, uint8_t msg_len)
{
  if (msg_len == 0)
    msg_len = strlen(msg);

  if (interruption_mode_enabled){
	char_written = 0;
    HAL_UART_Transmit_IT(&huart6, msg, msg_len);
	while (!char_written);
  }

  else
    HAL_UART_Transmit(&huart6, msg, msg_len, MIN_SEC_TO_MS(0, 5));
}

uint8_t is_password_input_time_expired(Password *password)
{
  return HAL_GetTick() > (password->input_time_start + password->input_time_limit);
}

char read_input(Password *password)
{
  static uint8_t btn_pressed = 0;
  char_readed = 0;
  char c;

  while (!char_readed)
  {
    if (interruption_mode_enabled)
    {
      HAL_UART_Receive_IT(&huart6, &c, 1);
    }
    else
    {
      HAL_StatusTypeDef status = HAL_UART_Receive(&huart6, &c, 1, MIN_SEC_TO_MS(0, 1));
      if (status == HAL_OK)
        char_readed = 1;
    }

    //Кнопка нажата и нажатие еще не зафиксировано (reset-низкий уровень)
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_RESET && !btn_pressed)
    {
      btn_pressed = 1;
      interruption_mode_enabled = !interruption_mode_enabled;
      if (interruption_mode_enabled)
        send_msg(MSG_INTERRUPT_MODE_ENABLED, 0);
      else
        send_msg(MSG_INTERRUPT_MODE_DISABLED, 0);
    }
    //нажатие было зафиксировано и кнопка отпущена
    else if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_SET && btn_pressed)
      btn_pressed = 0;

    //Пользователь слишком долго вводит пароль -> сброс прогресса
    if (!password_change_mode && is_password_input_time_expired(password))
      init_password(password);
  }

  return c;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  char_readed = 1;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	char_written = 1;
}

INPUT_TYPE input_type(char val)
{
  if (val == COM_CHANGE_PASS)
    return INPUT_CHANGE_PASS_COMMAND;

  if (val == '\r')
    return INPUT_ENTER;

  if (val >= 'a' && val <= 'z')
    return INPUT_LOWERCASE_ALPHA;

  if (val >= 'A' && val <= 'Z')
    return INPUT_UPPERCASE_ALPHA;

  return INPUT_UNKNOWN;
}

void init_password(Password *password)
{
  memcpy(password->value, init_password_value, init_password_len);
  password->length = init_password_len;
  password->current_pos = 0;
  password->is_correct = 0;
  password->number_of_mistakes = 0;
  password->input_time_start = HAL_GetTick();
  password->input_time_limit = DEFAULT_PASSWORD_INPUT_TIME_LIMIT;
}

void reset_number_of_mistakes(Password *password)
{
  password->number_of_mistakes = 0;
}

void process_password(Password *password, char val)
{
  uint32_t pass = password->value[password->current_pos] == val;
  if (pass)
    password->current_pos++;
  else
    password->current_pos = 0;

  if (password->current_pos == password->length)
    password->is_correct = 1;
}

void finish_password_change(Password *password, char val)
{
  if (val == 0)
  {
    password_change_mode = PASSWORD_CHANGE_CONFIRM;
    send_msg(MSG_CONFIRM_PASS, 0);
    return;
  }

  if (val == 'y')
  {
    password_change_mode = PASSWORD_CHANGE_NONE;

    if (new_password_len < 1)
    {
      send_msg(ERR_PASS_EMPTY, 0);
      return;
    }

    memcpy(init_password_value, new_password_value, MAX_PASSWORD_LEN);
    init_password_len = new_password_len;
    init_password(password);

    send_msg(MSG_PASS_CHANGED, 0);
  }
  else
  {
    password_change_mode = PASSWORD_CHANGE_NONE;

    send_msg(MSG_PASS_UNCHANGED, 0);
  }
}

void process_password_change(Password *password, char val)
{
  if (new_password_len <= MAX_PASSWORD_LEN)
    new_password_value[new_password_len++] = val;

  if (new_password_len > MAX_PASSWORD_LEN)
    finish_password_change(password, 0);
}

void check_password(Password *password)
{
  if (password->is_correct)
  {
    light_green();
    init_password(password);
    return;
  }
  if (password->current_pos == 0)
  {
    password->number_of_mistakes += 1;
    if (password->number_of_mistakes == 3)
    {
      light_red();
      reset_number_of_mistakes(password);
    }
    else
      blink_red();
  }
  else
    blink_yellow();
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
  /* USER CODE BEGIN 2 */
  Password password = {};
  memcpy(init_password_value, DEFAULT_PASSWORD, DEFAULT_PASSWORD_LEN);
  init_password_len = DEFAULT_PASSWORD_LEN;

  init_password(&password);

  interruption_mode_enabled = 0;
  password_change_mode = PASSWORD_CHANGE_NONE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    char val = read_input(&password);
    send_msg(&val, 1);
    switch (input_type(val))
    {
    case INPUT_CHANGE_PASS_COMMAND:
      if (password_change_mode == PASSWORD_CHANGE_NONE)
      {
        password_change_mode = PASSWORD_CHANGE_INPUT;

        memset(new_password_value, 0, MAX_PASSWORD_LEN);
        new_password_len = 0;


        send_msg(MSG_ENTER_NEW_PASS, 0);

      }
      break;

    case INPUT_UPPERCASE_ALPHA:
      val -= ('A' - 'a');
    case INPUT_LOWERCASE_ALPHA:
      switch (password_change_mode)
      {
      case PASSWORD_CHANGE_NONE:
        process_password(&password, val);
        check_password(&password);
        break;
      case PASSWORD_CHANGE_INPUT:
        process_password_change(&password, val);
        break;
      case PASSWORD_CHANGE_CONFIRM:
        finish_password_change(&password, val);
        break;
      }
     break;

    case INPUT_ENTER:
      if (password_change_mode == PASSWORD_CHANGE_INPUT)
        finish_password_change(&password, 0);
      break;
    case INPUT_UNKNOWN:
    	break;
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
