#ifndef __LCD_H
#define __LCD_H

#include "stm32f10x.h"

/* 1.8 寸 SPI TFT（ST7735，128x160），软件 SPI 接线：
     CLK -> PA5
     SDA -> PA7
     RS  -> PB7   (0 = 命令，1 = 数据)
     RST -> PB8
     CS  -> PB9
   背光 BLK 接 VCC 即可 */
#define LCD_W   128
#define LCD_H   160

/* 常用颜色（RGB565） */
#define LCD_WHITE   0xFFFF
#define LCD_BLACK   0x0000
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F
#define LCD_YELLOW  0xFFE0
#define LCD_CYAN    0x07FF
#define LCD_MAGENTA 0xF81F
#define LCD_GRAY    0x8410

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fore, uint16_t back);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fore, uint16_t back);
void LCD_ShowStringCenter(uint16_t y, const char *str, uint16_t fore, uint16_t back);

#endif
