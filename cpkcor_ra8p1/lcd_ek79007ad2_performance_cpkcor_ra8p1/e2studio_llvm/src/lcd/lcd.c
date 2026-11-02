/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include "hal_data.h"
#include "lcd.h"

#if LCD_EN_EK79001
#include "lcd_ek79001.h"
#endif

#if LCD_EN_EK79007AD2
#include "lcd_ek79007ad2.h"
#endif

#if LCD_EN_ILI9881C
#include "lcd_ili9881c.h"
#endif

#if LCD_EN_ST7796U
#include "lcd_st7796u.h"
#endif

#if LCD_EN_TFP410PAPR
#include "lcd_tfp410.h"
#endif

/*================================== MACROS =======================================================*/

#ifndef LCD_EN_VOLATILE
#define LCD_EN_VOLATILE			0
#endif

#ifndef __LCD_DEBUG
#define __LCD_DEBUG				1
#endif

#if __LCD_DEBUG
#ifndef __LCD_DEBUG_EN_PULSE
#define __LCD_DEBUG_EN_PULSE	1
#endif
#else
#define __LCD_DEBUG_EN_PULSE	0
#endif

#if __LCD_DEBUG
#include <utils/log.h>
#define LIKE_RETURN(v, t, r, msg, ...)		if (v == t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return r; }
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define LCD_LOGD(msg, ...)                  LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGI(msg, ...)                  LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGW(msg, ...)                  LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define LCD_LOGE(msg, ...)                  LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define LIKE_RETURN(v, t, r, msg, ...)		if (v == t) { return r; }
#define LCD_LOGD(msg, ...)
#define LCD_LOGI(msg, ...)
#define LCD_LOGW(msg, ...)
#define LCD_LOGE(msg, ...)
#endif

#if LCD_EN_VOLATILE
#define VOLATILE volatile
#else
#define VOLATILE
#endif

#if __LCD_DEBUG_EN_PULSE
#define GEN_PULSE(pin, us)					do { \
												R_IOPORT_PinWrite(g_ioport.p_ctrl, pin, 0); \
												R_IOPORT_PinWrite(g_ioport.p_ctrl, pin, 1); \
												R_BSP_SoftwareDelay(us, BSP_DELAY_UNITS_MICROSECONDS); \
												R_IOPORT_PinWrite(g_ioport.p_ctrl, pin, 0); \
											} while(0)
#else
#define GEN_PULSE(pin, us)
#endif

#define GET_DRAW_LAYER(p, layer)			if (layer == 1) { p = &lcd->layer_1; } else if (layer == 2) { p = &lcd->layer_2; } else { p = NULL; }
#define IS_INVALID_POINT(p)					if (p == NULL) { return FSP_ERR_INVALID_POINTER; }
#define IS_INVALID_POSITION(lcd, x, y)		if ((x >= lcd->x_length) || (y >= lcd->y_length)) { return FSP_ERR_ASSERTION; }
#define IS_NONEED_PROCESS_1(a)				if (a == 0) { return FSP_SUCCESS; }
#define IS_NONEED_PROCESS_2(a, b)			if ((a == 0) || (b == 0)) { return FSP_SUCCESS; }
#define IS_UNSUPPORT_PERI(peri)				if (peri != LCD_PERI_GLCDC) { return FSP_ERR_UNSUPPORTED; }

#define IS_INVALID_LAYER(p)					do { \
												if (p == NULL) { \
													LCD_LOGE("Not support layer"); \
													return FSP_ERR_UNSUPPORTED; \
												} \
												if ((p->graphic_mem_1 == NULL) && (p->graphic_mem_2 == NULL)) {\
													LCD_LOGE("This layer is invalid"); \
													return FSP_ERR_UNSUPPORTED; \
												} \
											} while (0)
#define IS_OUT_OF_LAYER_RANGE(x, y, layer)	do { \
												if (x >= layer->x_length) { \
													LCD_LOGW("X [%" PRIu16 "] out of range", x); \
													return FSP_ERR_ASSERTION; \
												} \
												if (y >= layer->y_length) { \
													LCD_LOGW("Y [%" PRIu16 "] out of range", y); \
													return FSP_ERR_ASSERTION; \
												} \
											} while (0)

#define LCD_PULSE_PIN_1						BSP_IO_PORT_08_PIN_02

/*================================== TYPES ========================================================*/

struct lcd_model_name_t {
    LCD_ModelEnum model;
    const char *name;
};

/*================================== GLOBAL VARIABLES =============================================*/

/*================================== LOCAL VARIABLES ==============================================*/

static const struct lcd_model_name_t sc_lcd_model_name[] = {
#if LCD_EN_EK79001
    {LCD_MODEL_EK79001, "EK79001"},
#endif
#if LCD_EN_EK79007AD2
    {LCD_MODEL_EK79007AD2, "EK79007AD2"},
#endif
#if LCD_EN_ILI9881C
    {LCD_MODEL_ILI9881C, "ILI9881C"},
#endif
#if LCD_EN_JD9365HD
    {LCD_MODEL_JD9365HD, "JD9365HD"},
#endif
#if LCD_EN_ST7796U
    {LCD_MODEL_ST7796U, "ST7796U"},
#endif
#if LCD_EN_TFP410PAPR
    {LCD_MODEL_TFP410PAPR, "TFP410PAPR"},
#endif
    {LCD_MODEL_UNKNOW, "Unknow"},
};

#if LCD_EN_PERI_GLCDC
/* Default opaque ARGB8888 palette: 216 RGB cube colors + 40 grey levels.
 * 0x00..0xD7: R/G/B = 0x00, 0x33, 0x66, 0x99, 0xCC, 0xFF; blue varies fastest.
 * 0xD8..0xFF: R = G = B = 0x08 + 6 * (index - 0xD8).
 */
