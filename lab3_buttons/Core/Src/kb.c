#include "main.h"
#include "pca9538.h"
#include "kb.h"
#include "sdk_uart.h"
#include "usart.h"
#include "utils.h"

#define KBRD_RD_ADDR 0xE3
#define KBRD_WR_ADDR 0xE2
#define ROW1 0x1E
#define ROW2 0x3D
#define ROW3 0x7B
#define ROW4 0xF7

int ks_state = 0;
uint8_t ks_result = 0;
uint8_t ks_current_row = 0;

HAL_StatusTypeDef keyboard_read(void) {
	static uint8_t buf[4];
	uint8_t Nkey = 0x00;
	uint8_t kbd_in;

	switch (ks_state) {
	case 0:
		buf[0] = 0x70;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0;
		ks_state = 0;
		CHK_HAL(PCA9538_Write_Register(KBRD_WR_ADDR, CONFIG, buf))
		;
		ks_state = 1;
		return HAL_OK;
	case 1:
		buf[0] = 0;
		ks_state = 0;
		CHK_HAL(PCA9538_Write_Register(KBRD_WR_ADDR, OUTPUT_PORT, buf))
		;
		ks_state = 2;
		return HAL_OK;
	case 2:
		buf[0] = ks_current_row;
		ks_state = 0;
		CHK_HAL(PCA9538_Write_Register(KBRD_WR_ADDR, OUTPUT_PORT, buf))
		;
		ks_state = 3;
		return HAL_OK;
	case 3:
		buf[0] = 0;
		ks_state = 0;
		CHK_HAL(PCA9538_Read_Inputs(KBRD_RD_ADDR, buf))
		;
		ks_state = 4;
		break;
	case 4:
		ks_state = 0;
		kbd_in = buf[0] & 0x70;
		Nkey = 0;
		if (!(kbd_in & 0x10))
			Nkey |= 0x04;
		if (!(kbd_in & 0x20))
			Nkey |= 0x02;
		if (!(kbd_in & 0x40))
			Nkey |= 0x01;

		ks_result = Nkey;
		break;
	}
	return HAL_OK;
}
