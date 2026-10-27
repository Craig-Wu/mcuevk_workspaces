#ifndef __LCD_TFP410_H
#define __LCD_TFP410_H

#include "lcd.h"

#if LCD_EN_TFP410PAPR

#ifdef __cplusplus
extern "C" {
#endif

uint32_t LCD_TFP410_Init(LCD_DeviceType *lcd);

#ifdef __cplusplus
}
#endif

#endif /* #if LCD_EN_TFP410PAPR */

#endif