static const uint32_t sc_default_clut8[256] = {
	0xFF000000, /* 0x00: LCD_COLOR_CLUT8_BLACK */
	0xFF000033, /* 0x01: LCD_COLOR_CLUT8_RGB_000033 */
	0xFF000066, /* 0x02: LCD_COLOR_CLUT8_RGB_000066 */
	0xFF000099, /* 0x03: LCD_COLOR_CLUT8_RGB_000099 */
	0xFF0000CC, /* 0x04: LCD_COLOR_CLUT8_RGB_0000CC */
	0xFF0000FF, /* 0x05: LCD_COLOR_CLUT8_BLUE */
	0xFF003300, /* 0x06: LCD_COLOR_CLUT8_RGB_003300 */
	0xFF003333, /* 0x07: LCD_COLOR_CLUT8_RGB_003333 */
	0xFF003366, /* 0x08: LCD_COLOR_CLUT8_RGB_003366 */
	0xFF003399, /* 0x09: LCD_COLOR_CLUT8_RGB_003399 */
	0xFF0033CC, /* 0x0A: LCD_COLOR_CLUT8_RGB_0033CC */
	0xFF0033FF, /* 0x0B: LCD_COLOR_CLUT8_RGB_0033FF */
	0xFF006600, /* 0x0C: LCD_COLOR_CLUT8_RGB_006600 */
	0xFF006633, /* 0x0D: LCD_COLOR_CLUT8_RGB_006633 */
	0xFF006666, /* 0x0E: LCD_COLOR_CLUT8_RGB_006666 */
	0xFF006699, /* 0x0F: LCD_COLOR_CLUT8_RGB_006699 */
	0xFF0066CC, /* 0x10: LCD_COLOR_CLUT8_RGB_0066CC */
	0xFF0066FF, /* 0x11: LCD_COLOR_CLUT8_RGB_0066FF */
	0xFF009900, /* 0x12: LCD_COLOR_CLUT8_RGB_009900 */
	0xFF009933, /* 0x13: LCD_COLOR_CLUT8_RGB_009933 */
	0xFF009966, /* 0x14: LCD_COLOR_CLUT8_RGB_009966 */
	0xFF009999, /* 0x15: LCD_COLOR_CLUT8_RGB_009999 */
	0xFF0099CC, /* 0x16: LCD_COLOR_CLUT8_RGB_0099CC */
	0xFF0099FF, /* 0x17: LCD_COLOR_CLUT8_RGB_0099FF */
	0xFF00CC00, /* 0x18: LCD_COLOR_CLUT8_RGB_00CC00 */
	0xFF00CC33, /* 0x19: LCD_COLOR_CLUT8_RGB_00CC33 */
	0xFF00CC66, /* 0x1A: LCD_COLOR_CLUT8_RGB_00CC66 */
	0xFF00CC99, /* 0x1B: LCD_COLOR_CLUT8_RGB_00CC99 */
	0xFF00CCCC, /* 0x1C: LCD_COLOR_CLUT8_RGB_00CCCC */
	0xFF00CCFF, /* 0x1D: LCD_COLOR_CLUT8_RGB_00CCFF */
	0xFF00FF00, /* 0x1E: LCD_COLOR_CLUT8_GREEN */
	0xFF00FF33, /* 0x1F: LCD_COLOR_CLUT8_RGB_00FF33 */
	0xFF00FF66, /* 0x20: LCD_COLOR_CLUT8_RGB_00FF66 */
	0xFF00FF99, /* 0x21: LCD_COLOR_CLUT8_RGB_00FF99 */
	0xFF00FFCC, /* 0x22: LCD_COLOR_CLUT8_RGB_00FFCC */
	0xFF00FFFF, /* 0x23: LCD_COLOR_CLUT8_LIGHT_BLUE */
	0xFF330000, /* 0x24: LCD_COLOR_CLUT8_RGB_330000 */
	0xFF330033, /* 0x25: LCD_COLOR_CLUT8_RGB_330033 */
	0xFF330066, /* 0x26: LCD_COLOR_CLUT8_RGB_330066 */
	0xFF330099, /* 0x27: LCD_COLOR_CLUT8_RGB_330099 */
	0xFF3300CC, /* 0x28: LCD_COLOR_CLUT8_RGB_3300CC */
	0xFF3300FF, /* 0x29: LCD_COLOR_CLUT8_RGB_3300FF */
	0xFF333300, /* 0x2A: LCD_COLOR_CLUT8_RGB_333300 */
	0xFF333333, /* 0x2B: LCD_COLOR_CLUT8_RGB_333333 */
	0xFF333366, /* 0x2C: LCD_COLOR_CLUT8_RGB_333366 */
	0xFF333399, /* 0x2D: LCD_COLOR_CLUT8_RGB_333399 */
	0xFF3333CC, /* 0x2E: LCD_COLOR_CLUT8_RGB_3333CC */
	0xFF3333FF, /* 0x2F: LCD_COLOR_CLUT8_RGB_3333FF */
	0xFF336600, /* 0x30: LCD_COLOR_CLUT8_RGB_336600 */
	0xFF336633, /* 0x31: LCD_COLOR_CLUT8_RGB_336633 */
	0xFF336666, /* 0x32: LCD_COLOR_CLUT8_RGB_336666 */
	0xFF336699, /* 0x33: LCD_COLOR_CLUT8_RGB_336699 */
	0xFF3366CC, /* 0x34: LCD_COLOR_CLUT8_RGB_3366CC */
	0xFF3366FF, /* 0x35: LCD_COLOR_CLUT8_RGB_3366FF */
	0xFF339900, /* 0x36: LCD_COLOR_CLUT8_RGB_339900 */
	0xFF339933, /* 0x37: LCD_COLOR_CLUT8_RGB_339933 */
	0xFF339966, /* 0x38: LCD_COLOR_CLUT8_RGB_339966 */
	0xFF339999, /* 0x39: LCD_COLOR_CLUT8_RGB_339999 */
	0xFF3399CC, /* 0x3A: LCD_COLOR_CLUT8_RGB_3399CC */
	0xFF3399FF, /* 0x3B: LCD_COLOR_CLUT8_RGB_3399FF */
	0xFF33CC00, /* 0x3C: LCD_COLOR_CLUT8_RGB_33CC00 */
	0xFF33CC33, /* 0x3D: LCD_COLOR_CLUT8_RGB_33CC33 */
	0xFF33CC66, /* 0x3E: LCD_COLOR_CLUT8_RGB_33CC66 */
	0xFF33CC99, /* 0x3F: LCD_COLOR_CLUT8_RGB_33CC99 */
	0xFF33CCCC, /* 0x40: LCD_COLOR_CLUT8_RGB_33CCCC */
	0xFF33CCFF, /* 0x41: LCD_COLOR_CLUT8_RGB_33CCFF */
	0xFF33FF00, /* 0x42: LCD_COLOR_CLUT8_RGB_33FF00 */
	0xFF33FF33, /* 0x43: LCD_COLOR_CLUT8_RGB_33FF33 */
	0xFF33FF66, /* 0x44: LCD_COLOR_CLUT8_RGB_33FF66 */
	0xFF33FF99, /* 0x45: LCD_COLOR_CLUT8_RGB_33FF99 */
	0xFF33FFCC, /* 0x46: LCD_COLOR_CLUT8_RGB_33FFCC */
	0xFF33FFFF, /* 0x47: LCD_COLOR_CLUT8_RGB_33FFFF */
	0xFF660000, /* 0x48: LCD_COLOR_CLUT8_RGB_660000 */
	0xFF660033, /* 0x49: LCD_COLOR_CLUT8_RGB_660033 */
	0xFF660066, /* 0x4A: LCD_COLOR_CLUT8_RGB_660066 */
	0xFF660099, /* 0x4B: LCD_COLOR_CLUT8_RGB_660099 */
	0xFF6600CC, /* 0x4C: LCD_COLOR_CLUT8_RGB_6600CC */
	0xFF6600FF, /* 0x4D: LCD_COLOR_CLUT8_RGB_6600FF */
	0xFF663300, /* 0x4E: LCD_COLOR_CLUT8_RGB_663300 */
	0xFF663333, /* 0x4F: LCD_COLOR_CLUT8_RGB_663333 */
	0xFF663366, /* 0x50: LCD_COLOR_CLUT8_RGB_663366 */
	0xFF663399, /* 0x51: LCD_COLOR_CLUT8_RGB_663399 */
	0xFF6633CC, /* 0x52: LCD_COLOR_CLUT8_RGB_6633CC */
	0xFF6633FF, /* 0x53: LCD_COLOR_CLUT8_RGB_6633FF */
	0xFF666600, /* 0x54: LCD_COLOR_CLUT8_RGB_666600 */
	0xFF666633, /* 0x55: LCD_COLOR_CLUT8_RGB_666633 */
	0xFF666666, /* 0x56: LCD_COLOR_CLUT8_RGB_666666 */
	0xFF666699, /* 0x57: LCD_COLOR_CLUT8_RGB_666699 */
	0xFF6666CC, /* 0x58: LCD_COLOR_CLUT8_RGB_6666CC */
	0xFF6666FF, /* 0x59: LCD_COLOR_CLUT8_RGB_6666FF */
	0xFF669900, /* 0x5A: LCD_COLOR_CLUT8_RGB_669900 */
	0xFF669933, /* 0x5B: LCD_COLOR_CLUT8_RGB_669933 */
	0xFF669966, /* 0x5C: LCD_COLOR_CLUT8_RGB_669966 */
	0xFF669999, /* 0x5D: LCD_COLOR_CLUT8_RGB_669999 */
	0xFF6699CC, /* 0x5E: LCD_COLOR_CLUT8_RGB_6699CC */
	0xFF6699FF, /* 0x5F: LCD_COLOR_CLUT8_RGB_6699FF */
	0xFF66CC00, /* 0x60: LCD_COLOR_CLUT8_RGB_66CC00 */
	0xFF66CC33, /* 0x61: LCD_COLOR_CLUT8_RGB_66CC33 */
	0xFF66CC66, /* 0x62: LCD_COLOR_CLUT8_RGB_66CC66 */
	0xFF66CC99, /* 0x63: LCD_COLOR_CLUT8_RGB_66CC99 */
	0xFF66CCCC, /* 0x64: LCD_COLOR_CLUT8_RGB_66CCCC */
	0xFF66CCFF, /* 0x65: LCD_COLOR_CLUT8_RGB_66CCFF */
	0xFF66FF00, /* 0x66: LCD_COLOR_CLUT8_RGB_66FF00 */
	0xFF66FF33, /* 0x67: LCD_COLOR_CLUT8_RGB_66FF33 */
	0xFF66FF66, /* 0x68: LCD_COLOR_CLUT8_RGB_66FF66 */
	0xFF66FF99, /* 0x69: LCD_COLOR_CLUT8_RGB_66FF99 */
	0xFF66FFCC, /* 0x6A: LCD_COLOR_CLUT8_RGB_66FFCC */
	0xFF66FFFF, /* 0x6B: LCD_COLOR_CLUT8_RGB_66FFFF */
	0xFF990000, /* 0x6C: LCD_COLOR_CLUT8_RGB_990000 */
	0xFF990033, /* 0x6D: LCD_COLOR_CLUT8_RGB_990033 */
	0xFF990066, /* 0x6E: LCD_COLOR_CLUT8_RGB_990066 */
	0xFF990099, /* 0x6F: LCD_COLOR_CLUT8_RGB_990099 */
	0xFF9900CC, /* 0x70: LCD_COLOR_CLUT8_RGB_9900CC */
	0xFF9900FF, /* 0x71: LCD_COLOR_CLUT8_PURPLE */
	0xFF993300, /* 0x72: LCD_COLOR_CLUT8_RGB_993300 */
	0xFF993333, /* 0x73: LCD_COLOR_CLUT8_RGB_993333 */
	0xFF993366, /* 0x74: LCD_COLOR_CLUT8_RGB_993366 */
	0xFF993399, /* 0x75: LCD_COLOR_CLUT8_RGB_993399 */
	0xFF9933CC, /* 0x76: LCD_COLOR_CLUT8_RGB_9933CC */
	0xFF9933FF, /* 0x77: LCD_COLOR_CLUT8_RGB_9933FF */
	0xFF996600, /* 0x78: LCD_COLOR_CLUT8_RGB_996600 */
	0xFF996633, /* 0x79: LCD_COLOR_CLUT8_RGB_996633 */
	0xFF996666, /* 0x7A: LCD_COLOR_CLUT8_RGB_996666 */
	0xFF996699, /* 0x7B: LCD_COLOR_CLUT8_RGB_996699 */
	0xFF9966CC, /* 0x7C: LCD_COLOR_CLUT8_RGB_9966CC */
	0xFF9966FF, /* 0x7D: LCD_COLOR_CLUT8_RGB_9966FF */
	0xFF999900, /* 0x7E: LCD_COLOR_CLUT8_RGB_999900 */
	0xFF999933, /* 0x7F: LCD_COLOR_CLUT8_RGB_999933 */
	0xFF999966, /* 0x80: LCD_COLOR_CLUT8_RGB_999966 */
	0xFF999999, /* 0x81: LCD_COLOR_CLUT8_RGB_999999 */
	0xFF9999CC, /* 0x82: LCD_COLOR_CLUT8_RGB_9999CC */
	0xFF9999FF, /* 0x83: LCD_COLOR_CLUT8_RGB_9999FF */
	0xFF99CC00, /* 0x84: LCD_COLOR_CLUT8_RGB_99CC00 */
	0xFF99CC33, /* 0x85: LCD_COLOR_CLUT8_RGB_99CC33 */
	0xFF99CC66, /* 0x86: LCD_COLOR_CLUT8_RGB_99CC66 */
	0xFF99CC99, /* 0x87: LCD_COLOR_CLUT8_RGB_99CC99 */
	0xFF99CCCC, /* 0x88: LCD_COLOR_CLUT8_RGB_99CCCC */
	0xFF99CCFF, /* 0x89: LCD_COLOR_CLUT8_RGB_99CCFF */
	0xFF99FF00, /* 0x8A: LCD_COLOR_CLUT8_RGB_99FF00 */
	0xFF99FF33, /* 0x8B: LCD_COLOR_CLUT8_RGB_99FF33 */
	0xFF99FF66, /* 0x8C: LCD_COLOR_CLUT8_RGB_99FF66 */
	0xFF99FF99, /* 0x8D: LCD_COLOR_CLUT8_RGB_99FF99 */
	0xFF99FFCC, /* 0x8E: LCD_COLOR_CLUT8_RGB_99FFCC */
	0xFF99FFFF, /* 0x8F: LCD_COLOR_CLUT8_RGB_99FFFF */
	0xFFCC0000, /* 0x90: LCD_COLOR_CLUT8_RGB_CC0000 */
	0xFFCC0033, /* 0x91: LCD_COLOR_CLUT8_RGB_CC0033 */
	0xFFCC0066, /* 0x92: LCD_COLOR_CLUT8_RGB_CC0066 */
	0xFFCC0099, /* 0x93: LCD_COLOR_CLUT8_RGB_CC0099 */
	0xFFCC00CC, /* 0x94: LCD_COLOR_CLUT8_RGB_CC00CC */
	0xFFCC00FF, /* 0x95: LCD_COLOR_CLUT8_RGB_CC00FF */
	0xFFCC3300, /* 0x96: LCD_COLOR_CLUT8_RGB_CC3300 */
	0xFFCC3333, /* 0x97: LCD_COLOR_CLUT8_RGB_CC3333 */
	0xFFCC3366, /* 0x98: LCD_COLOR_CLUT8_RGB_CC3366 */
	0xFFCC3399, /* 0x99: LCD_COLOR_CLUT8_RGB_CC3399 */
	0xFFCC33CC, /* 0x9A: LCD_COLOR_CLUT8_RGB_CC33CC */
	0xFFCC33FF, /* 0x9B: LCD_COLOR_CLUT8_RGB_CC33FF */
	0xFFCC6600, /* 0x9C: LCD_COLOR_CLUT8_RGB_CC6600 */
	0xFFCC6633, /* 0x9D: LCD_COLOR_CLUT8_RGB_CC6633 */
	0xFFCC6666, /* 0x9E: LCD_COLOR_CLUT8_RGB_CC6666 */
	0xFFCC6699, /* 0x9F: LCD_COLOR_CLUT8_RGB_CC6699 */
	0xFFCC66CC, /* 0xA0: LCD_COLOR_CLUT8_RGB_CC66CC */
	0xFFCC66FF, /* 0xA1: LCD_COLOR_CLUT8_RGB_CC66FF */
	0xFFCC9900, /* 0xA2: LCD_COLOR_CLUT8_RGB_CC9900 */
	0xFFCC9933, /* 0xA3: LCD_COLOR_CLUT8_RGB_CC9933 */
	0xFFCC9966, /* 0xA4: LCD_COLOR_CLUT8_RGB_CC9966 */
	0xFFCC9999, /* 0xA5: LCD_COLOR_CLUT8_RGB_CC9999 */
	0xFFCC99CC, /* 0xA6: LCD_COLOR_CLUT8_RGB_CC99CC */
	0xFFCC99FF, /* 0xA7: LCD_COLOR_CLUT8_RGB_CC99FF */
	0xFFCCCC00, /* 0xA8: LCD_COLOR_CLUT8_RGB_CCCC00 */
	0xFFCCCC33, /* 0xA9: LCD_COLOR_CLUT8_RGB_CCCC33 */
	0xFFCCCC66, /* 0xAA: LCD_COLOR_CLUT8_RGB_CCCC66 */
	0xFFCCCC99, /* 0xAB: LCD_COLOR_CLUT8_RGB_CCCC99 */
	0xFFCCCCCC, /* 0xAC: LCD_COLOR_CLUT8_RGB_CCCCCC */
	0xFFCCCCFF, /* 0xAD: LCD_COLOR_CLUT8_RGB_CCCCFF */
	0xFFCCFF00, /* 0xAE: LCD_COLOR_CLUT8_RGB_CCFF00 */
	0xFFCCFF33, /* 0xAF: LCD_COLOR_CLUT8_RGB_CCFF33 */
	0xFFCCFF66, /* 0xB0: LCD_COLOR_CLUT8_RGB_CCFF66 */
	0xFFCCFF99, /* 0xB1: LCD_COLOR_CLUT8_RGB_CCFF99 */
	0xFFCCFFCC, /* 0xB2: LCD_COLOR_CLUT8_RGB_CCFFCC */
	0xFFCCFFFF, /* 0xB3: LCD_COLOR_CLUT8_RGB_CCFFFF */
	0xFFFF0000, /* 0xB4: LCD_COLOR_CLUT8_RED */
	0xFFFF0033, /* 0xB5: LCD_COLOR_CLUT8_RGB_FF0033 */
	0xFFFF0066, /* 0xB6: LCD_COLOR_CLUT8_RGB_FF0066 */
	0xFFFF0099, /* 0xB7: LCD_COLOR_CLUT8_RGB_FF0099 */
	0xFFFF00CC, /* 0xB8: LCD_COLOR_CLUT8_RGB_FF00CC */
	0xFFFF00FF, /* 0xB9: LCD_COLOR_CLUT8_MAGENTA */
	0xFFFF3300, /* 0xBA: LCD_COLOR_CLUT8_RGB_FF3300 */
	0xFFFF3333, /* 0xBB: LCD_COLOR_CLUT8_RGB_FF3333 */
	0xFFFF3366, /* 0xBC: LCD_COLOR_CLUT8_RGB_FF3366 */
	0xFFFF3399, /* 0xBD: LCD_COLOR_CLUT8_RGB_FF3399 */
	0xFFFF33CC, /* 0xBE: LCD_COLOR_CLUT8_RGB_FF33CC */
	0xFFFF33FF, /* 0xBF: LCD_COLOR_CLUT8_RGB_FF33FF */
	0xFFFF6600, /* 0xC0: LCD_COLOR_CLUT8_ORANGE */
	0xFFFF6633, /* 0xC1: LCD_COLOR_CLUT8_RGB_FF6633 */
	0xFFFF6666, /* 0xC2: LCD_COLOR_CLUT8_RGB_FF6666 */
	0xFFFF6699, /* 0xC3: LCD_COLOR_CLUT8_RGB_FF6699 */
	0xFFFF66CC, /* 0xC4: LCD_COLOR_CLUT8_RGB_FF66CC */
	0xFFFF66FF, /* 0xC5: LCD_COLOR_CLUT8_RGB_FF66FF */
	0xFFFF9900, /* 0xC6: LCD_COLOR_CLUT8_RGB_FF9900 */
	0xFFFF9933, /* 0xC7: LCD_COLOR_CLUT8_RGB_FF9933 */
	0xFFFF9966, /* 0xC8: LCD_COLOR_CLUT8_RGB_FF9966 */
	0xFFFF9999, /* 0xC9: LCD_COLOR_CLUT8_RGB_FF9999 */
	0xFFFF99CC, /* 0xCA: LCD_COLOR_CLUT8_RGB_FF99CC */
	0xFFFF99FF, /* 0xCB: LCD_COLOR_CLUT8_RGB_FF99FF */
	0xFFFFCC00, /* 0xCC: LCD_COLOR_CLUT8_RGB_FFCC00 */
	0xFFFFCC33, /* 0xCD: LCD_COLOR_CLUT8_RGB_FFCC33 */
	0xFFFFCC66, /* 0xCE: LCD_COLOR_CLUT8_RGB_FFCC66 */
	0xFFFFCC99, /* 0xCF: LCD_COLOR_CLUT8_RGB_FFCC99 */
	0xFFFFCCCC, /* 0xD0: LCD_COLOR_CLUT8_RGB_FFCCCC */
	0xFFFFCCFF, /* 0xD1: LCD_COLOR_CLUT8_RGB_FFCCFF */
	0xFFFFFF00, /* 0xD2: LCD_COLOR_CLUT8_YELLOW */
	0xFFFFFF33, /* 0xD3: LCD_COLOR_CLUT8_RGB_FFFF33 */
	0xFFFFFF66, /* 0xD4: LCD_COLOR_CLUT8_RGB_FFFF66 */
	0xFFFFFF99, /* 0xD5: LCD_COLOR_CLUT8_RGB_FFFF99 */
	0xFFFFFFCC, /* 0xD6: LCD_COLOR_CLUT8_RGB_FFFFCC */
	0xFFFFFFFF, /* 0xD7: LCD_COLOR_CLUT8_WHITE */
	0xFF080808, /* 0xD8: LCD_COLOR_CLUT8_GREY_08 */
	0xFF0E0E0E, /* 0xD9: LCD_COLOR_CLUT8_GREY_0E */
	0xFF141414, /* 0xDA: LCD_COLOR_CLUT8_GREY_14 */
	0xFF1A1A1A, /* 0xDB: LCD_COLOR_CLUT8_GREY_1A */
	0xFF202020, /* 0xDC: LCD_COLOR_CLUT8_GREY_20 */
	0xFF262626, /* 0xDD: LCD_COLOR_CLUT8_GREY_26 */
	0xFF2C2C2C, /* 0xDE: LCD_COLOR_CLUT8_GREY_2C */
	0xFF323232, /* 0xDF: LCD_COLOR_CLUT8_GREY_32 */
	0xFF383838, /* 0xE0: LCD_COLOR_CLUT8_GREY_38 */
	0xFF3E3E3E, /* 0xE1: LCD_COLOR_CLUT8_GREY_3E */
	0xFF444444, /* 0xE2: LCD_COLOR_CLUT8_GREY_44 */
	0xFF4A4A4A, /* 0xE3: LCD_COLOR_CLUT8_GREY_4A */
	0xFF505050, /* 0xE4: LCD_COLOR_CLUT8_GREY_50 */
	0xFF565656, /* 0xE5: LCD_COLOR_CLUT8_GREY_56 */
	0xFF5C5C5C, /* 0xE6: LCD_COLOR_CLUT8_GREY_5C */
	0xFF626262, /* 0xE7: LCD_COLOR_CLUT8_GREY_62 */
	0xFF686868, /* 0xE8: LCD_COLOR_CLUT8_GREY_68 */
	0xFF6E6E6E, /* 0xE9: LCD_COLOR_CLUT8_GREY_6E */
	0xFF747474, /* 0xEA: LCD_COLOR_CLUT8_GREY_74 */
	0xFF7A7A7A, /* 0xEB: LCD_COLOR_CLUT8_GREY_7A */
	0xFF808080, /* 0xEC: LCD_COLOR_CLUT8_GREY */
	0xFF868686, /* 0xED: LCD_COLOR_CLUT8_GREY_86 */
	0xFF8C8C8C, /* 0xEE: LCD_COLOR_CLUT8_GREY_8C */
	0xFF929292, /* 0xEF: LCD_COLOR_CLUT8_GREY_92 */
	0xFF989898, /* 0xF0: LCD_COLOR_CLUT8_GREY_98 */
	0xFF9E9E9E, /* 0xF1: LCD_COLOR_CLUT8_GREY_9E */
	0xFFA4A4A4, /* 0xF2: LCD_COLOR_CLUT8_GREY_A4 */
	0xFFAAAAAA, /* 0xF3: LCD_COLOR_CLUT8_GREY_AA */
	0xFFB0B0B0, /* 0xF4: LCD_COLOR_CLUT8_GREY_B0 */
	0xFFB6B6B6, /* 0xF5: LCD_COLOR_CLUT8_GREY_B6 */
	0xFFBCBCBC, /* 0xF6: LCD_COLOR_CLUT8_GREY_BC */
	0xFFC2C2C2, /* 0xF7: LCD_COLOR_CLUT8_GREY_C2 */
	0xFFC8C8C8, /* 0xF8: LCD_COLOR_CLUT8_GREY_C8 */
	0xFFCECECE, /* 0xF9: LCD_COLOR_CLUT8_GREY_CE */
	0xFFD4D4D4, /* 0xFA: LCD_COLOR_CLUT8_GREY_D4 */
	0xFFDADADA, /* 0xFB: LCD_COLOR_CLUT8_GREY_DA */
	0xFFE0E0E0, /* 0xFC: LCD_COLOR_CLUT8_GREY_E0 */
	0xFFE6E6E6, /* 0xFD: LCD_COLOR_CLUT8_GREY_E6 */
	0xFFECECEC, /* 0xFE: LCD_COLOR_CLUT8_GREY_EC */
	0xFFF2F2F2, /* 0xFF: LCD_COLOR_CLUT8_GREY_F2 */
};
static display_clut_cfg_t s_defalut_clut8_cfg = {
	.p_base = (uint32_t *)sc_default_clut8,
	.size = 256,
	.start = 0,
};
#endif

