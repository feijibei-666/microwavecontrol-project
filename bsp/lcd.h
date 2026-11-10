#ifndef __LCD_H
#define __LCD_H

#include "stm32f10x.h"

/* ST7735 128x160, SPI1 TX only: PA5=SCK, PA7=MOSI, PB7=DC,
 * PB8=RESET, PB9=CS. Module supply/backlight follow hardware specifications. */
#define LCD_W   128
#define LCD_H   160

#define LCD_WHITE   0xFFFF
#define LCD_BLACK   0x0000
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F
#define LCD_YELLOW  0xFFE0
#define LCD_CYAN    0x07FF
#define LCD_MAGENTA 0xF81F
#define LCD_GRAY    0x8410

#define LCD_FAULT_NONE          0
#define LCD_FAULT_SPI_TIMEOUT   1
#define LCD_FAULT_DMA_TIMEOUT   2
#define LCD_FAULT_DMA_TRANSFER  3

extern volatile uint32_t lcd_fault_code;

void LCD_Init(void);
/* RGB565 host-order words. Source row stride is the ORIGINAL width, even
 * when the destination rectangle is clipped at the right/bottom edge.
 * Only the display task may use these interfaces; transfers are synchronous. */
void LCD_Blit(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
              const uint16_t *pixels);
uint8_t LCD_FontColumn(char ch, uint8_t column, uint8_t half);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fore, uint16_t back);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fore, uint16_t back);
void LCD_ShowStringCenter(uint16_t y, const char *str, uint16_t fore, uint16_t back);

#endif
