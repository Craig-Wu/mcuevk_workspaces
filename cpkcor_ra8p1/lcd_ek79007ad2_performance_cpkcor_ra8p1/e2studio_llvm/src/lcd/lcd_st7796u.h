#ifndef __LCD_ST7796U_H
#define __LCD_ST7796U_H

#include "lcd.h"

#if LCD_EN_ST7796U

#ifdef __cplusplus
extern "C" {
#endif

uint32_t LCD_ST7796U_DrawBitmap(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *bitmap);
uint32_t LCD_ST7796U_DrawPoint(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint32_t color);
uint32_t LCD_ST7796U_Fill(LCD_DeviceType *lcd, uint32_t color);
uint32_t LCD_ST7796U_FillRectangle(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, uint32_t color);
uint32_t LCD_ST7796U_Init(LCD_DeviceType *lcd);
uint32_t LCD_ST7796U_IsIdle(LCD_DeviceType *lcd, bool *idle);
uint32_t LCD_ST7796U_Present(LCD_DeviceType *lcd);

#ifdef __cplusplus
}
#endif

#endif /* #if LCD_EN_ST7796U */

#endif