static const uint32_t sc_lcd_model_name_len = sizeof(sc_lcd_model_name) / sizeof(sc_lcd_model_name[0]);

/*================================== Private Functions Prototypes =================================*/

static uint32_t convertToARGB1555(LCD_ColorConvertType *cct);
static uint32_t convertToARGB4444(LCD_ColorConvertType *cct);
static uint32_t convertToARGB8888(LCD_ColorConvertType *cct);
static uint32_t convertToRGB565(LCD_ColorConvertType *cct);
static uint32_t convertToRGB666(LCD_ColorConvertType *cct);
static uint32_t convertToRGB888(LCD_ColorConvertType *cct);
static void convertRGB565ToARGB4444(const uint16_t *src, uint32_t length, uint16_t *out, uint16_t alpha);
static void dirtyRectClear(LCD_DirtyRectType *rect);
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width);
static uint32_t prepareWriteBuffer(LCD_LayerType *draw_layer, bool overwrite_all);

/*================================== Public Functions =============================================*/

/**
 * @brief	颜色转换
 * @param	cct LCD_ColorConvertType 对象
 * @details	alpha 通道转换规则：
 * 			- 源格式有，目标格式没有：按源格式的 alpha 值计算源格式 RGB 百分比，再按此百分比计算目标格式的 RGB
 * 			- 源格式有，目标格式也有：
 * 				- cct->alpha < 0：以移位操作快速获取新 alpha
 * 				- cct->alpha >=0：按 cct->alpha 设置目标格式 alpha
 * 			- 源格式没有，目标格式有：
 * 				- cct->alpha < 0：按不透明处理
 * 				- cct->alpha >=0：按 cct->alpha 设置目标格式 alpha
 */
uint32_t LCD_ConvetColor(LCD_ColorConvertType *cct)
{
	uint32_t err = FSP_SUCCESS;

	IS_INVALID_POINT(cct);
	IS_INVALID_POINT(cct->src);
	IS_INVALID_POINT(cct->out);

	switch (cct->out_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		err = convertToARGB1555(cct);
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		err = convertToARGB4444(cct);
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		err = convertToARGB8888(cct);
		break;
	case LCD_COLOR_FORMAT_RGB565:
		err = convertToRGB565(cct);
		break;
	case LCD_COLOR_FORMAT_RGB666:
		err = convertToRGB666(cct);
		break;
	case LCD_COLOR_FORMAT_RGB888:
		err = convertToRGB888(cct);
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		break;
	}

	return err;
}

/**
 * @brief	绘制 bitmap
 * @param	lcd    LCD_DeviceType 对象
 * @param	layer  指定要绘制在哪一层
 * @param	bitmap LCD_BitmapType 对象
 * @note	不进行格式转换，务必确保传入的图像格式和显示层的格式一致
 */
