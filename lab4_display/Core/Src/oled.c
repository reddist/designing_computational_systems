#include "oled.h"
#include "i2c.h"

static uint8_t OLED_Buffer[1024];

static OLED_t OLED;


static void oled_WriteCommand(uint8_t command) {
	HAL_I2C_Mem_Write(&hi2c1,OLED_I2C_ADDR,0x00,1,&command,1,10);
}


uint8_t oled_Init(void) {
	HAL_Delay(100);

	oled_WriteCommand(0xAE);  // Выключить дисплей

	oled_WriteCommand(0x20);  // Режим адресации памяти:
	oled_WriteCommand(0x00);  // 	горизонтальный

	oled_WriteCommand(0xC8);  // Инверсия отображения сигналов  COM

	oled_WriteCommand(0x40);  // Начальная строка: 0

	oled_WriteCommand(0x81);  // контраст
	oled_WriteCommand(0xFF);  // 256

	oled_WriteCommand(0xA1);  // Инверсия отображения сигналов SEG (SEG0 = column 127)

	oled_WriteCommand(0xA6);  // Нормальный цвет (не инвертированный)

	oled_WriteCommand(0xA8);  // Настройка мультиплексора COM
	oled_WriteCommand(0x3F);  // 	(mux ratio = 64)

	oled_WriteCommand(0xA4);  // Отображать содержимое видеопамяти

	oled_WriteCommand(0xD3);  // Вертикальный сдвиг:
	oled_WriteCommand(0x00);  // 	0

	oled_WriteCommand(0xD5);  // Настройка синхросигналов
	oled_WriteCommand(0xF0);  // 	(DCLK divide ratio = 1, Fosc = 15)

	oled_WriteCommand(0xD9);  // Настройка предзарядки
	oled_WriteCommand(0x22);  // 	(phase1 = 2, phase2 = 2)

	oled_WriteCommand(0xDA);  // Настройка конфигурации
	oled_WriteCommand(0x12);  // 	сигналов COM

	oled_WriteCommand(0xDB);  // Настройка напряжения
	oled_WriteCommand(0x20);  // 	Vcomh = ~0.77 x Vcc

	oled_WriteCommand(0x8D);  // Включение встроенного источника
	oled_WriteCommand(0x14);  // 	 напряжения (зарядового насоса)

	oled_WriteCommand(0xAF);  // Включить дисплей

	oled_WriteCommand(0x21);
	oled_WriteCommand(0x00);
	oled_WriteCommand(0x7f); // установка номеров используемых столбцов

	oled_WriteCommand(0x22);
	oled_WriteCommand(0x00);
	oled_WriteCommand(0x07); // установка номеров используемых страниц

	oled_Fill(Black);

	oled_UpdateScreen();

	OLED.CurrentX = 0;
	OLED.CurrentY = 0;

	OLED.Initialized = 1;

	return 1;
}

void oled_Fill(OLED_COLOR color) {
	uint32_t i;

	for(i = 0; i < sizeof(OLED_Buffer); i++) {
		OLED_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
	}
}


void oled_UpdateScreen(void) {
	HAL_I2C_Mem_Write(&hi2c1,OLED_I2C_ADDR,0x40,1,OLED_Buffer,sizeof(OLED_Buffer),sizeof(OLED_Buffer));
}

void oled_DrawPixel(uint8_t x, uint8_t y, OLED_COLOR color) {
	if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
		return;
	}

	if (OLED.Inverted) {
		color = (OLED_COLOR)!color;
	}

	if (color == White) {
		OLED_Buffer[x + (y / 8) * OLED_WIDTH] |= 1 << (y % 8);
	} else {
		OLED_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
	}
}

void oled_DrawHLine(uint8_t x1, uint8_t x2, uint8_t y, OLED_COLOR color) {
	for(int i = x1; i <= x2; i++) {
		oled_DrawPixel(i, y, color);
	}
}
void oled_DrawVLine(uint8_t y1, uint8_t y2, uint8_t x, OLED_COLOR color) {
	for(int i = y1; i <= y2; i++) {
		oled_DrawPixel(x, i, color);
	}
}
void oled_DrawSquare(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2, OLED_COLOR color) {
	oled_DrawHLine(x1, x2, y1, color);
	oled_DrawHLine(x1, x2, y2, color);
	oled_DrawVLine(y1, y2, x1, color);
	oled_DrawVLine(y1, y2, x2, color);
}
char oled_WriteChar(char ch, FontDef Font, OLED_COLOR color) {
	uint32_t i, b, j;

	if (OLED_WIDTH <= (OLED.CurrentX + Font.FontWidth) ||
			OLED_HEIGHT <= (OLED.CurrentY + Font.FontHeight)) {
		return 0;
	}

	for (i = 0; i < Font.FontHeight; i++) {
		b = Font.data[(ch - 32) * Font.FontHeight + i];
		for (j = 0; j < Font.FontWidth; j++) {
			if ((b << j) & 0x8000) {
				oled_DrawPixel(OLED.CurrentX + j, (OLED.CurrentY + i), (OLED_COLOR) color);
			} else {
				oled_DrawPixel(OLED.CurrentX + j, (OLED.CurrentY + i), (OLED_COLOR)!color);
			}
		}
	}

	OLED.CurrentX += Font.FontWidth;

	return ch;
}

char oled_WriteString(char* str, FontDef Font, OLED_COLOR color) {
	while (*str) {
		if (oled_WriteChar(*str, Font, color) != *str) {
			return *str;
		}
		str++;
	}
	return *str;
}

void oled_SetCursor(uint8_t x, uint8_t y) {
	OLED.CurrentX = x;
	OLED.CurrentY = y;
}

void oled_DrawBitmap(uint8_t* bmp, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
	for (int h = 0; h < height; h++) {
		for (int w = 0; w < width; w++) {
			oled_DrawPixel(w+x, h+y, bmp[w+h*width]);
		}
	}
}
