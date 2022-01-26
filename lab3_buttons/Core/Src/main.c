/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "buzzer.h"
#include "sdk_uart.h"
#include "kb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
enum MainState {
	MS_IDLE, MS_PLAY, MS_EDIT,
};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FIFO_BUFFER_SIZE 32
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t event_buf[FIFO_BUFFER_SIZE];
size_t write_ptr = 0;
size_t read_ptr = 0;
int btn_state_prev = 0;
uint8_t loop_confirmed = 0;
int btn_state = 0;
char print_buf[256];
uint8_t BOUNCE_LIMIT = 10;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void init_buffer() {
	for (size_t i = 0; i < FIFO_BUFFER_SIZE; ++i)
		event_buf[i] = 0;
}

void append_buffer(uint8_t num) {
	event_buf[write_ptr] = num;
	write_ptr = (write_ptr + 1) % FIFO_BUFFER_SIZE;
}

int read_buffer() {
	if (read_ptr == write_ptr) {
		return -1;
	}
	uint8_t num = event_buf[read_ptr];
	read_ptr = (read_ptr + 1) % FIFO_BUFFER_SIZE;
	return num;
}

void KB_Test(void) {
	static uint8_t const rows[4] = { 0xF7, 0x7B, 0x3D, 0x1E };
	static int current_row = 0;
	static int row_result[4] = { 0, 0, 0, 0 };

	if (ks_state == 0) {
		if (row_result[current_row] != ks_result) {
			uint8_t keyNum = 0;
			if (ks_result & 1) {
				append_buffer(3 * current_row + 1);
			}
			if (ks_result & 2) {
				append_buffer(3 * current_row + 2);
			}
			if (ks_result & 4) {
				append_buffer(3 * current_row + 3);
			}
		}

		row_result[current_row] = ks_result;
		current_row = (current_row + 1) % 4;
		ks_current_row = rows[current_row];
		keyboard_read();
	}
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
	if (hi2c == &hi2c1 && ks_state) {
		keyboard_read();
	}
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	if (hi2c == &hi2c1 && ks_state) {
		keyboard_read();
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		KB_Test();
	}
}

uint8_t is_button_pressed()
{
  static uint8_t bounce_counter = 0;
  static uint8_t btn_pressed = 0;

  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_RESET) {
    if (bounce_counter <= BOUNCE_LIMIT)
      bounce_counter++;
  } else {
    if (bounce_counter > 0)
      bounce_counter--;
  }

  if (!btn_pressed && bounce_counter >= BOUNCE_LIMIT){
    btn_pressed = 1;
  }

  else if (btn_pressed && bounce_counter == 0)
    btn_pressed = 0;

  return btn_pressed;
}

void play_sound(uint16_t sound) {
	Buzzer_Set_Volume(BUZZER_VOLUME_MAX);
	Buzzer_Set_Freq(sound);
}

void __light_LED(GPIO_PinState pin13, GPIO_PinState pin14, GPIO_PinState pin15, uint16_t brightness) {
	htim4.Instance->CCR2 = brightness;
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, pin13);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, pin14);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, pin15);
};

//#region application

char* GAME_STARTED_MSG = "Game is started!\r\n";
char* GAME_STOPPED_MSG = "Game is stopped!\r\n";
char* GAME_SPEED_SLOW_MSG = "Set game speed to slow.\r\n";
char* GAME_SPEED_MEDIUM_MSG = "Set game speed to medium.\r\n";
char* GAME_SPEED_FAST_MSG = "Set game speed to fast.\r\n";
char* GAME_PLAYBACK_LEDS_MSG = "Set only LEDs mode for next game.\r\n";
char* GAME_PLAYBACK_SOUND_MSG = "Set only Sound mode for next game.\r\n";
char* GAME_PLAYBACK_LEDS_SOUND_MSG = "Set LEDs and Sound mode for next game.\r\n";

typedef enum {
    red,
    green,
    yellow
} COLOR;

void init_LED(){
	HAL_TIM_OC_Start(&htim4, TIM_CHANNEL_2);
	HAL_TIM_OC_Start(&htim4, TIM_CHANNEL_3);
	HAL_TIM_OC_Start(&htim4, TIM_CHANNEL_4);
	HAL_TIM_PWM_Init(&htim4);
}

void light_off() {
	htim4.Instance->CCR2 = 0;
	htim4.Instance->CCR3 = 0;
	htim4.Instance->CCR4 = 0;
}

void light_LED(COLOR color, uint16_t brightness) {
	light_off();
	switch (color) {
	        case red:
	        	htim4.Instance->CCR4 = brightness;
	            break;
	        case green:
	        	htim4.Instance->CCR2 = brightness;
	            break;
	        case yellow:
	        	htim4.Instance->CCR3 = brightness;
	            break;
	    }
};