uint32_t LCD_DrawBitmap(LCD_DeviceType *lcd, uint8_t layer, LCD_BitmapType *bitmap)
{
	uint16_t i;
	uint32_t err;
	LCD_BitmapType real_bitmap;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint8_t *p8_bg = NULL;
	VOLATILE uint8_t *p8_bp = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint16_t *p16_bp = NULL;
	VOLATILE uint32_t *p32_bg = NULL;
	VOLATILE uint32_t *p32_bp = NULL;

	if (lcd->drawBitmap != NULL) {
		return lcd->drawBitmap(lcd, layer, bitmap);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_OUT_OF_LAYER_RANGE(bitmap->x, bitmap->y, draw_layer);
	IS_INVALID_POSITION(draw_layer, bitmap->x, bitmap->y);
	IS_NONEED_PROCESS_2(bitmap->length, bitmap->width);

	memcpy(&real_bitmap, bitmap, sizeof(LCD_BitmapType));
	if ((real_bitmap.x + real_bitmap.length) > draw_layer->x_length) {
		real_bitmap.length = draw_layer->x_length - real_bitmap.x;
	}
	if ((real_bitmap.y + real_bitmap.width) > draw_layer->y_length) {
		real_bitmap.width = draw_layer->y_length - real_bitmap.y;
	}

    err = prepareWriteBuffer(draw_layer, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (draw_layer->color_format) {
    case LCD_COLOR_FORMAT_ARGB1555:
    case LCD_COLOR_FORMAT_ARGB4444:
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (uint16_t *)draw_layer->graphic_writeable;
    	p16_bp = (uint16_t *)real_bitmap.bitmap;
    	p16_bg = &p16_bg[real_bitmap.y * (draw_layer->x_length + draw_layer->x_dummy) + real_bitmap.x];
    	for (i = 0; i < real_bitmap.width; i++) {
    		memcpy(p16_bg, p16_bp, real_bitmap.length * 2);
    		p16_bg = &p16_bg[draw_layer->x_length + draw_layer->x_dummy];
    		p16_bp = &p16_bp[real_bitmap.length];
    	}
    	break;
    case LCD_COLOR_FORMAT_ARGB8888:
    case LCD_COLOR_FORMAT_RGB888:
    	p32_bg = (uint32_t *)draw_layer->graphic_writeable;
		p32_bp = (uint32_t *)real_bitmap.bitmap;
    	p32_bg = &p32_bg[real_bitmap.y * (draw_layer->x_length + draw_layer->x_dummy) + real_bitmap.x];
    	for (i = 0; i < real_bitmap.width; i++) {
    		memcpy(p32_bg, p32_bp, real_bitmap.length * 4);
    		p32_bg = &p32_bg[draw_layer->x_length + draw_layer->x_dummy];
    		p32_bp = &p32_bp[real_bitmap.length];
    	}
    	break;
    case LCD_COLOR_FORMAT_CLUT8:
    	p8_bg = draw_layer->graphic_writeable;
    	p8_bp = (uint8_t *)real_bitmap.bitmap;
    	p8_bg = &p8_bg[real_bitmap.y * (draw_layer->x_length + draw_layer->x_dummy) + real_bitmap.x];
    	for (i = 0; i < real_bitmap.width; i++) {
    		memcpy(p8_bg, p8_bp, real_bitmap.length);
    		p8_bg = &p8_bg[draw_layer->x_length + draw_layer->x_dummy];
    		p8_bp = &p8_bp[real_bitmap.length];
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&draw_layer->dirty_current, real_bitmap.x, real_bitmap.y, real_bitmap.length, real_bitmap.width);

	return err;
}

uint32_t LCD_DrawHLine(LCD_DeviceType *lcd, uint8_t layer, LCD_HLineType *hline)
{
	uint16_t i;
	uint16_t length;
	uint32_t err;
	uint32_t start;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(hline);

	if (lcd->drawHLine != NULL) {
		return lcd->drawHLine(lcd, layer, hline);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, hline->x, hline->y);
	IS_NONEED_PROCESS_1(hline->length);

	err = prepareWriteBuffer(draw_layer, false);
	UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

	if ((hline->x + hline->length) > draw_layer->x_length) {
		length = draw_layer->x_length - hline->x;
	}
	else {
		length = hline->length;
	}

	start = hline->y * (draw_layer->x_length + draw_layer->x_dummy) + hline->x;
	switch (draw_layer->color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_RGB565:
		p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
		for (i = 0; i < length; i++) {
			p16_bg[start + i] = (uint16_t)hline->color;
		}
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
		for (i = 0; i < length; i++) {
			p32_bg[start + i] = hline->color;
		}
		break;
	case LCD_COLOR_FORMAT_CLUT8:
		for (i = 0; i < length; i++) {
			draw_layer->graphic_writeable[start + i] = (uint8_t)hline->color;
		}
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	dirtyRectMerge(&draw_layer->dirty_current, hline->x, hline->y, length, 1);

	return err;
}

/**
 * @brief	绘制包含起点和终点的直线
 * @details	起点和终点均须位于图层范围内；使用整数 Bresenham 算法，绘制后登记脏区域。
 * @param	lcd   LCD 设备实例
 * @param	layer 图层编号，1 或 2
 * @param	line  起点、终点和颜色
 * @return	FSP 错误码
 */
uint32_t LCD_DrawLine(LCD_DeviceType *lcd, uint8_t layer, LCD_LineType *line)
{
	int32_t dx;
	int32_t dy;
	int32_t error;
	int32_t error_twice;
	int32_t step_x;
	int32_t step_y;
	int32_t x;
	int32_t y;
	uint16_t left;
	uint16_t top;
	uint32_t err;
	uint32_t start;
	uint32_t stride;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint8_t *p8_bg = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(line);

	if (lcd->drawLine != NULL) {
		return lcd->drawLine(lcd, layer, line);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, line->x_start, line->y_start);
	IS_INVALID_POSITION(draw_layer, line->x_end, line->y_end);

	err = prepareWriteBuffer(draw_layer, false);
	UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

	switch (draw_layer->color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_RGB565:
		p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
		break;
	case LCD_COLOR_FORMAT_CLUT8:
		p8_bg = (VOLATILE uint8_t *)draw_layer->graphic_writeable;
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	x = line->x_start;
	y = line->y_start;
	dx = line->x_end >= line->x_start ? (int32_t)line->x_end - line->x_start : (int32_t)line->x_start - line->x_end;
	dy = line->y_end >= line->y_start ? (int32_t)line->y_end - line->y_start : (int32_t)line->y_start - line->y_end;
	step_x = line->x_start < line->x_end ? 1 : -1;
	step_y = line->y_start < line->y_end ? 1 : -1;
	error = dx - dy;
	stride = (uint32_t)draw_layer->x_length + draw_layer->x_dummy;

	while (true) {
		start = (uint32_t)y * stride + (uint32_t)x;
		if (p8_bg != NULL) {
			p8_bg[start] = (uint8_t)line->color;
		}
		else if (p16_bg != NULL) {
			p16_bg[start] = (uint16_t)line->color;
		}
		else {
			p32_bg[start] = line->color;
		}
		if ((x == line->x_end) && (y == line->y_end)) {
			break;
		}

		error_twice = error * 2;
		if (error_twice >= -dy) {
			error -= dy;
			x += step_x;
		}
		if (error_twice <= dx) {
			error += dx;
			y += step_y;
		}
	}

	left = line->x_start < line->x_end ? line->x_start : line->x_end;
	top = line->y_start < line->y_end ? line->y_start : line->y_end;
	dirtyRectMerge(&draw_layer->dirty_current, left, top, (uint16_t)(dx + 1), (uint16_t)(dy + 1));

	return err;
}

/**
 * @brief	绘制四边厚度相同的空心矩形
 * @details	length/width 为外框水平/垂直像素数，边框向内延伸，内部像素保持原样。
 * 		外框左上角须位于图层内；右侧和底部超出图层时按原始边框位置裁剪。
 * @param	lcd       LCD 设备实例
 * @param	layer     图层编号，1 或 2
 * @param	rectangle 外框位置、尺寸、边框厚度和颜色
 * @retval	FSP_SUCCESS              绘制完成并登记脏区域
 * @retval	FSP_ERR_INVALID_ARGUMENT 厚度为 0，或任一外框尺寸不大于厚度的两倍
 * @return	其它 FSP 错误码表示绘制失败
 */
uint32_t LCD_DrawOutlineRectangle(LCD_DeviceType *lcd, uint8_t layer, LCD_OutlineRectangleType *rectangle)
{
	bool middle_row;
	uint32_t border_twice;
	uint32_t bottom;
	uint32_t column;
	uint32_t err;
	uint32_t inner_bottom;
	uint32_t inner_left;
	uint32_t inner_right;
	uint32_t inner_top;
	uint32_t left_end;
	uint32_t right;
	uint32_t row;
	uint32_t start;
	uint32_t stride;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint8_t *p8_bg = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(rectangle);

	border_twice = (uint32_t)rectangle->border_width * 2U;
	if ((rectangle->border_width == 0) || (border_twice >= rectangle->length) || (border_twice >= rectangle->width)) {
		return FSP_ERR_INVALID_ARGUMENT;
	}
	if (lcd->drawOutlineRectangle != NULL) {
		return lcd->drawOutlineRectangle(lcd, layer, rectangle);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, rectangle->x, rectangle->y);

	switch (draw_layer->color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_RGB565:
		p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
		break;
	case LCD_COLOR_FORMAT_CLUT8:
		p8_bg = (VOLATILE uint8_t *)draw_layer->graphic_writeable;
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	/* 先按原始外框计算内边界，再裁剪可见范围，避免在裁剪处生成新的边框。 */
	right = (uint32_t)rectangle->x + rectangle->length;
	bottom = (uint32_t)rectangle->y + rectangle->width;
	inner_left = (uint32_t)rectangle->x + rectangle->border_width;
	inner_top = (uint32_t)rectangle->y + rectangle->border_width;
	inner_right = right - rectangle->border_width;
	inner_bottom = bottom - rectangle->border_width;
	if (right > draw_layer->x_length) {
		right = draw_layer->x_length;
	}
	if (bottom > draw_layer->y_length) {
		bottom = draw_layer->y_length;
	}

	err = prepareWriteBuffer(draw_layer, false);
	UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

	stride = (uint32_t)draw_layer->x_length + draw_layer->x_dummy;
	for (row = rectangle->y; row < bottom; row++) {
		middle_row = (row >= inner_top) && (row < inner_bottom);
		left_end = middle_row && (inner_left < right) ? inner_left : right;
		start = row * stride;
		for (column = rectangle->x; column < left_end; column++) {
			if (p8_bg != NULL) {
				p8_bg[start + column] = (uint8_t)rectangle->color;
			}
			else if (p16_bg != NULL) {
				p16_bg[start + column] = (uint16_t)rectangle->color;
			}
			else {
				p32_bg[start + column] = rectangle->color;
			}
		}
		if (middle_row) {
			for (column = inner_right; column < right; column++) {
				if (p8_bg != NULL) {
					p8_bg[start + column] = (uint8_t)rectangle->color;
				}
				else if (p16_bg != NULL) {
					p16_bg[start + column] = (uint16_t)rectangle->color;
				}
				else {
					p32_bg[start + column] = rectangle->color;
				}
			}
		}
	}

	dirtyRectMerge(&draw_layer->dirty_current, rectangle->x, rectangle->y,
			(uint16_t)(right - rectangle->x), (uint16_t)(bottom - rectangle->y));

	return err;
}

uint32_t LCD_DrawPoint(LCD_DeviceType *lcd, uint8_t layer, uint16_t x, uint16_t y, uint32_t color)
{
	uint32_t err;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	if (lcd->drawPoint != NULL) {
		return lcd->drawPoint(lcd, layer, x, y, color);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, x, y);

	err = prepareWriteBuffer(draw_layer, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (draw_layer->color_format) {
    case LCD_COLOR_FORMAT_ARGB1555:
    case LCD_COLOR_FORMAT_ARGB4444:
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
    	p16_bg[y * (draw_layer->x_length + draw_layer->x_dummy) + x] = (uint16_t)color;
    	break;
    case LCD_COLOR_FORMAT_ARGB8888:
    case LCD_COLOR_FORMAT_RGB888:
    	p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
    	p32_bg[y * (draw_layer->x_length + draw_layer->x_dummy) + x] = color;
    	break;
    case LCD_COLOR_FORMAT_CLUT8:
    	draw_layer->graphic_writeable[y * (draw_layer->x_length + draw_layer->x_dummy) + x] = (uint8_t)color;
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&draw_layer->dirty_current, x, y, 1, 1);

	return err;
}

uint32_t LCD_DrawVLine(LCD_DeviceType *lcd, uint8_t layer, LCD_VLineType *vline)
{
	uint32_t i;
	uint32_t length;
	uint32_t err;
	uint32_t start;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint8_t *p8_bg = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(vline);

	if (lcd->drawVLine != NULL) {
		return lcd->drawVLine(lcd, layer, vline);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, vline->x, vline->y);
	IS_NONEED_PROCESS_1(vline->length);

	err = prepareWriteBuffer(draw_layer, false);
	UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

	if ((vline->y + vline->length) > draw_layer->y_length) {
		length = draw_layer->y_length - vline->y;
	}
	else {
		length = vline->length;
	}

	start = vline->y * (draw_layer->x_length + draw_layer->x_dummy) + vline->x;
	switch (draw_layer->color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_RGB565:
		p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
		for (i = 0; i < length; i++) {
			p16_bg[start + i * (draw_layer->x_length + draw_layer->x_dummy)] = (uint16_t)vline->color;
		}
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
		for (i = 0; i < length; i++) {
			p32_bg[start + i * (draw_layer->x_length + draw_layer->x_dummy)] = vline->color;
		}
		break;
	case LCD_COLOR_FORMAT_CLUT8:
		p8_bg = (VOLATILE uint8_t *)draw_layer->graphic_writeable;
		for (i = 0; i < length; i++) {
			p8_bg[start + i * (draw_layer->x_length + draw_layer->x_dummy)] = (uint8_t)vline->color;
		}
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	dirtyRectMerge(&draw_layer->dirty_current, vline->x, vline->y, 1, (uint16_t)length);

	return err;
}

uint32_t LCD_Fill(LCD_DeviceType *lcd, uint8_t layer, uint32_t color)
{
	uint32_t i;
    uint32_t err;
    uint16_t total_length;
    uint16_t total_width;
    uint32_t total;

    LCD_LayerType *draw_layer = NULL;
    VOLATILE uint8_t *p8_bg = NULL;
    VOLATILE uint16_t *p16_bg = NULL;
    VOLATILE uint32_t *p32_bg = NULL;

    if (lcd->fill != NULL) {
        return lcd->fill(lcd, layer, color);
    }

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	total_length = draw_layer->x_length + draw_layer->x_dummy;
	total_width = draw_layer->y_length + draw_layer->y_dummy;
	total = total_length * total_width;

	err = prepareWriteBuffer(draw_layer, true);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (draw_layer->color_format) {
    case LCD_COLOR_FORMAT_ARGB1555:
    case LCD_COLOR_FORMAT_ARGB4444:
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p16_bg[i] = (uint16_t)color;
    	}
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    case LCD_COLOR_FORMAT_ARGB8888:
    	p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p32_bg[i] = color;
    	}
    	break;
    case LCD_COLOR_FORMAT_CLUT8:
    	p8_bg = (VOLATILE uint8_t *)draw_layer->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p8_bg[i] = (uint8_t)color;
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&draw_layer->dirty_current, 0, 0, total_length, total_width);

    return err;
}

uint32_t LCD_FillRectangle(LCD_DeviceType *lcd, uint8_t layer, LCD_RectangelType *rectangle)
{
	uint32_t i, j;
	uint32_t err;
	LCD_RectangelType real_rectangle;

	LCD_LayerType *draw_layer = NULL;
	VOLATILE uint8_t *p8_bg = NULL;
	VOLATILE uint16_t *p16_bg = NULL;
	VOLATILE uint32_t *p32_bg = NULL;

	if (lcd->fillRectangle != NULL) {
		return lcd->fillRectangle(lcd, layer, rectangle);
	}

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);
	IS_INVALID_POSITION(draw_layer, rectangle->x, rectangle->y);
	IS_NONEED_PROCESS_2(rectangle->length, rectangle->width);

	memcpy(&real_rectangle, rectangle, sizeof(LCD_RectangelType));
	if ((real_rectangle.x + real_rectangle.length) > draw_layer->x_length) {
		real_rectangle.length = draw_layer->x_length - real_rectangle.x;
	}
	if ((real_rectangle.y + real_rectangle.width) > draw_layer->y_length) {
		real_rectangle.width = draw_layer->y_length - real_rectangle.y;
	}

    err = prepareWriteBuffer(draw_layer, false);
    UNLIKE_RETURN(err, 0, "prepareWriteBuffer() failed: %" PRIu32, err);

    switch (draw_layer->color_format) {
    case LCD_COLOR_FORMAT_ARGB1555:
    case LCD_COLOR_FORMAT_ARGB4444:
    case LCD_COLOR_FORMAT_RGB565:
    	p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
    	p16_bg = &p16_bg[real_rectangle.y * (draw_layer->x_length + draw_layer->x_dummy)];
    	for (i = 0; i < real_rectangle.width; i++) {
    		for (j = real_rectangle.x; j < (real_rectangle.x + real_rectangle.length); j++) {
    			p16_bg[j] = (uint16_t)rectangle->color;
    		}
    		p16_bg = &p16_bg[draw_layer->x_length + draw_layer->x_dummy];
    	}
    	break;
    case LCD_COLOR_FORMAT_RGB888:
    case LCD_COLOR_FORMAT_ARGB8888:
    	p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
    	p32_bg = &p32_bg[real_rectangle.y * (draw_layer->x_length + draw_layer->x_dummy)];
    	for (i = 0; i < real_rectangle.width; i++) {
    		for (j = real_rectangle.x; j < (real_rectangle.x + real_rectangle.length); j++) {
    			p32_bg[j] = rectangle->color;
    		}
    		p32_bg = &p32_bg[draw_layer->x_length + draw_layer->x_dummy];
    	}
    	break;
    case LCD_COLOR_FORMAT_CLUT8:
    	p8_bg = (VOLATILE uint8_t *)draw_layer->graphic_writeable;
    	p8_bg = &p8_bg[real_rectangle.y * (draw_layer->x_length + draw_layer->x_dummy)];
    	for (i = 0; i < real_rectangle.width; i++) {
    		for (j = real_rectangle.x; j < (real_rectangle.x + real_rectangle.length); j++) {
    			p8_bg[j] = (uint8_t)rectangle->color;
    		}
    		p8_bg = &p8_bg[draw_layer->x_length + draw_layer->x_dummy];
    	}
    	break;
    default:
    	return FSP_ERR_UNSUPPORTED;
    	break;
    }

    dirtyRectMerge(&draw_layer->dirty_current, real_rectangle.x, real_rectangle.y, real_rectangle.length, real_rectangle.width);

	return err;
}

uint32_t LCD_Initialize(LCD_DeviceType *lcd, LCD_ConfigType *cfg)
{
    uint32_t i, err;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(cfg);

    if ((cfg->peri_1 == NULL) && (cfg->peri_2 == NULL)) {
    	LCD_LOGE("Both peri_1 and peri_2 is NULL");
    	return FSP_ERR_ASSERTION;
    }
    if ((cfg->peri_model_1 == LCD_PERI_UNUSED) && (cfg->peri_model_2 == LCD_PERI_UNUSED)) {
    	LCD_LOGE("At least special a peri");
    	return FSP_ERR_ASSERTION;
    }

    lcd->peri_1 = cfg->peri_1;
    lcd->peri_2 = cfg->peri_2;
    lcd->peri_model_1 = cfg->peri_model_1;
    lcd->peri_model_2 = cfg->peri_model_2;
#if LCD_EN_PERI_I2C
    lcd->iicWrite = cfg->iicWrite;
    lcd->iicRead = cfg->iicRead;
#endif

    if (lcd->peri_1 == NULL) {
    	lcd->peri_1 = lcd->peri_2;
    	lcd->peri_model_1 = lcd->peri_model_2;
    }
    if (lcd->peri_2 == NULL) {
    	lcd->peri_2 = lcd->peri_1;
    	lcd->peri_model_2 = lcd->peri_model_1;
    }

#if LCD_EN_PERI_GLCDC
	lcd->layer_1.runtime_cfg = NULL;
	lcd->layer_2.runtime_cfg = NULL;
#endif

#if LCD_EN_PERI_MIPI
	/* 确保 peri_1 指向的是 GLCDC 的实例 */
	if ((lcd->peri_model_1 == LCD_PERI_MIPI) && (lcd->peri_model_2 == LCD_PERI_GLCDC)) {
		void const *t = lcd->peri_1;
		lcd->peri_1 = lcd->peri_2;
		lcd->peri_2 = t;

		LCD_PeriModelEnum t_model = lcd->peri_model_1;
		lcd->peri_model_1 = lcd->peri_model_2;
		lcd->peri_model_2 = t_model;
	}
#endif

	/* 以下接口都用 lcd.c 提供的默认实现，若某个屏幕有自定义实现，在下面的 switch 块中添加
	 * 以下接口必须由各屏幕模块自己实现，若不支持，则设为 NULL
	 * - init
	 * - setBackLight */
	lcd->drawBitmap = NULL;
	lcd->drawHLine = NULL;
	lcd->drawLine = NULL;
	lcd->drawOutlineRectangle = NULL;
	lcd->drawPoint = NULL;
	lcd->drawVLine = NULL;
	lcd->fill = NULL;
	lcd->fillRectangle = NULL;
	lcd->isIdle = NULL;
	lcd->moveLayer = NULL;
	lcd->present = NULL;
	lcd->setDefaultCLUT = NULL;
	lcd->updateLayer1Mem = NULL;
	lcd->updateLayer2Mem = NULL;

    switch (cfg->model) {
#if LCD_EN_EK79001
    case LCD_MODEL_EK79001:
        lcd->model = LCD_MODEL_EK79001;
        lcd->init = LCD_EK79001_Init;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_EK79007AD2
    case LCD_MODEL_EK79007AD2:
        lcd->model = LCD_MODEL_EK79007AD2;
        lcd->init = LCD_EK79007AD2_Init;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_ILI9881C
    case LCD_MODEL_ILI9881C:
        lcd->model = LCD_MODEL_ILI9881C;
        lcd->init = LCD_ILI9881C_Init;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_JD9365HD
    case LCD_MODEL_JD9365HD:
        lcd->model = LCD_MODEL_JD9365HD;
		LCD_LOGE("Not support JD9365HD yet");
        return FSP_ERR_UNSUPPORTED;
#endif
#if LCD_EN_ST7796U
    case LCD_MODEL_ST7796U:
        lcd->model = LCD_MODEL_ST7796U;
        lcd->drawBitmap = LCD_ST7796U_DrawBitmap;
        lcd->drawPoint = LCD_ST7796U_DrawPoint;
        lcd->fill = LCD_ST7796U_Fill;
        lcd->fillRectangle = LCD_ST7796U_FillRectangle;
        lcd->init = LCD_ST7796U_Init;
        lcd->isIdle = LCD_ST7796U_IsIdle;
        lcd->present = LCD_ST7796U_Present;
        lcd->setBackLight = NULL;
        break;
#endif
#if LCD_EN_TFP410PAPR
    case LCD_MODEL_TFP410PAPR:
        lcd->model = LCD_MODEL_TFP410PAPR;
        lcd->init = LCD_TFP410_Init;
        lcd->setBackLight = NULL;
        break;
#endif
    default:
        return FSP_ERR_UNSUPPORTED;
    }

    err = lcd->init(lcd);
    UNLIKE_RETURN(err, 0, "LCD init failed: %" PRIu32, err);

	for (i = 0; i < sc_lcd_model_name_len; i++) {
        if (lcd->model == sc_lcd_model_name[i].model) {
            lcd->name = sc_lcd_model_name[i].name;
            break;
        }
    }

    lcd->vsync_cnt = 0;
#if LCD_EN_PERI_GLCDC
    lcd->gr1_underflow_cnt = 0;
    lcd->gr2_underflow_cnt = 0;

	if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		display_instance_t *p_inst = (display_instance_t *)lcd->peri_1;
		display_runtime_cfg_t *p_runtime = (display_runtime_cfg_t *)lcd->layer_1.runtime_cfg;
		if (p_runtime != NULL) {
			memcpy(&p_runtime->input, &p_inst->p_cfg->input[0], sizeof(display_input_cfg_t));
			memcpy(&p_runtime->layer, &p_inst->p_cfg->layer[0], sizeof(display_layer_t));
			p_runtime->input.p_base = (uint32_t *)lcd->layer_1.graphic_displayed;
		}
		__DMB();
		p_runtime = (display_runtime_cfg_t *)lcd->layer_2.runtime_cfg;
		if (p_runtime != NULL) {
			memcpy(&p_runtime->input, &p_inst->p_cfg->input[1], sizeof(display_input_cfg_t));
			memcpy(&p_runtime->layer, &p_inst->p_cfg->layer[1], sizeof(display_layer_t));
			p_runtime->input.p_base = (uint32_t *)lcd->layer_2.graphic_displayed;
		}
	}
#endif

#if LCD_EN_PERI_MIPI
    lcd->mipi_timing_err_cnt = 0;
    lcd->mipi_underflow_cnt = 0;
    lcd->mipi_overflow_cnt = 0;
#endif

    if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_CLUT8) {
    	LCD_SetDefaultCLUT(lcd, 1);
    }
    if (lcd->layer_2.color_format == LCD_COLOR_FORMAT_CLUT8) {
    	LCD_SetDefaultCLUT(lcd, 2);
    }

	if ((lcd->layer_1.graphic_mem_1 != NULL) || (lcd->layer_1.graphic_mem_2 != NULL)) {
		LCD_LOGD("Layer 1 configuration:");
		LCD_LOGD("X length: %" PRIu32, lcd->layer_1.x_length);
		LCD_LOGD("X dummy : %" PRIu32, lcd->layer_1.x_dummy);
		LCD_LOGD("Y length: %" PRIu32, lcd->layer_1.y_length);
		LCD_LOGD("Y dummy : %" PRIu32, lcd->layer_1.y_dummy);
	}

	if ((lcd->layer_2.graphic_mem_1 != NULL) || (lcd->layer_2.graphic_mem_2 != NULL)) {
		LCD_LOGD("Layer 2 configuration:");
		LCD_LOGD("X length: %" PRIu32, lcd->layer_2.x_length);
		LCD_LOGD("X dummy : %" PRIu32, lcd->layer_2.x_dummy);
		LCD_LOGD("Y length: %" PRIu32, lcd->layer_2.y_length);
		LCD_LOGD("Y dummy : %" PRIu32, lcd->layer_2.y_dummy);
	}

    return err;
}

/**
 * @brief	查询是否没有尚未完成的显示提交，已登记但未提交的目标不影响空闲状态
 * @param	lcd   LCD 实例
 * @param	layer 要查询的层
 * @param	idle  返回空闲状态；空闲不表示 GLCDC 已停止扫描
 * @return	FSP_SUCCESS 成功，其它为错误
 */
uint32_t LCD_IsIdle(LCD_DeviceType *lcd, uint8_t layer, bool *idle)
{
	uint32_t err = FSP_SUCCESS;

	IS_INVALID_POINT(lcd);
	IS_INVALID_POINT(idle);

	if (lcd->isIdle != NULL) {
		return lcd->isIdle(lcd, layer, idle);
	}

#if LCD_EN_PERI_GLCDC
	IS_UNSUPPORT_PERI(lcd->peri_model_1);

	FSP_CRITICAL_SECTION_DEFINE;
	FSP_CRITICAL_SECTION_ENTER;
	if (layer == 1) {
		*idle = (lcd->layer_1.present_pending == false) && (R_GLCDC->GR[DISPLAY_FRAME_LAYER_1].VEN_b.PVEN == 0U);
	}
	else if (layer == 2) {
		*idle = (lcd->layer_2.present_pending == false) && (R_GLCDC->GR[DISPLAY_FRAME_LAYER_2].VEN_b.PVEN == 0U);
	}
	else {
		err = FSP_ERR_UNSUPPORTED;
	}
	FSP_CRITICAL_SECTION_EXIT;
#else
	*idle = false;
#endif

	return err;
}

/**
 * @brief	调整指定层相对于屏幕原点(0, 0)的位置
 * @param	lcd   LCD 实例
 * @param	layer 要更新的 GLCDC 层
 * @param	x     新的 X 位置
 * @param	y     新的 Y 位置
 * @retval	FSP_SUCCESS: 成功，将再下一次 vsync 时真正切换
 * @retval	FSP_ERR_IN_USE: 有待生效的提交
 * @retval	FSP_ERR_INVALID_UPDATE_TIMING: GLCDC 当前不能更新
 * @note	在 GLCDC 更新成功时，将自动置位 present_pending，不用再调用 LCD_Present()，这也意味着在 present_pending
 * 			被清理前，不能再进行绘制、更新图层 
 */
uint32_t LCD_MoveLayer(LCD_DeviceType *lcd, uint8_t layer, int16_t x, int16_t y)
{
	uint32_t err;

	IS_INVALID_POINT(lcd);

	if (lcd->moveLayer != NULL) {
		return lcd->moveLayer(lcd, layer, x, y);
	}

#if LCD_EN_PERI_GLCDC
	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	IS_INVALID_POINT(lcd->peri_1);

	LCD_LayerType *draw_layer = NULL;
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);

	if (draw_layer->present_pending || R_GLCDC->GR[draw_layer->index - 1].VEN_b.PVEN) {
		return FSP_ERR_IN_USE;
	}

	display_frame_layer_t l = layer == 1 ? DISPLAY_FRAME_LAYER_1 : DISPLAY_FRAME_LAYER_2;
	const display_instance_t *p_inst = (const display_instance_t *)lcd->peri_1;
	display_runtime_cfg_t *p_runtime = (display_runtime_cfg_t *)draw_layer->runtime_cfg;
	if ((x == p_runtime->layer.coordinate.x) && (y == p_runtime->layer.coordinate.y)) {
		LCD_LOGI("New coordinate (%d, %d) equal to old (%d, %d), ignore", x, y, p_runtime->layer.coordinate.x, p_runtime->layer.coordinate.y);
		return FSP_SUCCESS;
	}

	FSP_CRITICAL_SECTION_DEFINE;
	FSP_CRITICAL_SECTION_ENTER;

	p_runtime->layer.coordinate.x = x;
	p_runtime->layer.coordinate.y = y;
	err = p_inst->p_api->layerChange(p_inst->p_ctrl, p_runtime, l);
	if (err == FSP_SUCCESS) {
		draw_layer->present_pending = true;
	}
	__DMB();

	FSP_CRITICAL_SECTION_EXIT;
#else
	err = FSP_ERR_UNSUPPORTED;
#endif

	return err;
}

/**
 * @brief	提交已登记的显示目标，或提交当前可写缓冲中的新绘制
 * @note	成功提交后等待 LCD_IsIdle() 返回空闲；提交失败时保留候选及脏状态供重试。
 * @param	lcd   LCD 实例
 * @param	layer 要提交的层
 * @return	FSP_SUCCESS 成功提交或没有新画面，FSP_ERR_IN_USE 上次提交或硬件更新尚未完成，其它为错误
 */
uint32_t LCD_Present(LCD_DeviceType *lcd, uint8_t layer)
{
	uint32_t err;
	IS_INVALID_POINT(lcd);

#if LCD_EN_PERI_GLCDC
	LCD_LayerType *draw_layer = NULL;

	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	IS_INVALID_POINT(lcd->peri_1);
	GET_DRAW_LAYER(draw_layer, layer);
	IS_INVALID_LAYER(draw_layer);

	uint8_t *submitted;
	bool registered;
	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	FSP_CRITICAL_SECTION_DEFINE;

	FSP_CRITICAL_SECTION_ENTER;

	if (draw_layer->present_pending || (R_GLCDC->GR[draw_layer->index - 1].VEN_b.PVEN != 0U)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_IN_USE;
	}

	submitted = draw_layer->graphic_to_present;
	registered = (submitted != NULL);
	if (registered == false) {
		if (draw_layer->dirty_current.valid == false) {
			/* 没有候选或新绘制时，不切换到尚未同步的后台。 */
			FSP_CRITICAL_SECTION_EXIT;
			return FSP_SUCCESS;
		}
		submitted = draw_layer->graphic_writeable;
	}
	if (submitted == NULL) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_ASSERTION;
	}

	/* 单缓冲更换地址和只读候选也必须通过 GLCDC 提交。 */
	__DSB();
	err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, submitted, draw_layer->index - 1);
	if (err != FSP_SUCCESS) {
		FSP_CRITICAL_SECTION_EXIT;
		if (err == FSP_ERR_INVALID_UPDATE_TIMING) {
			return FSP_ERR_IN_USE;
		}
		else {
			LCD_LOGE("Buffer change failed: %" PRIu32, err);
			return err;
		}
	}

	if (registered) {
		/* 外部完整画面与原前台可能整帧不同，包括由候选复制后叠加绘图的画面。 */
		dirtyRectClear(&draw_layer->dirty_submitted);
		dirtyRectMerge(&draw_layer->dirty_submitted, 0, 0, draw_layer->x_length + draw_layer->x_dummy, draw_layer->y_length + draw_layer->y_dummy);
	}
	else {
		draw_layer->dirty_submitted = draw_layer->dirty_current;
	}
	dirtyRectClear(&draw_layer->dirty_current);
	draw_layer->graphic_submitted = submitted;
	draw_layer->graphic_to_present = NULL;
	__DMB();
	draw_layer->present_pending = true;
	GEN_PULSE(LCD_PULSE_PIN_1, 1);
	FSP_CRITICAL_SECTION_EXIT;

	return FSP_SUCCESS;
#else
	if (lcd->present == NULL) {
		return FSP_ERR_UNSUPPORTED;
	}
	err = lcd->present(lcd, layer);

	return err;
#endif
}

