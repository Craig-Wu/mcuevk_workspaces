#ifndef __LCD_ILI9881C_H
#define __LCD_ILI9881C_H

#include "lcd.h"

#if LCD_EN_ILI9881C

#ifdef __cplusplus
extern "C" {
#endif

uint32_t LCD_ILI9881C_Init(LCD_DeviceType *lcd);

#ifdef __cplusplus
}
#endif

#endif /* #if LCD_EN_ILI9881C */

#endif