typedef enum {
    slow,
    medium,
    fast
} GAME_SPEED;

typedef enum {
    sound,
    leds,
    leds_sound
} PLAYBACK_MODE;

struct Impulse {
    COLOR color;
    uint16_t brightness;
    uint16_t sound;
    uint8_t code;
};

struct Impulse impulse_1 = { color: red, brightness: 200, code: 1, sound: 500 };
struct Impulse impulse_2 = { color: red, brightness: 500, code: 2, sound: 700 };
struct Impulse impulse_3 = { color: red, brightness: 1000, code: 3, sound: 900 };
struct Impulse impulse_4 = { color: green, brightness: 200, code: 4, sound: 1100 };
struct Impulse impulse_5 = { color: green, brightness: 500, code: 5, sound: 1300 };
struct Impulse impulse_6 = { color: green, brightness: 1000, code: 6, sound: 1500 };
struct Impulse impulse_7 = { color: yellow, brightness: 200, code: 7, sound: 1700 };
struct Impulse impulse_8 = { color: yellow, brightness: 500, code: 8, sound: 1850 };
struct Impulse impulse_9 = { color: yellow, brightness: 1000, code: 9, sound: 2000 };

struct Trace {
    uint8_t code;
    uint8_t is_correct;
    uint8_t is_registered;
};

struct Trace default_trace = {0,0,0};

struct State {
    uint8_t is_started;
    PLAYBACK_MODE playback_mode;
    GAME_SPEED game_speed;
    uint32_t speed_duration;
    struct Impulse game_sequence[20];
    struct Trace game_trace[20];
    uint8_t current_step;
    uint8_t sequence_length;
    uint32_t prev_loop_time;
};

void select_next_playback_mode(struct State* state) {
    state->playback_mode = (state->playback_mode + 1) % 3;
    switch (state->playback_mode) {
        case leds:
            UART_Transmit(GAME_PLAYBACK_LEDS_MSG);
            break;
        case sound:
            UART_Transmit(GAME_PLAYBACK_SOUND_MSG);
            break;
        case leds_sound:
            UART_Transmit(GAME_PLAYBACK_LEDS_SOUND_MSG);
            break;
    }
}

void select_next_game_speed(struct State* state) {
    state->game_speed = (state->game_speed + 1) % 3;
    switch (state->game_speed) {
        case slow:
            state->speed_duration = 1600;
            UART_Transmit(GAME_SPEED_SLOW_MSG);
            break;
        case medium:
            state->speed_duration = 800;
            UART_Transmit(GAME_SPEED_MEDIUM_MSG);
            break;
        case fast:
            state->speed_duration = 400;
            UART_Transmit(GAME_SPEED_FAST_MSG);
            break;
    }
}

void print_results(struct State* state) {
	int points = 0;
    for (int i = 0; i < state->sequence_length; i++) {
        char msg[32];
        sprintf(&msg, "%d. Pressed %d (%d)\r\n", i+1, state->game_trace[i].code, state->game_trace[i].is_correct);
        UART_Transmit((const char*) &msg);
        points += state->game_trace[i].is_correct;
    }
    char msg[32];
    sprintf(&msg, "Earned points: %d\r\n", points);
    UART_Transmit((const char*) &msg);
}

void init_game_state(struct State* state){
		loop_confirmed = 0;
		state->prev_loop_time = HAL_GetTick();
	    state->game_sequence[0] = impulse_1;
	    state->game_sequence[1] = impulse_2;
	    state->game_sequence[2] = impulse_3;
	    state->game_sequence[3] = impulse_4;
	    state->game_sequence[4] = impulse_5;
	    state->game_sequence[5] = impulse_6;
	    state->game_sequence[6] = impulse_7;
	    state->game_sequence[7] = impulse_8;
	    state->game_sequence[8] = impulse_9;
	    state->game_sequence[9] = impulse_1;
	    state->game_sequence[10] = impulse_2;
	    state->game_sequence[11] = impulse_3;
	    state->game_sequence[12] = impulse_4;
	    state->game_sequence[13] = impulse_5;
	    state->game_sequence[14] = impulse_6;
	    state->game_sequence[15] = impulse_7;
	    state->game_sequence[16] = impulse_8;
	    state->game_sequence[17] = impulse_9;
	    state->game_sequence[18] = impulse_1;
	    state->game_sequence[19] = impulse_2;

	    state->game_trace[0] =  default_trace;
	    state->game_trace[1] =  default_trace;
	    state->game_trace[2] =  default_trace;
	    state->game_trace[3] =  default_trace;
	    state->game_trace[4] =  default_trace;
	    state->game_trace[5] =  default_trace;
	    state->game_trace[6] =  default_trace;
	    state->game_trace[7] =  default_trace;
	    state->game_trace[8] =  default_trace;
	    state->game_trace[9] =  default_trace;
	    state->game_trace[10] = default_trace;
	    state->game_trace[11] = default_trace;
	    state->game_trace[12] = default_trace;
	    state->game_trace[13] = default_trace;
	    state->game_trace[14] = default_trace;
	    state->game_trace[15] = default_trace;
	    state->game_trace[16] = default_trace;
	    state->game_trace[17] = default_trace;
	    state->game_trace[18] = default_trace;
	    state->game_trace[19] = default_trace;
	    state->current_step = -1;
}