uint32_t LCD_SetBackLight(LCD_DeviceType *lcd, uint8_t percent)
{
	uint32_t err;

	if (lcd->setBackLight == NULL) {
		return FSP_ERR_UNSUPPORTED;
	}
	err = lcd->setBackLight(lcd, percent);

	return err;
}

uint32_t LCD_SetDefaultCLUT(LCD_DeviceType *lcd, uint8_t layer)
{
	uint32_t err = FSP_ERR_UNSUPPORTED;

	if (lcd->setDefaultCLUT != NULL) {
		return lcd->setDefaultCLUT(lcd, layer);
	}

#if LCD_EN_PERI_GLCDC
	const display_instance_t *p_inst = (const display_instance_t *)lcd->peri_1;
	err = p_inst->p_api->clut(p_inst->p_ctrl, &s_defalut_clut8_cfg, layer - 1);
#endif

	return err;
}

/**
 * @brief	更新 Layer1 的缓冲槽位，可选择立即提交或登记为后续 LCD_Present() 的显示目标
 * @note	连续登记且尚未提交时，最后一次非空登记覆盖前一次显示目标，不回滚此前的槽位更新。
 *			更新新画面会丢弃旧的未提交绘制及待同步脏状态；本函数不复制像素。
 *			立即模式直接提交新地址，并沿用旧实现对硬件更新时序冲突的重试；地址仍在后续 Vsync 生效。
 *			已有提交尚未完成时返回忙状态；旧显示缓冲需保留到后续提交的硬件地址切换完成。
 * @param	lcd            LCD 实例
 * @param	mem_num        要更新的下标，1 为 graphic_mem_1，2 为 graphic_mem_2
 * @param	new_buffer     新的显示缓冲，地址须按 64 字节对齐，大小必须与屏幕尺寸（包括 dummy）完全匹配；NULL 仅用于移除非前台缓冲
 * @param	read_only      给定的显示缓冲为只读
 * @param	immediate_show true 时非空缓冲立即提交，无需再调用 LCD_Present()；false 时仅登记；对 NULL 移除操作无影响
 * @return	FSP_SUCCESS 槽位更新成功，非空缓冲已按 immediate_show 选择登记或提交
 * 			FSP_ERR_IN_USE 有挂起提交或硬件正在更新
 * 			FSP_ERR_INVALID_CHANNEL 尝试移除前台
 * 			其它为错误
 */
uint32_t LCD_UpdateLayer1Mem(LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show)
{
	IS_INVALID_POINT(lcd);

	if (lcd->updateLayer1Mem != NULL) {
		return lcd->updateLayer1Mem(lcd, mem_num, new_buffer, read_only, immediate_show);
	}

#if LCD_EN_PERI_GLCDC
	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	IS_INVALID_POINT(lcd->peri_1);

	if ((mem_num != 1) && (mem_num != 2)) {
		return FSP_ERR_INVALID_ARGUMENT;
	}

	uint32_t err;
	uint8_t *mem_1;
	uint8_t *mem_2;
	uint8_t *old_mem;
	uint8_t *old_writeable;
	uint8_t *displayed;
	uint8_t *to_present;
	uint8_t *writeable;
	bool mem1_read_only;
	bool mem2_read_only;
	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	FSP_CRITICAL_SECTION_DEFINE;

	FSP_CRITICAL_SECTION_ENTER;

	/* 上一次软件提交尚未完成时，两种模式都保持忙状态。 */
	if (lcd->layer_1.present_pending) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_IN_USE;
	}
	if (((immediate_show == false) || (new_buffer == NULL)) && (R_GLCDC->GR[DISPLAY_FRAME_LAYER_1].VEN_b.PVEN != 0U)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_IN_USE;
	}

	/* 保留原 bufferChange() 对非空帧缓冲地址的 64 字节对齐要求。 */
	if ((new_buffer != NULL) && (((uintptr_t)new_buffer % 64U) != 0U)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_INVALID_ALIGNMENT;
	}

	mem_1 = lcd->layer_1.graphic_mem_1;
	mem_2 = lcd->layer_1.graphic_mem_2;
	old_mem = (mem_num == 1) ? mem_1 : mem_2;
	old_writeable = lcd->layer_1.graphic_writeable;
	/* 保存本次更新前的地址；立即提交成功时，此前的硬件地址更新已完成。 */
	displayed = (uint8_t *)(uintptr_t)R_GLCDC->GR[DISPLAY_FRAME_LAYER_1].FLM2;
	mem1_read_only = (mem_1 == NULL) || lcd->layer_1.mem1_read_only;
	mem2_read_only = (mem_2 == NULL) || lcd->layer_1.mem2_read_only;

	/* 先计算新状态，检查失败时不修改 LCD 实例。 */
	if (mem_num == 1) {
		mem_1 = new_buffer;
		mem1_read_only = (new_buffer == NULL) || read_only;
	}
	else {
		mem_2 = new_buffer;
		mem2_read_only = (new_buffer == NULL) || read_only;
	}
	if ((mem_1 != NULL) && (mem_1 == mem_2)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_INVALID_ARGUMENT;
	}

	if (new_buffer != NULL) {
		if (immediate_show) {
			/* 沿用旧实现的直接提交行为，提交失败时不修改 LCD 实例。 */
			__DSB();
			err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, new_buffer, DISPLAY_FRAME_LAYER_1);
			while (err == FSP_ERR_INVALID_UPDATE_TIMING) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, new_buffer, DISPLAY_FRAME_LAYER_1);
			}
			if (err != FSP_SUCCESS) {
				FSP_CRITICAL_SECTION_EXIT;
				LCD_LOGE("bufferChange failed: %" PRIu32, err);
				return err;
			}
			to_present = NULL;
		}
		else {
			/* 最后一次非空登记覆盖此前尚未提交的目标，包括只读缓冲。 */
			to_present = new_buffer;
		}
	}
	else {
		/* NULL 仅用于移除非前台槽位，不能停止当前前台的扫描。 */
		if ((old_mem != NULL) && (old_mem == displayed)) {
			FSP_CRITICAL_SECTION_EXIT;
			return FSP_ERR_INVALID_CHANNEL;
		}
		if ((mem_1 == NULL) && (mem_2 == NULL)) {
			FSP_CRITICAL_SECTION_EXIT;
			return FSP_ERR_INVALID_ARGUMENT;
		}
		to_present = lcd->layer_1.graphic_to_present;
		if (to_present == old_mem) {
			/* 移除候选所在的槽位时取消登记，不影响另一槽位的候选。 */
			to_present = NULL;
		}
	}

	if ((new_buffer != NULL) && (read_only == false) && (new_buffer != displayed)) {
		/* 新画面可写且不在扫描时，允许后续绘图直接修改该候选。 */
		writeable = new_buffer;
	}
	else if ((old_writeable != NULL) && (old_writeable != displayed) &&
			 (((old_writeable == mem_1) && (mem1_read_only == false)) ||
			  ((old_writeable == mem_2) && (mem2_read_only == false)))) {
		writeable = old_writeable;
	}
	else if ((mem_1 != NULL) && (mem1_read_only == false) && ((mem_1 != displayed) || (mem_2 == NULL))) {
		/* 单缓冲仍保留直接绘制行为；双缓冲不选择正在扫描的前台。 */
		writeable = mem_1;
	}
	else if ((mem_2 != NULL) && (mem2_read_only == false) && ((mem_2 != displayed) || (mem_1 == NULL))) {
		writeable = mem_2;
	}
	else {
		writeable = NULL;
	}

	lcd->layer_1.graphic_mem_1 = mem_1;
	lcd->layer_1.graphic_mem_2 = mem_2;
	lcd->layer_1.mem1_read_only = mem1_read_only;
	lcd->layer_1.mem2_read_only = mem2_read_only;
	lcd->layer_1.graphic_writeable = writeable;
	lcd->layer_1.graphic_displayed = displayed;
	lcd->layer_1.graphic_to_present = to_present;
	lcd->layer_1.graphic_submitted = ((new_buffer != NULL) && immediate_show) ? new_buffer : NULL;

	/* 槽位更新后，旧的延迟同步关系不再有效；新画面的同步由后续提交管理。 */
	dirtyRectClear(&lcd->layer_1.dirty_submitted);
	if ((new_buffer != NULL) || (old_writeable != writeable)) {
		dirtyRectClear(&lcd->layer_1.dirty_current);
	}
	if ((new_buffer != NULL) && immediate_show) {
		/* 与 Present 提交完整候选一致，交接后的后台需同步整帧差异。 */
		dirtyRectMerge(&lcd->layer_1.dirty_submitted, 0, 0, lcd->layer_1.x_length + lcd->layer_1.x_dummy, lcd->layer_1.y_length + lcd->layer_1.y_dummy);
	}
	else if ((to_present == NULL) && (lcd->layer_1.dirty_current.valid == false) && (displayed != NULL) && (writeable != NULL) && (writeable != displayed)) {
		/* 移除候选或更换写缓冲后，局部绘图需从仍在显示的前台恢复底图。 */
		dirtyRectMerge(&lcd->layer_1.dirty_submitted, 0, 0, lcd->layer_1.x_length + lcd->layer_1.x_dummy, lcd->layer_1.y_length + lcd->layer_1.y_dummy);
	}

	__DMB();
	if ((new_buffer != NULL) && immediate_show) {
		lcd->layer_1.present_pending = true;
	}
	FSP_CRITICAL_SECTION_EXIT;
	return FSP_SUCCESS;