void end_game(struct State* state){
	state->is_started = 0;
	light_off();
	Buzzer_Set_Volume(BUZZER_VOLUME_MUTE);
	print_results(state);
}

void switch_game_mode(struct State* state) {
    state->is_started = !state->is_started;
    if (state->is_started) {
        init_game_state(state);
        UART_Transmit(GAME_STARTED_MSG);
    } else {
    	end_game(state);
        UART_Transmit(GAME_STOPPED_MSG);
    }
}

void handle_game_key(struct State* state, uint8_t key) {
	snprintf(print_buf, sizeof(print_buf), "Pressed key = %d\r\n", key);
	UART_Transmit((uint8_t*) print_buf);
    struct Impulse impulse = state->game_sequence[state->current_step];
    struct Trace *trace = &state->game_trace[state->current_step];
    if (!trace->is_registered) {
        trace->code = key;
        trace->is_correct = impulse.code == trace->code;
    }
}

void print_key_description(uint8_t key) {
	snprintf(print_buf, sizeof(print_buf), "Pressed key = %d\r\n", key);
	UART_Transmit((uint8_t*) print_buf);
}

void handle_program_key(struct State* state, uint8_t key) {
    if (state->is_started) {
        handle_game_key(state, key);
    } else {
        print_key_description(key);
    }
}

void confirm_loop_sync(struct State* state) {
    if (state->prev_loop_time + state->speed_duration < HAL_GetTick()) {
        loop_confirmed = 1;
        state->prev_loop_time = HAL_GetTick();
    }
}

void game_loop(struct State* state) {
	confirm_loop_sync(state);
    if (!state->is_started || !loop_confirmed)
        return;
    state->current_step++;
    if (state->current_step < state->sequence_length) {
        struct Impulse impulse = state->game_sequence[state->current_step];
        switch (state->playback_mode) {
            case leds:
                light_LED(impulse.color, impulse.brightness);
                break;
            case sound:
                play_sound(impulse.sound);
                break;
            case leds_sound:
                play_sound(impulse.sound);
                light_LED(impulse.color, impulse.brightness);
                break;
        }
    } else {
        end_game(state);
    }
    loop_confirmed = 0;
}


void init_state(struct State* state) {
    state->is_started = 0;
    state->playback_mode = leds_sound;
    state->game_speed = medium;
    state->speed_duration = 1000;
    state->sequence_length = 20;
    init_game_state(state);
}

void handle_command(struct State* state, uint8_t code) {
    switch (code) {
        case 12:
            switch_game_mode(state);
            break;
        case 11:
            select_next_playback_mode(state);
            break;
        case 10:
            select_next_game_speed(state);
            break;
        case -1:
        	return;
        default:
            handle_program_key(state, code);
            break;
    }
}

//#endregion
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
  MX_TIM4_Init();
  MX_TIM6_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
	uint8_t is_test_mode = 1;
	Buzzer_Init();
	init_LED();
	HAL_TIM_Base_Start_IT(&htim6);

	init_buffer();
	struct State state = {};
	init_state(&state);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for (;;) {
		if (is_button_pressed()) {
			while(is_button_pressed()){}
			is_test_mode ^= 1;
			if (is_test_mode) {
				UART_Transmit((uint8_t*) "*** KEYBOARD TEST MODE ***\r\n");
				Buzzer_Set_Volume(BUZZER_VOLUME_MUTE);
			} else {
				UART_Transmit((uint8_t*) "*** APPLICATION MODE ***\r\n");
			}
		}

		if (is_test_mode) {
			int key = read_buffer();
			if (key >= 0) {
				snprintf(print_buf, sizeof(print_buf), "Pressed key = %d\r\n", 13 - key);
				UART_Transmit((uint8_t*) print_buf);
			}
		} else {
			int cmd = read_buffer();
			if(cmd >= 0)
			handle_command(&state, 13 - cmd);
			game_loop(&state);
		}

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