#else
	return FSP_ERR_UNSUPPORTED;
#endif
}

uint32_t LCD_UpdateLayer2Mem(LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show)
{
	IS_INVALID_POINT(lcd);

	if (lcd->updateLayer2Mem != NULL) {
		return lcd->updateLayer2Mem(lcd, mem_num, new_buffer, read_only, immediate_show);
	}

#if LCD_EN_PERI_GLCDC
	IS_UNSUPPORT_PERI(lcd->peri_model_1);
	IS_INVALID_POINT(lcd->peri_1);

	if ((mem_num != 1) && (mem_num != 2)) {
		return FSP_ERR_INVALID_ARGUMENT;
	}

	uint32_t err;
	uint8_t *mem_1;
	uint8_t *mem_2;
	uint8_t *old_mem;
	uint8_t *old_writeable;
	uint8_t *displayed;
	uint8_t *to_present;
	uint8_t *writeable;
	bool mem1_read_only;
	bool mem2_read_only;
	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	FSP_CRITICAL_SECTION_DEFINE;

	FSP_CRITICAL_SECTION_ENTER;

	/* 上一次软件提交尚未完成时，两种模式都保持忙状态。 */
	if (lcd->layer_2.present_pending) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_IN_USE;
	}
	if (((immediate_show == false) || (new_buffer == NULL)) && (R_GLCDC->GR[DISPLAY_FRAME_LAYER_2].VEN_b.PVEN != 0U)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_IN_USE;
	}

	/* 保留原 bufferChange() 对非空帧缓冲地址的 64 字节对齐要求。 */
	if ((new_buffer != NULL) && (((uintptr_t)new_buffer % 64U) != 0U)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_INVALID_ALIGNMENT;
	}

	mem_1 = lcd->layer_2.graphic_mem_1;
	mem_2 = lcd->layer_2.graphic_mem_2;
	old_mem = (mem_num == 1) ? mem_1 : mem_2;
	old_writeable = lcd->layer_2.graphic_writeable;
	/* 保存本次更新前的地址；立即提交成功时，此前的硬件地址更新已完成。 */
	displayed = (uint8_t *)(uintptr_t)R_GLCDC->GR[DISPLAY_FRAME_LAYER_2].FLM2;
	mem1_read_only = (mem_1 == NULL) || lcd->layer_2.mem1_read_only;
	mem2_read_only = (mem_2 == NULL) || lcd->layer_2.mem2_read_only;

	/* 先计算新状态，检查失败时不修改 LCD 实例。 */
	if (mem_num == 1) {
		mem_1 = new_buffer;
		mem1_read_only = (new_buffer == NULL) || read_only;
	}
	else {
		mem_2 = new_buffer;
		mem2_read_only = (new_buffer == NULL) || read_only;
	}
	if ((mem_1 != NULL) && (mem_1 == mem_2)) {
		FSP_CRITICAL_SECTION_EXIT;
		return FSP_ERR_INVALID_ARGUMENT;
	}

	if (new_buffer != NULL) {
		if (immediate_show) {
			/* 沿用旧实现的直接提交行为，提交失败时不修改 LCD 实例。 */
			__DSB();
			err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, new_buffer, DISPLAY_FRAME_LAYER_2);
			while (err == FSP_ERR_INVALID_UPDATE_TIMING) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, new_buffer, DISPLAY_FRAME_LAYER_2);
			}
			if (err != FSP_SUCCESS) {
				FSP_CRITICAL_SECTION_EXIT;
				LCD_LOGE("bufferChange failed: %" PRIu32, err);
				return err;
			}
			to_present = NULL;
		}
		else {
			/* 最后一次非空登记覆盖此前尚未提交的目标，包括只读缓冲。 */
			to_present = new_buffer;
		}
	}
	else {
		/* NULL 仅用于移除非前台槽位，不能停止当前前台的扫描。 */
		if ((old_mem != NULL) && (old_mem == displayed)) {
			FSP_CRITICAL_SECTION_EXIT;
			return FSP_ERR_INVALID_CHANNEL;
		}
		if ((mem_1 == NULL) && (mem_2 == NULL)) {
			FSP_CRITICAL_SECTION_EXIT;
			return FSP_ERR_INVALID_ARGUMENT;
		}
		to_present = lcd->layer_2.graphic_to_present;
		if (to_present == old_mem) {
			/* 移除候选所在的槽位时取消登记，不影响另一槽位的候选。 */
			to_present = NULL;
		}
	}

	if ((new_buffer != NULL) && (read_only == false) && (new_buffer != displayed)) {
		/* 新画面可写且不在扫描时，允许后续绘图直接修改该候选。 */
		writeable = new_buffer;
	}
	else if ((old_writeable != NULL) && (old_writeable != displayed) &&
			 (((old_writeable == mem_1) && (mem1_read_only == false)) ||
			  ((old_writeable == mem_2) && (mem2_read_only == false)))) {
		writeable = old_writeable;
	}
	else if ((mem_1 != NULL) && (mem1_read_only == false) && ((mem_1 != displayed) || (mem_2 == NULL))) {
		/* 单缓冲仍保留直接绘制行为；双缓冲不选择正在扫描的前台。 */
		writeable = mem_1;
	}
	else if ((mem_2 != NULL) && (mem2_read_only == false) && ((mem_2 != displayed) || (mem_1 == NULL))) {
		writeable = mem_2;
	}
	else {
		writeable = NULL;
	}

	lcd->layer_2.graphic_mem_1 = mem_1;
	lcd->layer_2.graphic_mem_2 = mem_2;
	lcd->layer_2.mem1_read_only = mem1_read_only;
	lcd->layer_2.mem2_read_only = mem2_read_only;
	lcd->layer_2.graphic_writeable = writeable;
	lcd->layer_2.graphic_displayed = displayed;
	lcd->layer_2.graphic_to_present = to_present;
	lcd->layer_2.graphic_submitted = ((new_buffer != NULL) && immediate_show) ? new_buffer : NULL;

	/* 槽位更新后，旧的延迟同步关系不再有效；新画面的同步由后续提交管理。 */
	dirtyRectClear(&lcd->layer_2.dirty_submitted);
	if ((new_buffer != NULL) || (old_writeable != writeable)) {
		dirtyRectClear(&lcd->layer_2.dirty_current);
	}
	if ((new_buffer != NULL) && immediate_show) {
		/* 与 Present 提交完整候选一致，交接后的后台需同步整帧差异。 */
		dirtyRectMerge(&lcd->layer_2.dirty_submitted, 0, 0, lcd->layer_2.x_length + lcd->layer_2.x_dummy, lcd->layer_2.y_length + lcd->layer_2.y_dummy);
	}
	else if ((to_present == NULL) && (lcd->layer_2.dirty_current.valid == false) && (displayed != NULL) && (writeable != NULL) && (writeable != displayed)) {
		/* 移除候选或更换写缓冲后，局部绘图需从仍在显示的前台恢复底图。 */
		dirtyRectMerge(&lcd->layer_2.dirty_submitted, 0, 0, lcd->layer_2.x_length + lcd->layer_2.x_dummy, lcd->layer_2.y_length + lcd->layer_2.y_dummy);
	}

	__DMB();
	if ((new_buffer != NULL) && immediate_show) {
		lcd->layer_2.present_pending = true;
	}
	FSP_CRITICAL_SECTION_EXIT;
	return FSP_SUCCESS;
#else
	return FSP_ERR_UNSUPPORTED;
#endif
}

/*================================== Private Functions ============================================*/

static uint32_t convertToARGB1555(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		break;
	case LCD_COLOR_FORMAT_RGB565:
		break;
	case LCD_COLOR_FORMAT_RGB666:
		break;
	case LCD_COLOR_FORMAT_RGB888:
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

static uint32_t convertToARGB4444(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	const uint16_t *p16_src = (const uint16_t *)cct->src;
	uint16_t *p16_out = (uint16_t *)cct->out;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_ARGB8888:
		err = FSP_ERR_UNSUPPORTED;
		break;
	case LCD_COLOR_FORMAT_RGB565:
		if (cct->alpha < 0) {
			convertRGB565ToARGB4444(p16_src, cct->total_pixels, p16_out, 0xF000);
		}
		else {
			convertRGB565ToARGB4444(p16_src, cct->total_pixels, p16_out, (cct->alpha << 12) & 0xF000);
		}
		break;
	case LCD_COLOR_FORMAT_RGB666:
	case LCD_COLOR_FORMAT_RGB888:
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

static uint32_t convertToARGB8888(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		break;
	case LCD_COLOR_FORMAT_RGB565:
		break;
	case LCD_COLOR_FORMAT_RGB666:
		break;
	case LCD_COLOR_FORMAT_RGB888:
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

static uint32_t convertToRGB565(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		break;
	case LCD_COLOR_FORMAT_RGB565:
		break;
	case LCD_COLOR_FORMAT_RGB666:
		break;
	case LCD_COLOR_FORMAT_RGB888:
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

static uint32_t convertToRGB666(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		break;
	case LCD_COLOR_FORMAT_RGB565:
		break;
	case LCD_COLOR_FORMAT_RGB666:
		break;
	case LCD_COLOR_FORMAT_RGB888:
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

static uint32_t convertToRGB888(LCD_ColorConvertType *cct)
{
	uint32_t err = 0;

	switch (cct->src_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		break;
	case LCD_COLOR_FORMAT_RGB565:
		break;
	case LCD_COLOR_FORMAT_RGB666:
		break;
	case LCD_COLOR_FORMAT_RGB888:
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

/**
 * @brief	将连续 RGB565 像素转换为 ARGB4444，各颜色分量保留高 4 位
 * @details	源和目标须按 2 字节对齐，允许完全原地转换，不支持其它重叠方式。
 * 			两地址均按 4 字节对齐时，每个 32 位字同时转换两个像素；其它情况逐像素处理。
 * @param	src    RGB565 源像素，格式为 R[15:11]、G[10:5]、B[4:0]
 * @param	length 像素数，允许为 0 或奇数
 * @param	out    ARGB4444 目标像素，容量至少为 length 个 uint16_t
 * @param	alpha  已左移到 [15:12] 的统一 Alpha，0x0000 全透明，0xF000 不透明
 */
static void convertRGB565ToARGB4444(const uint16_t *src, uint32_t length, uint16_t *out, uint16_t alpha)
{
	uint32_t pixels;
	uint32_t alpha_pair;

	alpha &= 0xF000U;
	alpha_pair = (uint32_t)alpha | ((uint32_t)alpha << 16);

	/* 同时偏移 2 字节时，先转换一个像素，使后续两个地址均按 4 字节对齐。 */
	if ((length != 0U) && (((uintptr_t)src & 3U) == 2U) && (((uintptr_t)out & 3U) == 2U)) {
		pixels = *src++;
		*out++ = (uint16_t)(alpha | ((pixels >> 4) & 0x0F00U) | ((pixels >> 3) & 0x00F0U) | ((pixels >> 1) & 0x000FU));
		length--;
	}

	if ((((uintptr_t)src | (uintptr_t)out) & 3U) == 0U) {
		while (length >= 2U) {
			/* 定长 memcpy 避免 uint16_t 缓冲被 uint32_t 指针访问时的别名问题。 */
			memcpy(&pixels, src, sizeof(pixels));
			pixels = alpha_pair | ((pixels >> 4) & 0x0F000F00U) | ((pixels >> 3) & 0x00F000F0U) | ((pixels >> 1) & 0x000F000FU);
			memcpy(out, &pixels, sizeof(pixels));
			src += 2;
			out += 2;
			length -= 2U;
		}
	}

	/* 包括奇数尾像素，以及源、目标无法同时按 4 字节对齐的情况。 */
	while (length != 0U) {
		pixels = *src++;
		*out++ = (uint16_t)(alpha | ((pixels >> 4) & 0x0F00U) | ((pixels >> 3) & 0x00F0U) | ((pixels >> 1) & 0x000FU));
		length--;
	}
}

/**
 * @brief 清除脏矩形
 * @param[in,out] rect 脏矩形
 */
static void dirtyRectClear(LCD_DirtyRectType *rect)
{
	rect->valid = false;
}

/**
 * @brief 将给定区域合并到单个包围脏矩形中
 * @param[in,out] rect   脏矩形
 * @param[in]     x      区域左上角 X 坐标
 * @param[in]     y      区域左上角 Y 坐标
 * @param[in]     length 区域水平像素数
 * @param[in]     width  区域垂直像素数
 */
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width)
{
	uint32_t bottom;
	uint32_t right;
	uint16_t left;
	uint16_t top;

	if ((length == 0) || (width == 0)) {
		return;
	}
	if (rect->valid == false) {
		rect->x = x;
		rect->y = y;
		rect->length = length;
		rect->width = width;
		rect->valid = true;

		return;
	}

	right = ((uint32_t)rect->x + rect->length) > ((uint32_t)x + length) ? (uint32_t)rect->x + rect->length : (uint32_t)x + length;
	bottom = ((uint32_t)rect->y + rect->width) > ((uint32_t)y + width) ? (uint32_t)rect->y + rect->width : (uint32_t)y + width;
	left = rect->x < x ? rect->x : x;
	top = rect->y < y ? rect->y : y;
	rect->x = left;
	rect->y = top;
	rect->length = (uint16_t)(right - rect->x);
	rect->width = (uint16_t)(bottom - rect->y);
}

/**
 * @brief	在绘制前准备可写帧缓冲
 * @details	局部绘制优先以待显示目标为底图，否则同步当前前台的脏区域；整帧覆盖跳过复制。
 * @param	lcd           LCD 设备实例
 * @param	overwrite_all 本次绘制将覆盖整个可写帧缓冲时设为 true
 * @retval	FSP_SUCCESS       可写帧缓冲已准备完成
 * @retval	FSP_ERR_ASSERTION 可写帧缓冲或同步源无效
 * @retval	FSP_ERR_IN_USE    上一次提交尚未完成，或双缓冲写目标仍在扫描
 */
static uint32_t prepareWriteBuffer(LCD_LayerType *draw_layer, bool overwrite_all)
{
	uint16_t row;
	uint16_t stride;

	uint8_t *source;
	uint8_t *to_present = draw_layer->graphic_to_present;
	uint8_t *writeable = draw_layer->graphic_writeable;
	uint16_t *p16_dst = NULL;
	uint16_t *p16_src = NULL;
	uint32_t *p32_dst = NULL;
	uint32_t *p32_src = NULL;

	LCD_DirtyRectType rect = draw_layer->dirty_submitted;

	/* 忙状态检查必须先于整帧覆盖及无脏区域的快速返回。 */
	if (draw_layer->present_pending || (R_GLCDC->GR[draw_layer->index - 1].VEN_b.PVEN != 0U)) {
		return FSP_ERR_IN_USE;
	}
	if (writeable == NULL) {
		LCD_LOGE("writeable is NULL");
		return FSP_ERR_ASSERTION;
	}
	if (!(((writeable == draw_layer->graphic_mem_1) && (draw_layer->mem1_read_only == false)) ||
			((writeable == draw_layer->graphic_mem_2) && (draw_layer->mem2_read_only == false)))) {
		LCD_LOGE("graphic_writeable is not a writable buffer !");
		return FSP_ERR_ASSERTION;
	}
	if ((writeable == draw_layer->graphic_displayed) && (draw_layer->graphic_mem_1 != NULL) && (draw_layer->graphic_mem_2 != NULL)) {
		return FSP_ERR_IN_USE;
	}

	if (overwrite_all || (to_present == writeable)) {
		/* 可写候选已有完整底图，或本次将覆盖整帧；不复制旧前台。 */
		dirtyRectClear(&draw_layer->dirty_submitted);
		if (to_present != NULL) {
			draw_layer->graphic_to_present = writeable;
		}
		return FSP_SUCCESS;
	}
	if (to_present != NULL) {
		source = to_present;
		dirtyRectClear(&rect);
		dirtyRectMerge(&rect, 0, 0, draw_layer->x_length + draw_layer->x_dummy, draw_layer->y_length + draw_layer->y_dummy);
	}
	else {
		/* 前台可能已不在两个槽位中，不能按另一槽位推断同步源。 */
		source = draw_layer->graphic_displayed;
	}
	if (rect.valid == false) {
		return FSP_SUCCESS;
	}
	if (source == NULL) {
		return FSP_ERR_ASSERTION;
	}
	if (source == writeable) {
		dirtyRectClear(&draw_layer->dirty_submitted);
		return FSP_SUCCESS;
	}

	stride = draw_layer->x_length + draw_layer->x_dummy;
	switch (draw_layer->color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
	case LCD_COLOR_FORMAT_ARGB4444:
	case LCD_COLOR_FORMAT_RGB565:
		p16_src = (uint16_t *)source + rect.y * stride + rect.x;
		p16_dst = (uint16_t *)writeable + rect.y * stride + rect.x;
		for (row = 0; row < rect.width; row++) {
			memcpy(p16_dst, p16_src, rect.length * 2);
			p16_src = &p16_src[stride];
			p16_dst = &p16_dst[stride];
		}
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		p32_src = (uint32_t *)source + rect.y * stride + rect.x;
		p32_dst = (uint32_t *)writeable + rect.y * stride + rect.x;
		for (row = 0; row < rect.width; row++) {
			memcpy(p32_dst, p32_src, rect.length * 4);
			p32_src = &p32_src[stride];
			p32_dst = &p32_dst[stride];
		}
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	dirtyRectClear(&draw_layer->dirty_submitted);
	if (to_present != NULL) {
		/* 叠加绘图后提交可写副本，不修改只读原图，也不再提交原候选。 */
		draw_layer->graphic_to_present = writeable;
	}

	return FSP_SUCCESS;
}
