#include "ov5640.h"

#if CAMERA_EN_DVP || CAMERA_EN_MIPI

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <math.h>

#include "hal_data.h"
#include "iic.h"

/*================================== MACROS =======================================================*/

#ifndef __OV5640_DEBUG
#define __OV5640_DEBUG	1
#endif

#define MIPI_CSI_CALLBACK		MIPI_CSI_Callback
/* 新板（2026-06-15） 修改为 BSP_IO_PORT_00_PIN_15
 * 旧板要使用需改为 BSP_IO_PORT_04_PIN_11 */
#define OV5640_PIN_VIN_PWR_ON	BSP_IO_PORT_00_PIN_15
#define OV5640_PIN_VIN_RESET	BSP_IO_PORT_07_PIN_04

#define OV5640_PIN_CEU_PWR_ON	BSP_IO_PORT_07_PIN_10
#define OV5640_PIN_CEU_RESET	BSP_IO_PORT_07_PIN_09

/* 如果 OV5640_CLOCK_FIXED_DIV 为 1，那么会直接写入这些值 */
#define OV5640_DVP_PLL_PRE_DIV		3U	/* REG 0x3037[3:0] */
#define OV5640_DVP_PLL_ROOT_DIV		1U	/* REG 0x3037[4]，1 表示 /2 */
#define OV5640_DVP_SCLK_ROOT_DIV	1U	/* REG 0x3108[1:0]，1 表示 /2 */

#define OV5640_MIPI_PLL_PRE_DIV		8   /* REG 0x3037[3:0] */
#define OV5640_MIPI_PLL_ROOT_DIV	0   /* REG 0x3037[4] */
#define OV5640_MIPI_SCLK_ROOT_DIV	2   /* REG 0x3108[1:0] */

/* MIPI lane 数须与传感器及 CSI 接收端一致；RA8P1 数据手册表 2.96 的每 lane 位速率范围。 */
#define OV5640_MIPI_LANE_COUNT		2U
#define OV5640_MIPI_LANE_RATE_MAX_BPS	720000000U
#define OV5640_MIPI_LANE_RATE_MIN_BPS	80000000U

/* OV5640 时钟约束，依据数据手册 2.5、表 7-1 */
#define OV5640_PLL_MULT_EVEN_FROM	128U		/* REG 0x3036 不小于该值时只能取偶数 */
#define OV5640_PLL_MULT_MAX			252U		/* REG 0x3036 上限 */
#define OV5640_PLL_MULT_MIN			4U			/* REG 0x3036 下限 */
#define OV5640_VCO_MAX_HZ			800000000U	/* 手册写 800MHz，Linux 驱动用 1GHz，视实际修改 */

#define iicWrite(reg, val)		IIC_SCCB_WriteReg(OV5640_IIC_ADDR, reg, 16, val)
#define iicRead(reg, val)		IIC_SCCB_ReadReg(OV5640_IIC_ADDR, reg, 16, val)
#define IS_INVALID_POINT(p)		if (p == NULL) { return FSP_ERR_INVALID_POINTER; }

#if __OV5640_DEBUG
#include "utils/log.h"
#define CHECK_IIC_READ(err, reg)		{ if (err) { LOG_E(__FUNCTION__, "Read reg 0x%04X failed: %u", reg, err); return err; } }
#define CHECK_IIC_WRITE(err, reg)		{ if (err) { LOG_E(__FUNCTION__, "Write reg 0x%04X failed: %u", reg, err); return err; } }
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define OV5640_LOGD(msg, ...)			LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV5640_LOGI(msg, ...)			LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV5640_LOGW(msg, ...)			LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV5640_LOGE(msg, ...)			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define CHECK_IIC_READ(err, reg)		{ if (err) { return err; } }
#define CHECK_IIC_WRITE(err, reg)		{ if (err) { return err; } }
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { return v; }
#define OV5640_LOGD(msg, ...)
#define OV5640_LOGI(msg, ...)
#define OV5640_LOGW(msg, ...)
#define OV5640_LOGE(msg, ...)
#endif

#define IIC_READ_CHECK(reg, val)	{ err = iicRead(reg, val); CHECK_IIC_READ(err, reg); }
#define IIC_WRITE_CHECK(reg, val)	{ err = iicWrite(reg, val); CHECK_IIC_WRITE(err, reg); }

/*================================== TYPES ========================================================*/

/* 位域成员必须按数据手册的位序从低位到高位声明：本工程所用 LLVM/Clang（小端 ARM）
 * 自 LSB 起分配位域，若按高位在前的顺序声明，读写的将是错误的寄存器位。
 * 位序依据数据手册表 7-1（0x3034~0x3037）与表 7-2（0x3108）。 */
struct OV5640_ClockConfig {
	union {
		uint8_t reg3034;	/* SC PLL CONTRL0 */
		struct {
			uint8_t mipi_bit_mode : 4;				/* [3:0] 0x8: 8-bit, 0xA: 10-bit */
			uint8_t pll_charge_pump_control : 3;	/* [6:4] */
			uint8_t debug_mode : 1;					/* [7] */
		} reg3034_b;
	};
	union {
		uint8_t reg3035;	/* SC PLL CONTRL1 */
		struct {
			uint8_t scale_divider_for_mipi : 4;		/* [3:0] */
			uint8_t system_clock_divider : 4;		/* [7:4] 0: /16, 1~15: /1~/15 */
		} reg3035_b;
	};
	union {
		uint8_t reg3036;	/* SC PLL CONTRL2 */
		struct {
			uint8_t pll_multiplier;					/* [7:0] 4~252, >127 只能偶数 */
		} reg3036_b;
	};
	union {
		uint8_t reg3037;	/* SC PLL CONTRL3 */
		struct {
			uint8_t pll_pre_divider : 4;			/* [3:0] 1, 2, 3, 4, 6, 8 */
			uint8_t pll_root_divider : 1;			/* [4] 0: bypass, 1: /2 */
			uint8_t debug_mode : 3;					/* [7:5] */
		} reg3037_b;
	};
	union {
		uint8_t reg3108;	/* SYSTEM ROOT DIVIDER */
		struct {
			uint8_t sclk_root_divider : 2;			/* [1:0] /1, /2, /4, /8 */
			uint8_t sclk2x_root_divider : 2;		/* [3:2] /1, /2, /4, /8 */
			uint8_t pclk_root_divider : 2;			/* [5:4] /1, /2, /4, /8 */
			uint8_t debug_mode : 2;					/* [7:6] */
		} reg3108_b;
	};
	uint8_t reg4837;	/* MIPI PCLK PERIOD；由 selectMIPIClock() 计算，单位 0.5 ns。 */
};

struct OV5640_Reg {
	uint16_t reg;
	uint8_t val;
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static const uint8_t sc_auto_focus_firmware[] = {
	0x02, 0x0F, 0xD6, 0x02, 0x0A, 0x39, 0xC2, 0x01, 0x22, 0x22, 0x00, 0x02, 0x0F, 0xB2, 0xE5, 0x1F,
    0x70, 0x72, 0xF5, 0x1E, 0xD2, 0x35, 0xFF, 0xEF, 0x25, 0xE0, 0x24, 0x4E, 0xF8, 0xE4, 0xF6, 0x08,
    0xF6, 0x0F, 0xBF, 0x34, 0xF2, 0x90, 0x0E, 0x93, 0xE4, 0x93, 0xFF, 0xE5, 0x4B, 0xC3, 0x9F, 0x50,
    0x04, 0x7F, 0x05, 0x80, 0x02, 0x7F, 0xFB, 0x78, 0xBD, 0xA6, 0x07, 0x12, 0x0F, 0x04, 0x40, 0x04,
    0x7F, 0x03, 0x80, 0x02, 0x7F, 0x30, 0x78, 0xBC, 0xA6, 0x07, 0xE6, 0x18, 0xF6, 0x08, 0xE6, 0x78,
    0xB9, 0xF6, 0x78, 0xBC, 0xE6, 0x78, 0xBA, 0xF6, 0x78, 0xBF, 0x76, 0x33, 0xE4, 0x08, 0xF6, 0x78,
    0xB8, 0x76, 0x01, 0x75, 0x4A, 0x02, 0x78, 0xB6, 0xF6, 0x08, 0xF6, 0x74, 0xFF, 0x78, 0xC1, 0xF6,
    0x08, 0xF6, 0x75, 0x1F, 0x01, 0x78, 0xBC, 0xE6, 0x75, 0xF0, 0x05, 0xA4, 0xF5, 0x4B, 0x12, 0x0A,
    0xFF, 0xC2, 0x37, 0x22, 0x78, 0xB8, 0xE6, 0xD3, 0x94, 0x00, 0x40, 0x02, 0x16, 0x22, 0xE5, 0x1F,
    0xB4, 0x05, 0x23, 0xE4, 0xF5, 0x1F, 0xC2, 0x01, 0x78, 0xB6, 0xE6, 0xFE, 0x08, 0xE6, 0xFF, 0x78,
    0x4E, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0xA2, 0x37, 0xE4, 0x33, 0xF5, 0x3C, 0x90, 0x30, 0x28, 0xF0,
    0x75, 0x1E, 0x10, 0xD2, 0x35, 0x22, 0xE5, 0x4B, 0x75, 0xF0, 0x05, 0x84, 0x78, 0xBC, 0xF6, 0x90,
    0x0E, 0x8C, 0xE4, 0x93, 0xFF, 0x25, 0xE0, 0x24, 0x0A, 0xF8, 0xE6, 0xFC, 0x08, 0xE6, 0xFD, 0x78,
    0xBC, 0xE6, 0x25, 0xE0, 0x24, 0x4E, 0xF8, 0xA6, 0x04, 0x08, 0xA6, 0x05, 0xEF, 0x12, 0x0F, 0x0B,
    0xD3, 0x78, 0xB7, 0x96, 0xEE, 0x18, 0x96, 0x40, 0x0D, 0x78, 0xBC, 0xE6, 0x78, 0xB9, 0xF6, 0x78,
    0xB6, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0x90, 0x0E, 0x8C, 0xE4, 0x93, 0x12, 0x0F, 0x0B, 0xC3, 0x78,
    0xC2, 0x96, 0xEE, 0x18, 0x96, 0x50, 0x0D, 0x78, 0xBC, 0xE6, 0x78, 0xBA, 0xF6, 0x78, 0xC1, 0xA6,
    0x06, 0x08, 0xA6, 0x07, 0x78, 0xB6, 0xE6, 0xFE, 0x08, 0xE6, 0xC3, 0x78, 0xC2, 0x96, 0xFF, 0xEE,
    0x18, 0x96, 0x78, 0xC3, 0xF6, 0x08, 0xA6, 0x07, 0x90, 0x0E, 0x95, 0xE4, 0x18, 0x12, 0x0E, 0xE9,
    0x40, 0x02, 0xD2, 0x37, 0x78, 0xBC, 0xE6, 0x08, 0x26, 0x08, 0xF6, 0xE5, 0x1F, 0x64, 0x01, 0x70,
    0x4A, 0xE6, 0xC3, 0x78, 0xC0, 0x12, 0x0E, 0xDF, 0x40, 0x05, 0x12, 0x0E, 0xDA, 0x40, 0x39, 0x12,
    0x0F, 0x02, 0x40, 0x04, 0x7F, 0xFE, 0x80, 0x02, 0x7F, 0x02, 0x78, 0xBD, 0xA6, 0x07, 0x78, 0xB9,
    0xE6, 0x24, 0x03, 0x78, 0xBF, 0xF6, 0x78, 0xB9, 0xE6, 0x24, 0xFD, 0x78, 0xC0, 0xF6, 0x12, 0x0F,
    0x02, 0x40, 0x06, 0x78, 0xC0, 0xE6, 0xFF, 0x80, 0x04, 0x78, 0xBF, 0xE6, 0xFF, 0x78, 0xBE, 0xA6,
    0x07, 0x75, 0x1F, 0x02, 0x78, 0xB8, 0x76, 0x01, 0x02, 0x02, 0x4A, 0xE5, 0x1F, 0x64, 0x02, 0x60,
    0x03, 0x02, 0x02, 0x2A, 0x78, 0xBE, 0xE6, 0xFF, 0xC3, 0x78, 0xC0, 0x12, 0x0E, 0xE0, 0x40, 0x08,
    0x12, 0x0E, 0xDA, 0x50, 0x03, 0x02, 0x02, 0x28, 0x12, 0x0F, 0x02, 0x40, 0x04, 0x7F, 0xFF, 0x80,
    0x02, 0x7F, 0x01, 0x78, 0xBD, 0xA6, 0x07, 0x78, 0xB9, 0xE6, 0x04, 0x78, 0xBF, 0xF6, 0x78, 0xB9,
    0xE6, 0x14, 0x78, 0xC0, 0xF6, 0x18, 0x12, 0x0F, 0x04, 0x40, 0x04, 0xE6, 0xFF, 0x80, 0x02, 0x7F,
    0x00, 0x78, 0xBF, 0xA6, 0x07, 0xD3, 0x08, 0xE6, 0x64, 0x80, 0x94, 0x80, 0x40, 0x04, 0xE6, 0xFF,
    0x80, 0x02, 0x7F, 0x00, 0x78, 0xC0, 0xA6, 0x07, 0xC3, 0x18, 0xE6, 0x64, 0x80, 0x94, 0xB3, 0x50,
    0x04, 0xE6, 0xFF, 0x80, 0x02, 0x7F, 0x33, 0x78, 0xBF, 0xA6, 0x07, 0xC3, 0x08, 0xE6, 0x64, 0x80,
    0x94, 0xB3, 0x50, 0x04, 0xE6, 0xFF, 0x80, 0x02, 0x7F, 0x33, 0x78, 0xC0, 0xA6, 0x07, 0x12, 0x0F,
    0x02, 0x40, 0x06, 0x78, 0xC0, 0xE6, 0xFF, 0x80, 0x04, 0x78, 0xBF, 0xE6, 0xFF, 0x78, 0xBE, 0xA6,
    0x07, 0x75, 0x1F, 0x03, 0x78, 0xB8, 0x76, 0x01, 0x80, 0x20, 0xE5, 0x1F, 0x64, 0x03, 0x70, 0x26,
    0x78, 0xBE, 0xE6, 0xFF, 0xC3, 0x78, 0xC0, 0x12, 0x0E, 0xE0, 0x40, 0x05, 0x12, 0x0E, 0xDA, 0x40,
    0x09, 0x78, 0xB9, 0xE6, 0x78, 0xBE, 0xF6, 0x75, 0x1F, 0x04, 0x78, 0xBE, 0xE6, 0x75, 0xF0, 0x05,
    0xA4, 0xF5, 0x4B, 0x02, 0x0A, 0xFF, 0xE5, 0x1F, 0xB4, 0x04, 0x10, 0x90, 0x0E, 0x94, 0xE4, 0x78,
    0xC3, 0x12, 0x0E, 0xE9, 0x40, 0x02, 0xD2, 0x37, 0x75, 0x1F, 0x05, 0x22, 0x30, 0x01, 0x03, 0x02,
    0x04, 0xC0, 0x30, 0x02, 0x03, 0x02, 0x04, 0xC0, 0x90, 0x51, 0xA5, 0xE0, 0x78, 0x93, 0xF6, 0xA3,
    0xE0, 0x08, 0xF6, 0xA3, 0xE0, 0x08, 0xF6, 0xE5, 0x1F, 0x70, 0x3C, 0x75, 0x1E, 0x20, 0xD2, 0x35,
    0x12, 0x0C, 0x7A, 0x78, 0x7E, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0x78, 0x8B, 0xA6, 0x09, 0x18, 0x76,
    0x01, 0x12, 0x0C, 0x5B, 0x78, 0x4E, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0x78, 0x8B, 0xE6, 0x78, 0x6E,
    0xF6, 0x75, 0x1F, 0x01, 0x78, 0x93, 0xE6, 0x78, 0x90, 0xF6, 0x78, 0x94, 0xE6, 0x78, 0x91, 0xF6,
    0x78, 0x95, 0xE6, 0x78, 0x92, 0xF6, 0x22, 0x79, 0x90, 0xE7, 0xD3, 0x78, 0x93, 0x96, 0x40, 0x05,
    0xE7, 0x96, 0xFF, 0x80, 0x08, 0xC3, 0x79, 0x93, 0xE7, 0x78, 0x90, 0x96, 0xFF, 0x78, 0x88, 0x76,
    0x00, 0x08, 0xA6, 0x07, 0x79, 0x91, 0xE7, 0xD3, 0x78, 0x94, 0x96, 0x40, 0x05, 0xE7, 0x96, 0xFF,
    0x80, 0x08, 0xC3, 0x79, 0x94, 0xE7, 0x78, 0x91, 0x96, 0xFF, 0x12, 0x0C, 0x8E, 0x79, 0x92, 0xE7,
    0xD3, 0x78, 0x95, 0x96, 0x40, 0x05, 0xE7, 0x96, 0xFF, 0x80, 0x08, 0xC3, 0x79, 0x95, 0xE7, 0x78,
    0x92, 0x96, 0xFF, 0x12, 0x0C, 0x8E, 0x12, 0x0C, 0x5B, 0x78, 0x8A, 0xE6, 0x25, 0xE0, 0x24, 0x4E,
    0xF8, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0x78, 0x8A, 0xE6, 0x24, 0x6E, 0xF8, 0xA6, 0x09, 0x78, 0x8A,
    0xE6, 0x24, 0x01, 0xFF, 0xE4, 0x33, 0xFE, 0xD3, 0xEF, 0x94, 0x0F, 0xEE, 0x64, 0x80, 0x94, 0x80,
    0x40, 0x04, 0x7F, 0x00, 0x80, 0x05, 0x78, 0x8A, 0xE6, 0x04, 0xFF, 0x78, 0x8A, 0xA6, 0x07, 0xE5,
    0x1F, 0xB4, 0x01, 0x0A, 0xE6, 0x60, 0x03, 0x02, 0x04, 0xC0, 0x75, 0x1F, 0x02, 0x22, 0x12, 0x0C,
    0x7A, 0x78, 0x80, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0x12, 0x0C, 0x7A, 0x78, 0x82, 0xA6, 0x06, 0x08,
    0xA6, 0x07, 0x78, 0x6E, 0xE6, 0x78, 0x8C, 0xF6, 0x78, 0x6E, 0xE6, 0x78, 0x8D, 0xF6, 0x7F, 0x01,
    0xEF, 0x25, 0xE0, 0x24, 0x4F, 0xF9, 0xC3, 0x78, 0x81, 0xE6, 0x97, 0x18, 0xE6, 0x19, 0x97, 0x50,
    0x0A, 0x12, 0x0C, 0x82, 0x78, 0x80, 0xA6, 0x04, 0x08, 0xA6, 0x05, 0x74, 0x6E, 0x2F, 0xF9, 0x78,
    0x8C, 0xE6, 0xC3, 0x97, 0x50, 0x08, 0x74, 0x6E, 0x2F, 0xF8, 0xE6, 0x78, 0x8C, 0xF6, 0xEF, 0x25,
    0xE0, 0x24, 0x4F, 0xF9, 0xD3, 0x78, 0x83, 0xE6, 0x97, 0x18, 0xE6, 0x19, 0x97, 0x40, 0x0A, 0x12,
    0x0C, 0x82, 0x78, 0x82, 0xA6, 0x04, 0x08, 0xA6, 0x05, 0x74, 0x6E, 0x2F, 0xF9, 0x78, 0x8D, 0xE6,
    0xD3, 0x97, 0x40, 0x08, 0x74, 0x6E, 0x2F, 0xF8, 0xE6, 0x78, 0x8D, 0xF6, 0x0F, 0xEF, 0x64, 0x10,
    0x70, 0x9E, 0xC3, 0x79, 0x81, 0xE7, 0x78, 0x83, 0x96, 0xFF, 0x19, 0xE7, 0x18, 0x96, 0x78, 0x84,
    0xF6, 0x08, 0xA6, 0x07, 0xC3, 0x79, 0x8C, 0xE7, 0x78, 0x8D, 0x96, 0x08, 0xF6, 0xD3, 0x79, 0x81,
    0xE7, 0x78, 0x7F, 0x96, 0x19, 0xE7, 0x18, 0x96, 0x40, 0x05, 0x09, 0xE7, 0x08, 0x80, 0x06, 0xC3,
    0x79, 0x7F, 0xE7, 0x78, 0x81, 0x96, 0xFF, 0x19, 0xE7, 0x18, 0x96, 0xFE, 0x78, 0x86, 0xA6, 0x06,
    0x08, 0xA6, 0x07, 0x79, 0x8C, 0xE7, 0xD3, 0x78, 0x8B, 0x96, 0x40, 0x05, 0xE7, 0x96, 0xFF, 0x80,
    0x08, 0xC3, 0x79, 0x8B, 0xE7, 0x78, 0x8C, 0x96, 0xFF, 0x78, 0x8F, 0xA6, 0x07, 0xE5, 0x1F, 0x64,
    0x02, 0x70, 0x69, 0x90, 0x0E, 0x91, 0x93, 0xFF, 0x18, 0xE6, 0xC3, 0x9F, 0x50, 0x72, 0x12, 0x0C,
    0x4A, 0x12, 0x0C, 0x2F, 0x90, 0x0E, 0x8E, 0x12, 0x0C, 0x38, 0x78, 0x80, 0x12, 0x0C, 0x6B, 0x7B,
    0x04, 0x12, 0x0C, 0x1D, 0xC3, 0x12, 0x06, 0x45, 0x50, 0x56, 0x90, 0x0E, 0x92, 0xE4, 0x93, 0xFF,
    0x78, 0x8F, 0xE6, 0x9F, 0x40, 0x02, 0x80, 0x11, 0x90, 0x0E, 0x90, 0xE4, 0x93, 0xFF, 0xD3, 0x78,
    0x89, 0xE6, 0x9F, 0x18, 0xE6, 0x94, 0x00, 0x40, 0x03, 0x75, 0x1F, 0x05, 0x12, 0x0C, 0x4A, 0x12,
    0x0C, 0x2F, 0x90, 0x0E, 0x8F, 0x12, 0x0C, 0x38, 0x78, 0x7E, 0x12, 0x0C, 0x6B, 0x7B, 0x40, 0x12,
    0x0C, 0x1D, 0xD3, 0x12, 0x06, 0x45, 0x40, 0x18, 0x75, 0x1F, 0x05, 0x22, 0xE5, 0x1F, 0xB4, 0x05,
    0x0F, 0xD2, 0x01, 0xC2, 0x02, 0xE4, 0xF5, 0x1F, 0xF5, 0x1E, 0xD2, 0x35, 0xD2, 0x33, 0xD2, 0x36,
    0x22, 0xEF, 0x8D, 0xF0, 0xA4, 0xA8, 0xF0, 0xCF, 0x8C, 0xF0, 0xA4, 0x28, 0xCE, 0x8D, 0xF0, 0xA4,
    0x2E, 0xFE, 0x22, 0xBC, 0x00, 0x0B, 0xBE, 0x00, 0x29, 0xEF, 0x8D, 0xF0, 0x84, 0xFF, 0xAD, 0xF0,
    0x22, 0xE4, 0xCC, 0xF8, 0x75, 0xF0, 0x08, 0xEF, 0x2F, 0xFF, 0xEE, 0x33, 0xFE, 0xEC, 0x33, 0xFC,
    0xEE, 0x9D, 0xEC, 0x98, 0x40, 0x05, 0xFC, 0xEE, 0x9D, 0xFE, 0x0F, 0xD5, 0xF0, 0xE9, 0xE4, 0xCE,
    0xFD, 0x22, 0xED, 0xF8, 0xF5, 0xF0, 0xEE, 0x84, 0x20, 0xD2, 0x1C, 0xFE, 0xAD, 0xF0, 0x75, 0xF0,
    0x08, 0xEF, 0x2F, 0xFF, 0xED, 0x33, 0xFD, 0x40, 0x07, 0x98, 0x50, 0x06, 0xD5, 0xF0, 0xF2, 0x22,
    0xC3, 0x98, 0xFD, 0x0F, 0xD5, 0xF0, 0xEA, 0x22, 0xE8, 0x8F, 0xF0, 0xA4, 0xCC, 0x8B, 0xF0, 0xA4,
    0x2C, 0xFC, 0xE9, 0x8E, 0xF0, 0xA4, 0x2C, 0xFC, 0x8A, 0xF0, 0xED, 0xA4, 0x2C, 0xFC, 0xEA, 0x8E,
    0xF0, 0xA4, 0xCD, 0xA8, 0xF0, 0x8B, 0xF0, 0xA4, 0x2D, 0xCC, 0x38, 0x25, 0xF0, 0xFD, 0xE9, 0x8F,
    0xF0, 0xA4, 0x2C, 0xCD, 0x35, 0xF0, 0xFC, 0xEB, 0x8E, 0xF0, 0xA4, 0xFE, 0xA9, 0xF0, 0xEB, 0x8F,
    0xF0, 0xA4, 0xCF, 0xC5, 0xF0, 0x2E, 0xCD, 0x39, 0xFE, 0xE4, 0x3C, 0xFC, 0xEA, 0xA4, 0x2D, 0xCE,
    0x35, 0xF0, 0xFD, 0xE4, 0x3C, 0xFC, 0x22, 0x75, 0xF0, 0x08, 0x75, 0x82, 0x00, 0xEF, 0x2F, 0xFF,
    0xEE, 0x33, 0xFE, 0xCD, 0x33, 0xCD, 0xCC, 0x33, 0xCC, 0xC5, 0x82, 0x33, 0xC5, 0x82, 0x9B, 0xED,
    0x9A, 0xEC, 0x99, 0xE5, 0x82, 0x98, 0x40, 0x0C, 0xF5, 0x82, 0xEE, 0x9B, 0xFE, 0xED, 0x9A, 0xFD,
    0xEC, 0x99, 0xFC, 0x0F, 0xD5, 0xF0, 0xD6, 0xE4, 0xCE, 0xFB, 0xE4, 0xCD, 0xFA, 0xE4, 0xCC, 0xF9,
    0xA8, 0x82, 0x22, 0xB8, 0x00, 0xC1, 0xB9, 0x00, 0x59, 0xBA, 0x00, 0x2D, 0xEC, 0x8B, 0xF0, 0x84,
    0xCF, 0xCE, 0xCD, 0xFC, 0xE5, 0xF0, 0xCB, 0xF9, 0x78, 0x18, 0xEF, 0x2F, 0xFF, 0xEE, 0x33, 0xFE,
    0xED, 0x33, 0xFD, 0xEC, 0x33, 0xFC, 0xEB, 0x33, 0xFB, 0x10, 0xD7, 0x03, 0x99, 0x40, 0x04, 0xEB,
    0x99, 0xFB, 0x0F, 0xD8, 0xE5, 0xE4, 0xF9, 0xFA, 0x22, 0x78, 0x18, 0xEF, 0x2F, 0xFF, 0xEE, 0x33,
    0xFE, 0xED, 0x33, 0xFD, 0xEC, 0x33, 0xFC, 0xC9, 0x33, 0xC9, 0x10, 0xD7, 0x05, 0x9B, 0xE9, 0x9A,
    0x40, 0x07, 0xEC, 0x9B, 0xFC, 0xE9, 0x9A, 0xF9, 0x0F, 0xD8, 0xE0, 0xE4, 0xC9, 0xFA, 0xE4, 0xCC,
    0xFB, 0x22, 0x75, 0xF0, 0x10, 0xEF, 0x2F, 0xFF, 0xEE, 0x33, 0xFE, 0xED, 0x33, 0xFD, 0xCC, 0x33,
    0xCC, 0xC8, 0x33, 0xC8, 0x10, 0xD7, 0x07, 0x9B, 0xEC, 0x9A, 0xE8, 0x99, 0x40, 0x0A, 0xED, 0x9B,
    0xFD, 0xEC, 0x9A, 0xFC, 0xE8, 0x99, 0xF8, 0x0F, 0xD5, 0xF0, 0xDA, 0xE4, 0xCD, 0xFB, 0xE4, 0xCC,
    0xFA, 0xE4, 0xC8, 0xF9, 0x22, 0xEB, 0x9F, 0xF5, 0xF0, 0xEA, 0x9E, 0x42, 0xF0, 0xE9, 0x9D, 0x42,
    0xF0, 0xE8, 0x9C, 0x45, 0xF0, 0x22, 0xE8, 0x60, 0x0F, 0xEC, 0xC3, 0x13, 0xFC, 0xED, 0x13, 0xFD,
    0xEE, 0x13, 0xFE, 0xEF, 0x13, 0xFF, 0xD8, 0xF1, 0x22, 0xE8, 0x60, 0x0F, 0xEF, 0xC3, 0x33, 0xFF,
    0xEE, 0x33, 0xFE, 0xED, 0x33, 0xFD, 0xEC, 0x33, 0xFC, 0xD8, 0xF1, 0x22, 0xE4, 0x93, 0xFC, 0x74,
    0x01, 0x93, 0xFD, 0x74, 0x02, 0x93, 0xFE, 0x74, 0x03, 0x93, 0xFF, 0x22, 0xE6, 0xFB, 0x08, 0xE6,
    0xF9, 0x08, 0xE6, 0xFA, 0x08, 0xE6, 0xCB, 0xF8, 0x22, 0xEC, 0xF6, 0x08, 0xED, 0xF6, 0x08, 0xEE,
    0xF6, 0x08, 0xEF, 0xF6, 0x22, 0xA4, 0x25, 0x82, 0xF5, 0x82, 0xE5, 0xF0, 0x35, 0x83, 0xF5, 0x83,
    0x22, 0xD0, 0x83, 0xD0, 0x82, 0xF8, 0xE4, 0x93, 0x70, 0x12, 0x74, 0x01, 0x93, 0x70, 0x0D, 0xA3,
    0xA3, 0x93, 0xF8, 0x74, 0x01, 0x93, 0xF5, 0x82, 0x88, 0x83, 0xE4, 0x73, 0x74, 0x02, 0x93, 0x68,
    0x60, 0xEF, 0xA3, 0xA3, 0xA3, 0x80, 0xDF, 0x90, 0x38, 0x04, 0x78, 0x52, 0x12, 0x0B, 0xFD, 0x90,
    0x38, 0x00, 0xE0, 0xFE, 0xA3, 0xE0, 0xFD, 0xED, 0xFF, 0xC3, 0x12, 0x0B, 0x9E, 0x90, 0x38, 0x10,
    0x12, 0x0B, 0x92, 0x90, 0x38, 0x06, 0x78, 0x54, 0x12, 0x0B, 0xFD, 0x90, 0x38, 0x02, 0xE0, 0xFE,
    0xA3, 0xE0, 0xFD, 0xED, 0xFF, 0xC3, 0x12, 0x0B, 0x9E, 0x90, 0x38, 0x12, 0x12, 0x0B, 0x92, 0xA3,
    0xE0, 0xB4, 0x31, 0x07, 0x78, 0x52, 0x79, 0x52, 0x12, 0x0C, 0x13, 0x90, 0x38, 0x14, 0xE0, 0xB4,
    0x71, 0x15, 0x78, 0x52, 0xE6, 0xFE, 0x08, 0xE6, 0x78, 0x02, 0xCE, 0xC3, 0x13, 0xCE, 0x13, 0xD8,
    0xF9, 0x79, 0x53, 0xF7, 0xEE, 0x19, 0xF7, 0x90, 0x38, 0x15, 0xE0, 0xB4, 0x31, 0x07, 0x78, 0x54,
    0x79, 0x54, 0x12, 0x0C, 0x13, 0x90, 0x38, 0x15, 0xE0, 0xB4, 0x71, 0x15, 0x78, 0x54, 0xE6, 0xFE,
    0x08, 0xE6, 0x78, 0x02, 0xCE, 0xC3, 0x13, 0xCE, 0x13, 0xD8, 0xF9, 0x79, 0x55, 0xF7, 0xEE, 0x19,
    0xF7, 0x79, 0x52, 0x12, 0x0B, 0xD9, 0x09, 0x12, 0x0B, 0xD9, 0xAF, 0x47, 0x12, 0x0B, 0xB2, 0xE5,
    0x44, 0xFB, 0x7A, 0x00, 0xFD, 0x7C, 0x00, 0x12, 0x04, 0xD3, 0x78, 0x5A, 0xA6, 0x06, 0x08, 0xA6,
    0x07, 0xAF, 0x45, 0x12, 0x0B, 0xB2, 0xAD, 0x03, 0x7C, 0x00, 0x12, 0x04, 0xD3, 0x78, 0x56, 0xA6,
    0x06, 0x08, 0xA6, 0x07, 0xAF, 0x48, 0x78, 0x54, 0x12, 0x0B, 0xB4, 0xE5, 0x43, 0xFB, 0xFD, 0x7C,
    0x00, 0x12, 0x04, 0xD3, 0x78, 0x5C, 0xA6, 0x06, 0x08, 0xA6, 0x07, 0xAF, 0x46, 0x7E, 0x00, 0x78,
    0x54, 0x12, 0x0B, 0xB6, 0xAD, 0x03, 0x7C, 0x00, 0x12, 0x04, 0xD3, 0x78, 0x58, 0xA6, 0x06, 0x08,
    0xA6, 0x07, 0xC3, 0x78, 0x5B, 0xE6, 0x94, 0x08, 0x18, 0xE6, 0x94, 0x00, 0x50, 0x05, 0x76, 0x00,
    0x08, 0x76, 0x08, 0xC3, 0x78, 0x5D, 0xE6, 0x94, 0x08, 0x18, 0xE6, 0x94, 0x00, 0x50, 0x05, 0x76,
    0x00, 0x08, 0x76, 0x08, 0x78, 0x5A, 0x12, 0x0B, 0xC6, 0xFF, 0xD3, 0x78, 0x57, 0xE6, 0x9F, 0x18,
    0xE6, 0x9E, 0x40, 0x0E, 0x78, 0x5A, 0xE6, 0x13, 0xFE, 0x08, 0xE6, 0x78, 0x57, 0x12, 0x0C, 0x08,
    0x80, 0x04, 0x7E, 0x00, 0x7F, 0x00, 0x78, 0x5E, 0x12, 0x0B, 0xBE, 0xFF, 0xD3, 0x78, 0x59, 0xE6,
    0x9F, 0x18, 0xE6, 0x9E, 0x40, 0x0E, 0x78, 0x5C, 0xE6, 0x13, 0xFE, 0x08, 0xE6, 0x78, 0x59, 0x12,
    0x0C, 0x08, 0x80, 0x04, 0x7E, 0x00, 0x7F, 0x00, 0xE4, 0xFC, 0xFD, 0x78, 0x62, 0x12, 0x06, 0x99,
    0x78, 0x5A, 0x12, 0x0B, 0xC6, 0x78, 0x57, 0x26, 0xFF, 0xEE, 0x18, 0x36, 0xFE, 0x78, 0x66, 0x12,
    0x0B, 0xBE, 0x78, 0x59, 0x26, 0xFF, 0xEE, 0x18, 0x36, 0xFE, 0xE4, 0xFC, 0xFD, 0x78, 0x6A, 0x12,
    0x06, 0x99, 0x12, 0x0B, 0xCE, 0x78, 0x66, 0x12, 0x06, 0x8C, 0xD3, 0x12, 0x06, 0x45, 0x40, 0x08,
    0x12, 0x0B, 0xCE, 0x78, 0x66, 0x12, 0x06, 0x99, 0x78, 0x54, 0x12, 0x0B, 0xD0, 0x78, 0x6A, 0x12,
    0x06, 0x8C, 0xD3, 0x12, 0x06, 0x45, 0x40, 0x0A, 0x78, 0x54, 0x12, 0x0B, 0xD0, 0x78, 0x6A, 0x12,
    0x06, 0x99, 0x78, 0x61, 0xE6, 0x90, 0x60, 0x01, 0xF0, 0x78, 0x65, 0xE6, 0xA3, 0xF0, 0x78, 0x69,
    0xE6, 0xA3, 0xF0, 0x78, 0x55, 0xE6, 0xA3, 0xF0, 0x7D, 0x01, 0x78, 0x61, 0x12, 0x0B, 0xE9, 0x24,
    0x01, 0x12, 0x0B, 0xA6, 0x78, 0x65, 0x12, 0x0B, 0xE9, 0x24, 0x02, 0x12, 0x0B, 0xA6, 0x78, 0x69,
    0x12, 0x0B, 0xE9, 0x24, 0x03, 0x12, 0x0B, 0xA6, 0x78, 0x6D, 0x12, 0x0B, 0xE9, 0x24, 0x04, 0x12,
    0x0B, 0xA6, 0x0D, 0xBD, 0x05, 0xD4, 0xC2, 0x0E, 0xC2, 0x06, 0x22, 0x85, 0x08, 0x41, 0x90, 0x30,
    0x24, 0xE0, 0xF5, 0x3D, 0xA3, 0xE0, 0xF5, 0x3E, 0xA3, 0xE0, 0xF5, 0x3F, 0xA3, 0xE0, 0xF5, 0x40,
    0xA3, 0xE0, 0xF5, 0x3C, 0xD2, 0x34, 0xE5, 0x41, 0x12, 0x06, 0xB1, 0x09, 0x31, 0x03, 0x09, 0x35,
    0x04, 0x09, 0x3B, 0x05, 0x09, 0x3E, 0x06, 0x09, 0x41, 0x07, 0x09, 0x4A, 0x08, 0x09, 0x5B, 0x12,
    0x09, 0x73, 0x18, 0x09, 0x89, 0x19, 0x09, 0x5E, 0x1A, 0x09, 0x6A, 0x1B, 0x09, 0xAD, 0x80, 0x09,
    0xB2, 0x81, 0x0A, 0x1D, 0x8F, 0x0A, 0x09, 0x90, 0x0A, 0x1D, 0x91, 0x0A, 0x1D, 0x92, 0x0A, 0x1D,
    0x93, 0x0A, 0x1D, 0x94, 0x0A, 0x1D, 0x98, 0x0A, 0x17, 0x9F, 0x0A, 0x1A, 0xEC, 0x00, 0x00, 0x0A,
    0x38, 0x12, 0x0F, 0x74, 0x22, 0x12, 0x0F, 0x74, 0xD2, 0x03, 0x22, 0xD2, 0x03, 0x22, 0xC2, 0x03,
    0x22, 0xA2, 0x37, 0xE4, 0x33, 0xF5, 0x3C, 0x02, 0x0A, 0x1D, 0xC2, 0x01, 0xC2, 0x02, 0xC2, 0x03,
    0x12, 0x0D, 0x0D, 0x75, 0x1E, 0x70, 0xD2, 0x35, 0x02, 0x0A, 0x1D, 0x02, 0x0A, 0x04, 0x85, 0x40,
    0x4A, 0x85, 0x3C, 0x4B, 0x12, 0x0A, 0xFF, 0x02, 0x0A, 0x1D, 0x85, 0x4A, 0x40, 0x85, 0x4B, 0x3C,
    0x02, 0x0A, 0x1D, 0xE4, 0xF5, 0x22, 0xF5, 0x23, 0x85, 0x40, 0x31, 0x85, 0x3F, 0x30, 0x85, 0x3E,
    0x2F, 0x85, 0x3D, 0x2E, 0x12, 0x0F, 0x46, 0x80, 0x1F, 0x75, 0x22, 0x00, 0x75, 0x23, 0x01, 0x74,
    0xFF, 0xF5, 0x2D, 0xF5, 0x2C, 0xF5, 0x2B, 0xF5, 0x2A, 0x12, 0x0F, 0x46, 0x85, 0x2D, 0x40, 0x85,
    0x2C, 0x3F, 0x85, 0x2B, 0x3E, 0x85, 0x2A, 0x3D, 0xE4, 0xF5, 0x3C, 0x80, 0x70, 0x12, 0x0F, 0x16,
    0x80, 0x6B, 0x85, 0x3D, 0x45, 0x85, 0x3E, 0x46, 0xE5, 0x47, 0xC3, 0x13, 0xFF, 0xE5, 0x45, 0xC3,
    0x9F, 0x50, 0x02, 0x8F, 0x45, 0xE5, 0x48, 0xC3, 0x13, 0xFF, 0xE5, 0x46, 0xC3, 0x9F, 0x50, 0x02,
    0x8F, 0x46, 0xE5, 0x47, 0xC3, 0x13, 0xFF, 0xFD, 0xE5, 0x45, 0x2D, 0xFD, 0xE4, 0x33, 0xFC, 0xE5,
    0x44, 0x12, 0x0F, 0x90, 0x40, 0x05, 0xE5, 0x44, 0x9F, 0xF5, 0x45, 0xE5, 0x48, 0xC3, 0x13, 0xFF,
    0xFD, 0xE5, 0x46, 0x2D, 0xFD, 0xE4, 0x33, 0xFC, 0xE5, 0x43, 0x12, 0x0F, 0x90, 0x40, 0x05, 0xE5,
    0x43, 0x9F, 0xF5, 0x46, 0x12, 0x06, 0xD7, 0x80, 0x14, 0x85, 0x40, 0x48, 0x85, 0x3F, 0x47, 0x85,
    0x3E, 0x46, 0x85, 0x3D, 0x45, 0x80, 0x06, 0x02, 0x06, 0xD7, 0x12, 0x0D, 0x7E, 0x90, 0x30, 0x24,
    0xE5, 0x3D, 0xF0, 0xA3, 0xE5, 0x3E, 0xF0, 0xA3, 0xE5, 0x3F, 0xF0, 0xA3, 0xE5, 0x40, 0xF0, 0xA3,
    0xE5, 0x3C, 0xF0, 0x90, 0x30, 0x23, 0xE4, 0xF0, 0x22, 0xC0, 0xE0, 0xC0, 0x83, 0xC0, 0x82, 0xC0,
    0xD0, 0x90, 0x3F, 0x0C, 0xE0, 0xF5, 0x32, 0xE5, 0x32, 0x30, 0xE3, 0x74, 0x30, 0x36, 0x66, 0x90,
    0x60, 0x19, 0xE0, 0xF5, 0x0A, 0xA3, 0xE0, 0xF5, 0x0B, 0x90, 0x60, 0x1D, 0xE0, 0xF5, 0x14, 0xA3,
    0xE0, 0xF5, 0x15, 0x90, 0x60, 0x21, 0xE0, 0xF5, 0x0C, 0xA3, 0xE0, 0xF5, 0x0D, 0x90, 0x60, 0x29,
    0xE0, 0xF5, 0x0E, 0xA3, 0xE0, 0xF5, 0x0F, 0x90, 0x60, 0x31, 0xE0, 0xF5, 0x10, 0xA3, 0xE0, 0xF5,
    0x11, 0x90, 0x60, 0x39, 0xE0, 0xF5, 0x12, 0xA3, 0xE0, 0xF5, 0x13, 0x30, 0x01, 0x06, 0x30, 0x33,
    0x03, 0xD3, 0x80, 0x01, 0xC3, 0x92, 0x09, 0x30, 0x02, 0x06, 0x30, 0x33, 0x03, 0xD3, 0x80, 0x01,
    0xC3, 0x92, 0x0A, 0x30, 0x33, 0x0C, 0x30, 0x03, 0x09, 0x20, 0x02, 0x06, 0x20, 0x01, 0x03, 0xD3,
    0x80, 0x01, 0xC3, 0x92, 0x0B, 0x90, 0x30, 0x01, 0xE0, 0x44, 0x40, 0xF0, 0xE0, 0x54, 0xBF, 0xF0,
    0xE5, 0x32, 0x30, 0xE1, 0x14, 0x30, 0x34, 0x11, 0x90, 0x30, 0x22, 0xE0, 0xF5, 0x08, 0xE4, 0xF0,
    0x30, 0x00, 0x03, 0xD3, 0x80, 0x01, 0xC3, 0x92, 0x08, 0xE5, 0x32, 0x30, 0xE5, 0x12, 0x90, 0x56,
    0xA1, 0xE0, 0xF5, 0x09, 0x30, 0x31, 0x09, 0x30, 0x05, 0x03, 0xD3, 0x80, 0x01, 0xC3, 0x92, 0x0D,
    0x90, 0x3F, 0x0C, 0xE5, 0x32, 0xF0, 0xD0, 0xD0, 0xD0, 0x82, 0xD0, 0x83, 0xD0, 0xE0, 0x32, 0x90,
    0x0E, 0x7E, 0xE4, 0x93, 0xFE, 0x74, 0x01, 0x93, 0xFF, 0xC3, 0x90, 0x0E, 0x7C, 0x74, 0x01, 0x93,
    0x9F, 0xFF, 0xE4, 0x93, 0x9E, 0xFE, 0xE4, 0x8F, 0x3B, 0x8E, 0x3A, 0xF5, 0x39, 0xF5, 0x38, 0xAB,
    0x3B, 0xAA, 0x3A, 0xA9, 0x39, 0xA8, 0x38, 0xAF, 0x4B, 0xFC, 0xFD, 0xFE, 0x12, 0x05, 0x28, 0x12,
    0x0D, 0xE1, 0xE4, 0x7B, 0xFF, 0xFA, 0xF9, 0xF8, 0x12, 0x05, 0xB3, 0x12, 0x0D, 0xE1, 0x90, 0x0E,
    0x69, 0xE4, 0x12, 0x0D, 0xF6, 0x12, 0x0D, 0xE1, 0xE4, 0x85, 0x4A, 0x37, 0xF5, 0x36, 0xF5, 0x35,
    0xF5, 0x34, 0xAF, 0x37, 0xAE, 0x36, 0xAD, 0x35, 0xAC, 0x34, 0xA3, 0x12, 0x0D, 0xF6, 0x8F, 0x37,
    0x8E, 0x36, 0x8D, 0x35, 0x8C, 0x34, 0xE5, 0x3B, 0x45, 0x37, 0xF5, 0x3B, 0xE5, 0x3A, 0x45, 0x36,
    0xF5, 0x3A, 0xE5, 0x39, 0x45, 0x35, 0xF5, 0x39, 0xE5, 0x38, 0x45, 0x34, 0xF5, 0x38, 0xE4, 0xF5,
    0x22, 0xF5, 0x23, 0x85, 0x3B, 0x31, 0x85, 0x3A, 0x30, 0x85, 0x39, 0x2F, 0x85, 0x38, 0x2E, 0x02,
    0x0F, 0x46, 0xE0, 0xA3, 0xE0, 0x75, 0xF0, 0x02, 0xA4, 0xFF, 0xAE, 0xF0, 0xC3, 0x08, 0xE6, 0x9F,
    0xF6, 0x18, 0xE6, 0x9E, 0xF6, 0x22, 0xFF, 0xE5, 0xF0, 0x34, 0x60, 0x8F, 0x82, 0xF5, 0x83, 0xEC,
    0xF0, 0x22, 0x78, 0x52, 0x7E, 0x00, 0xE6, 0xFC, 0x08, 0xE6, 0xFD, 0x02, 0x04, 0xC1, 0xE4, 0xFC,
    0xFD, 0x12, 0x06, 0x99, 0x78, 0x5C, 0xE6, 0xC3, 0x13, 0xFE, 0x08, 0xE6, 0x13, 0x22, 0x78, 0x52,
    0xE6, 0xFE, 0x08, 0xE6, 0xFF, 0xE4, 0xFC, 0xFD, 0x22, 0xE7, 0xC4, 0xF8, 0x54, 0xF0, 0xC8, 0x68,
    0xF7, 0x09, 0xE7, 0xC4, 0x54, 0x0F, 0x48, 0xF7, 0x22, 0xE6, 0xFC, 0xED, 0x75, 0xF0, 0x04, 0xA4,
    0x22, 0x12, 0x06, 0x7C, 0x8F, 0x48, 0x8E, 0x47, 0x8D, 0x46, 0x8C, 0x45, 0x22, 0xE0, 0xFE, 0xA3,
    0xE0, 0xFD, 0xEE, 0xF6, 0xED, 0x08, 0xF6, 0x22, 0x13, 0xFF, 0xC3, 0xE6, 0x9F, 0xFF, 0x18, 0xE6,
    0x9E, 0xFE, 0x22, 0xE6, 0xC3, 0x13, 0xF7, 0x08, 0xE6, 0x13, 0x09, 0xF7, 0x22, 0xAD, 0x39, 0xAC,
    0x38, 0xFA, 0xF9, 0xF8, 0x12, 0x05, 0x28, 0x8F, 0x3B, 0x8E, 0x3A, 0x8D, 0x39, 0x8C, 0x38, 0xAB,
    0x37, 0xAA, 0x36, 0xA9, 0x35, 0xA8, 0x34, 0x22, 0x93, 0xFF, 0xE4, 0xFC, 0xFD, 0xFE, 0x12, 0x05,
    0x28, 0x8F, 0x37, 0x8E, 0x36, 0x8D, 0x35, 0x8C, 0x34, 0x22, 0x78, 0x84, 0xE6, 0xFE, 0x08, 0xE6,
    0xFF, 0xE4, 0x8F, 0x37, 0x8E, 0x36, 0xF5, 0x35, 0xF5, 0x34, 0x22, 0x90, 0x0E, 0x8C, 0xE4, 0x93,
    0x25, 0xE0, 0x24, 0x0A, 0xF8, 0xE6, 0xFE, 0x08, 0xE6, 0xFF, 0x22, 0xE6, 0xFE, 0x08, 0xE6, 0xFF,
    0xE4, 0x8F, 0x3B, 0x8E, 0x3A, 0xF5, 0x39, 0xF5, 0x38, 0x22, 0x78, 0x4E, 0xE6, 0xFE, 0x08, 0xE6,
    0xFF, 0x22, 0xEF, 0x25, 0xE0, 0x24, 0x4E, 0xF8, 0xE6, 0xFC, 0x08, 0xE6, 0xFD, 0x22, 0x78, 0x89,
    0xEF, 0x26, 0xF6, 0x18, 0xE4, 0x36, 0xF6, 0x22, 0x75, 0x89, 0x03, 0x75, 0xA8, 0x01, 0x75, 0xB8,
    0x04, 0x75, 0x34, 0xFF, 0x75, 0x35, 0x0E, 0x75, 0x36, 0x15, 0x75, 0x37, 0x0D, 0x12, 0x0E, 0x9A,
    0x12, 0x00, 0x09, 0x12, 0x0F, 0x16, 0x12, 0x00, 0x06, 0xD2, 0x00, 0xD2, 0x34, 0xD2, 0xAF, 0x75,
    0x34, 0xFF, 0x75, 0x35, 0x0E, 0x75, 0x36, 0x49, 0x75, 0x37, 0x03, 0x12, 0x0E, 0x9A, 0x30, 0x08,
    0x09, 0xC2, 0x34, 0x12, 0x08, 0xCB, 0xC2, 0x08, 0xD2, 0x34, 0x30, 0x0B, 0x09, 0xC2, 0x36, 0x12,
    0x02, 0x6C, 0xC2, 0x0B, 0xD2, 0x36, 0x30, 0x09, 0x09, 0xC2, 0x36, 0x12, 0x00, 0x0E, 0xC2, 0x09,
    0xD2, 0x36, 0x30, 0x0E, 0x03, 0x12, 0x06, 0xD7, 0x30, 0x35, 0xD3, 0x90, 0x30, 0x29, 0xE5, 0x1E,
    0xF0, 0xB4, 0x10, 0x05, 0x90, 0x30, 0x23, 0xE4, 0xF0, 0xC2, 0x35, 0x80, 0xC1, 0xE4, 0xF5, 0x4B,
    0x90, 0x0E, 0x7A, 0x93, 0xFF, 0xE4, 0x8F, 0x37, 0xF5, 0x36, 0xF5, 0x35, 0xF5, 0x34, 0xAF, 0x37,
    0xAE, 0x36, 0xAD, 0x35, 0xAC, 0x34, 0x90, 0x0E, 0x6A, 0x12, 0x0D, 0xF6, 0x8F, 0x37, 0x8E, 0x36,
    0x8D, 0x35, 0x8C, 0x34, 0x90, 0x0E, 0x72, 0x12, 0x06, 0x7C, 0xEF, 0x45, 0x37, 0xF5, 0x37, 0xEE,
    0x45, 0x36, 0xF5, 0x36, 0xED, 0x45, 0x35, 0xF5, 0x35, 0xEC, 0x45, 0x34, 0xF5, 0x34, 0xE4, 0xF5,
    0x22, 0xF5, 0x23, 0x85, 0x37, 0x31, 0x85, 0x36, 0x30, 0x85, 0x35, 0x2F, 0x85, 0x34, 0x2E, 0x12,
    0x0F, 0x46, 0xE4, 0xF5, 0x22, 0xF5, 0x23, 0x90, 0x0E, 0x72, 0x12, 0x0D, 0xEA, 0x12, 0x0F, 0x46,
    0xE4, 0xF5, 0x22, 0xF5, 0x23, 0x90, 0x0E, 0x6E, 0x12, 0x0D, 0xEA, 0x02, 0x0F, 0x46, 0xE5, 0x40,
    0x24, 0xF2, 0xF5, 0x37, 0xE5, 0x3F, 0x34, 0x43, 0xF5, 0x36, 0xE5, 0x3E, 0x34, 0xA2, 0xF5, 0x35,
    0xE5, 0x3D, 0x34, 0x28, 0xF5, 0x34, 0xE5, 0x37, 0xFF, 0xE4, 0xFE, 0xFD, 0xFC, 0x78, 0x18, 0x12,
    0x06, 0x69, 0x8F, 0x40, 0x8E, 0x3F, 0x8D, 0x3E, 0x8C, 0x3D, 0xE5, 0x37, 0x54, 0xA0, 0xFF, 0xE5,
    0x36, 0xFE, 0xE4, 0xFD, 0xFC, 0x78, 0x07, 0x12, 0x06, 0x56, 0x78, 0x10, 0x12, 0x0F, 0x9A, 0xE4,
    0xFF, 0xFE, 0xE5, 0x35, 0xFD, 0xE4, 0xFC, 0x78, 0x0E, 0x12, 0x06, 0x56, 0x12, 0x0F, 0x9D, 0xE4,
    0xFF, 0xFE, 0xFD, 0xE5, 0x34, 0xFC, 0x78, 0x18, 0x12, 0x06, 0x56, 0x78, 0x08, 0x12, 0x0F, 0x9A,
    0x22, 0x8F, 0x3B, 0x8E, 0x3A, 0x8D, 0x39, 0x8C, 0x38, 0x22, 0x12, 0x06, 0x7C, 0x8F, 0x31, 0x8E,
    0x30, 0x8D, 0x2F, 0x8C, 0x2E, 0x22, 0x93, 0xF9, 0xF8, 0x02, 0x06, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x12, 0x01, 0x17, 0x08, 0x31, 0x15, 0x53, 0x54, 0x44, 0x20, 0x20, 0x20, 0x20, 0x20, 0x13, 0x01,
    0x10, 0x01, 0x56, 0x40, 0x1A, 0x30, 0x29, 0x7E, 0x00, 0x30, 0x04, 0x20, 0xDF, 0x30, 0x05, 0x40,
    0xBF, 0x50, 0x03, 0x00, 0xFD, 0x50, 0x27, 0x01, 0xFE, 0x60, 0x00, 0x11, 0x00, 0x3F, 0x05, 0x30,
    0x00, 0x3F, 0x06, 0x22, 0x00, 0x3F, 0x01, 0x2A, 0x00, 0x3F, 0x02, 0x00, 0x00, 0x36, 0x06, 0x07,
    0x00, 0x3F, 0x0B, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x30, 0x01, 0x40, 0xBF, 0x30, 0x01, 0x00,
    0xBF, 0x30, 0x29, 0x70, 0x00, 0x3A, 0x00, 0x00, 0xFF, 0x3A, 0x00, 0x00, 0xFF, 0x36, 0x03, 0x36,
    0x02, 0x41, 0x44, 0x58, 0x20, 0x18, 0x10, 0x0A, 0x04, 0x04, 0x00, 0x03, 0xFF, 0x64, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x06, 0x00, 0x03, 0x51, 0x00, 0x7A,
    0x50, 0x3C, 0x28, 0x1E, 0x10, 0x10, 0x50, 0x2D, 0x28, 0x16, 0x10, 0x10, 0x02, 0x00, 0x10, 0x0C,
    0x10, 0x04, 0x0C, 0x6E, 0x06, 0x05, 0x00, 0xA5, 0x5A, 0x00, 0xAE, 0x35, 0xAF, 0x36, 0xE4, 0xFD,
    0xED, 0xC3, 0x95, 0x37, 0x50, 0x33, 0x12, 0x0F, 0xE2, 0xE4, 0x93, 0xF5, 0x38, 0x74, 0x01, 0x93,
    0xF5, 0x39, 0x45, 0x38, 0x60, 0x23, 0x85, 0x39, 0x82, 0x85, 0x38, 0x83, 0xE0, 0xFC, 0x12, 0x0F,
    0xE2, 0x74, 0x03, 0x93, 0x52, 0x04, 0x12, 0x0F, 0xE2, 0x74, 0x02, 0x93, 0x42, 0x04, 0x85, 0x39,
    0x82, 0x85, 0x38, 0x83, 0xEC, 0xF0, 0x0D, 0x80, 0xC7, 0x22, 0x78, 0xBE, 0xE6, 0xD3, 0x08, 0xFF,
    0xE6, 0x64, 0x80, 0xF8, 0xEF, 0x64, 0x80, 0x98, 0x22, 0x93, 0xFF, 0x7E, 0x00, 0xE6, 0xFC, 0x08,
    0xE6, 0xFD, 0x12, 0x04, 0xC1, 0x78, 0xC1, 0xE6, 0xFC, 0x08, 0xE6, 0xFD, 0xD3, 0xEF, 0x9D, 0xEE,
    0x9C, 0x22, 0x78, 0xBD, 0xD3, 0xE6, 0x64, 0x80, 0x94, 0x80, 0x22, 0x25, 0xE0, 0x24, 0x0A, 0xF8,
    0xE6, 0xFE, 0x08, 0xE6, 0xFF, 0x22, 0xE5, 0x3C, 0xD3, 0x94, 0x00, 0x40, 0x0B, 0x90, 0x0E, 0x88,
    0x12, 0x0B, 0xF1, 0x90, 0x0E, 0x86, 0x80, 0x09, 0x90, 0x0E, 0x82, 0x12, 0x0B, 0xF1, 0x90, 0x0E,
    0x80, 0xE4, 0x93, 0xF5, 0x44, 0xA3, 0xE4, 0x93, 0xF5, 0x43, 0xD2, 0x06, 0x30, 0x06, 0x03, 0xD3,
    0x80, 0x01, 0xC3, 0x92, 0x0E, 0x22, 0xA2, 0xAF, 0x92, 0x32, 0xC2, 0xAF, 0xE5, 0x23, 0x45, 0x22,
    0x90, 0x0E, 0x5D, 0x60, 0x0E, 0x12, 0x0F, 0xCB, 0xE0, 0xF5, 0x2C, 0x12, 0x0F, 0xC8, 0xE0, 0xF5,
    0x2D, 0x80, 0x0C, 0x12, 0x0F, 0xCB, 0xE5, 0x30, 0xF0, 0x12, 0x0F, 0xC8, 0xE5, 0x31, 0xF0, 0xA2,
    0x32, 0x92, 0xAF, 0x22, 0xD2, 0x01, 0xC2, 0x02, 0xE4, 0xF5, 0x1F, 0xF5, 0x1E, 0xD2, 0x35, 0xD2,
    0x33, 0xD2, 0x36, 0xD2, 0x01, 0xC2, 0x02, 0xF5, 0x1F, 0xF5, 0x1E, 0xD2, 0x35, 0xD2, 0x33, 0x22,
    0xFB, 0xD3, 0xED, 0x9B, 0x74, 0x80, 0xF8, 0x6C, 0x98, 0x22, 0x12, 0x06, 0x69, 0xE5, 0x40, 0x2F,
    0xF5, 0x40, 0xE5, 0x3F, 0x3E, 0xF5, 0x3F, 0xE5, 0x3E, 0x3D, 0xF5, 0x3E, 0xE5, 0x3D, 0x3C, 0xF5,
    0x3D, 0x22, 0xC0, 0xE0, 0xC0, 0x83, 0xC0, 0x82, 0x90, 0x3F, 0x0D, 0xE0, 0xF5, 0x33, 0xE5, 0x33,
    0xF0, 0xD0, 0x82, 0xD0, 0x83, 0xD0, 0xE0, 0x32, 0x90, 0x0E, 0x5F, 0xE4, 0x93, 0xFE, 0x74, 0x01,
    0x93, 0xF5, 0x82, 0x8E, 0x83, 0x22, 0x78, 0x7F, 0xE4, 0xF6, 0xD8, 0xFD, 0x75, 0x81, 0xCD, 0x02,
    0x0C, 0x98, 0x8F, 0x82, 0x8E, 0x83, 0x75, 0xF0, 0x04, 0xED, 0x02, 0x06, 0xA5,
};

static const struct OV5640_Reg sc_init_table_mipi[] = {
	/* Power down */
	{0x3008, 0x42},
	/* System and IO pad control */
	{0x3000, 0x00},
	{0x3002, 0x1C},
	{0x3004, 0xFF},
	{0x3006, 0xC3},
	{0x300E, 0x45},
	{0x302D, 0x60},
	{0x302E, 0x08},
	{0x3034, 0x18},
	{0x3035, 0x12},
	{0x3036, 0x7B},
	{0x3037, 0x08},
	/* SCCB control */
	{0x3103, 0x03},
	{0x3108, 0x12},
	/* AWB gain control */
	{0x3400, 0x07},
	{0x3401, 0x70},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x05},
	{0x3406, 0x00},
	/* AEC/AGC control */
	{0x3500, 0x00},
	{0x3501, 0xFF},
	{0x3502, 0x00},
	{0x3503, 0x00}, /* 使用自动曝光，AEC 和 AGC 自动控制 */
	{0x3505, 0xC0},
	{0x350A, 0x00},
	{0x350B, 0x20},
	/* VCM control */
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x3612, 0x29},
	{0x3618, 0x00},
	/* timing control */
	{0x3803, 0x04},
	{0x3808, 0x04},
	{0x3809, 0x00},
	{0x380A, 0x02},
	{0x380B, 0x58},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3824, 0x02},
	/* AEC/AGC power down domain control */
	{0x3A0F, 0x30},
	{0x3A10, 0x28},
	{0x3A11, 0x60},
	{0x3A13, 0x43},
	{0x3A18, 0x00},
	{0x3A19, 0xF8},
	{0x3A1B, 0x30},
	{0x3A1E, 0x26},
	{0x3A1F, 0x14},
	/* BLC control */
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x4005, 0x1A},
	/* format control */
	{0x4300, 0x32},
	/* VFIFO control */
	{0x460C, 0x22},
	/* DVP control */
	{0x471C, 0x50},
	{0x4741, 0x00},
	/* MIPI control */
	{0x4837, 0x10},
	/* ISP top control */
	{0x5000, 0xA7},
	{0x5001, 0xA3},
	{0x501F, 0x00},
	{0x503D, 0x00},
	/* gamma control */
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7D},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548A, 0x9A},
	{0x548B, 0xAA},
	{0x548C, 0xB8},
	{0x548D, 0xCD},
	{0x548E, 0xDD},
	{0x548F, 0xEA},
	{0x5490, 0x1D},
	/* SDE control. */
	{0x5580, 0x06},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558A, 0x00},
	{0x558B, 0xF8},
	/* LENC control. */
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0F},
	{0x5803, 0x0F},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0C},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580A, 0x08},
	{0x580B, 0x0D},
	{0x580C, 0x08},
	{0x580D, 0x03},
	{0x580E, 0x00},
	{0x580F, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0D},
	{0x5819, 0x08},
	{0x581A, 0x05},
	{0x581B, 0x06},
	{0x581C, 0x08},
	{0x581D, 0x0e},
	{0x581E, 0x29},
	{0x581F, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582A, 0x24},
	{0x582B, 0x22},
	{0x582C, 0x24},
	{0x582D, 0x24},
	{0x582E, 0x06},
	{0x582F, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583A, 0x26},
	{0x583B, 0x28},
	{0x583C, 0x42},
	{0x583D, 0xCE},
	/* Not in datasheet */
	{0x3620, 0x52},
	{0x3621, 0xE0},
	{0x3622, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0E},
	{0x3632, 0xE2},
	{0x3633, 0x12},
	{0x3634, 0x40},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3703, 0x5A},
	{0x3704, 0xA0},
	{0x3705, 0x1A},
	{0x3709, 0x52},
	{0x370B, 0x60},
	{0x370C, 0x03},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x371B, 0x20},
	{0x3731, 0x12},
	{0x3901, 0x0a},
	{0x3905, 0x02},
	{0x3906, 0x10},
};

// https://github.com/ArduCAM/Arduino/blob/master/ArduCAM/ov5640_regs.h
static const struct OV5640_Reg sc_init_table_dvp[] = {
	/* system and IO pad control */
	{0x3000, 0x00},
    {0x3002, 0x1C},
    {0x3004, 0xFF},
    {0x3006, 0xC3},
	{0x3008, 0x02},
    {0x300E, 0x58},
    {0x302E, 0x00},
	{0x3017, 0xFF},
    {0x3018, 0xFF},
	{0x302C, 0x02},
	{0x302D, 0x60},
	{0x3034, 0x1A},
    {0x3035, 0x11},
    {0x3036, 0x64},
    {0x3037, 0x13},
    {0x3038, 0x00},
    {0x3039, 0x00},
	/* SCCB control */
	{0x3103, 0x03},
    {0x3108, 0x01},
	/* AWB gain control */
    {0x3400, 0x06},
    {0x3401, 0x80},
    {0x3402, 0x04},
    {0x3403, 0x00},
    {0x3404, 0x06},
    {0x3405, 0x00},
	{0x3406, 0x00},
	/* AEC/AGC control */
	{0x3503, 0x00},
	/* VCM control */
	{0x3600, 0x08},
    {0x3601, 0x33},
	/* timing control */
	{0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x04},
    {0x3804, 0x0A},
    {0x3805, 0x3F},
    {0x3806, 0x07},
    {0x3807, 0x9B},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380A, 0x03},
    {0x380B, 0xC0},
	{0x380C, 0x07},
    {0x380D, 0x68},
    {0x380E, 0x03},
    {0x380F, 0xD8},
    {0x3810, 0x00},
    {0x3811, 0x10},
    {0x3812, 0x00},
    {0x3813, 0x06},
    {0x3814, 0x31},
    {0x3815, 0x31},
	{0x3820, 0x41},
    {0x3821, 0x07},
	/* AEC/AGC power down domain control */
	{0x3A00, 0x38},
    {0x3A02, 0x05},
    {0x3A03, 0xC4},
	{0x3A08, 0x00},
    {0x3A09, 0x93},
    {0x3A0A, 0x00},
    {0x3A0B, 0x7B},
    {0x3A0D, 0x08},
	{0x3A0E, 0x06},
	{0x3A0F, 0x42},
	{0x3A10, 0x38},
	{0x3A11, 0x60},
    {0x3A14, 0x05},
    {0x3A15, 0xc4},
	{0x3A18, 0x00},
    {0x3A19, 0xF8},
	{0x3A1B, 0x44},
	{0x3A1E, 0x36},
	{0x3A1F, 0x10},
	/* 50/60Hz detector control */
	{0x3C00, 0x04},
	{0x3C01, 0xB4},
	{0x3C04, 0x28},
    {0x3C05, 0x98},
    {0x3C06, 0x00},
    {0x3C07, 0x08},
    {0x3C08, 0x00},
    {0x3C09, 0x1C},
    {0x3C0A, 0x9C},
    {0x3C0B, 0x40},
	/* BLC control */
	{0x4001, 0x02},
    {0x4004, 0x02},
	{0x4005, 0x1A},
	/* format control */
	{0x4300, 0x30},
	/* JPEG control */
	{0x4401, 0x0d},
	{0x4407, 0x04},
	/* VFIFO control */
	{0x460B, 0x35},
    {0x460C, 0x22},
	/* DVP control */
	{0x4713, 0x04},
	{0x471C, 0x50},
	{0x4723, 0x03},
	{0x4740, 0x20},
	/* ISP top control */
	{0x5000, 0x27},
	{0x5001, 0xA3},
	{0x501F, 0x00},
	{0x5025, 0x00},
	/* AWB control */
	{0x5180, 0xFF},
    {0x5181, 0xF2},
    {0x5182, 0x00},
    {0x5183, 0x14},
    {0x5184, 0x25},
    {0x5185, 0x24},
    {0x5186, 0x16},
    {0x5187, 0x16},
    {0x5188, 0x16},
    {0x5189, 0x62},
    {0x518A, 0x62},
    {0x518B, 0xF0},
    {0x518C, 0xB2},
    {0x518D, 0x50},
    {0x518E, 0x30},
    {0x518F, 0x30},
    {0x5190, 0x50},
    {0x5191, 0xF8},
    {0x5192, 0x04},
    {0x5193, 0x70},
    {0x5194, 0xF0},
    {0x5195, 0xF0},
    {0x5196, 0x03},
    {0x5197, 0x01},
    {0x5198, 0x04},
    {0x5199, 0x12},
    {0x519A, 0x04},
    {0x519B, 0x00},
    {0x519C, 0x06},
    {0x519D, 0x82},
    {0x519E, 0x38},
	/* CIP control */
	{0x5300, 0x08},
    {0x5301, 0x30},
    {0x5302, 0x3F},
    {0x5303, 0x10},
    {0x5304, 0x08},
    {0x5305, 0x30},
    {0x5306, 0x18},
    {0x5307, 0x28},
    {0x5309, 0x08},
    {0x530A, 0x30},
    {0x530B, 0x04},
    {0x530C, 0x06},
	/* CMX control */
	{0x5381, 0x1E},
    {0x5382, 0x5B},
    {0x5383, 0x14},
    {0x5384, 0x06},
    {0x5385, 0x82},
    {0x5386, 0x88},
    {0x5387, 0x7C},
    {0x5388, 0x60},
    {0x5389, 0x1C},
    {0x538A, 0x01},
    {0x538B, 0x98},
	/* gamma control */
	{0x5480, 0x01},
    {0x5481, 0x06},
    {0x5482, 0x12},
    {0x5483, 0x24},
    {0x5484, 0x4A},
    {0x5485, 0x58},
    {0x5486, 0x65},
    {0x5487, 0x72},
    {0x5488, 0x7D},
    {0x5489, 0x88},
    {0x548A, 0x92},
    {0x548B, 0xA3},
    {0x548C, 0xB2},
    {0x548D, 0xC8},
    {0x548E, 0xDD},
    {0x548F, 0xF0},
    {0x5490, 0x15},
	/* SDE control */
	{0x5580, 0x06},
    {0x5583, 0x40},
    {0x5584, 0x20},
    {0x5589, 0x10},
    {0x558A, 0x00},
    {0x558B, 0xF8},
	/* LENC control */
	{0x5800, 0x20},
    {0x5801, 0x19},
    {0x5802, 0x17},
    {0x5803, 0x16},
    {0x5804, 0x18},
    {0x5805, 0x21},
    {0x5806, 0x0F},
    {0x5807, 0x0A},
    {0x5808, 0x07},
    {0x5809, 0x07},
    {0x580A, 0x0A},
    {0x580B, 0x0C},
    {0x580C, 0x0A},
    {0x580D, 0x03},
    {0x580E, 0x01},
    {0x580F, 0x01},
    {0x5810, 0x03},
    {0x5811, 0x09},
    {0x5812, 0x0A},
    {0x5813, 0x03},
    {0x5814, 0x01},
    {0x5815, 0x01},
    {0x5816, 0x03},
    {0x5817, 0x08},
    {0x5818, 0x10},
    {0x5819, 0x0A},
    {0x581A, 0x06},
    {0x581B, 0x06},
    {0x581C, 0x08},
    {0x581D, 0x0E},
    {0x581E, 0x22},
    {0x581F, 0x18},
    {0x5820, 0x13},
    {0x5821, 0x12},
    {0x5822, 0x16},
    {0x5823, 0x1E},
    {0x5824, 0x64},
    {0x5825, 0x2A},
    {0x5826, 0x2C},
    {0x5827, 0x2A},
    {0x5828, 0x46},
    {0x5829, 0x2A},
    {0x582A, 0x26},
    {0x582B, 0x24},
    {0x582C, 0x26},
    {0x582D, 0x2A},
    {0x582E, 0x28},
    {0x582F, 0x42},
    {0x5830, 0x40},
    {0x5831, 0x42},
    {0x5832, 0x08},
    {0x5833, 0x28},
    {0x5834, 0x26},
    {0x5835, 0x24},
    {0x5836, 0x26},
    {0x5837, 0x2A},
    {0x5838, 0x44},
    {0x5839, 0x4A},
    {0x583A, 0x2C},
    {0x583B, 0x2a},
    {0x583C, 0x46},
    {0x583D, 0xCE},
	/* scale control */
	{0x5688, 0x11},
    {0x5689, 0x11},
    {0x568A, 0x11},
    {0x568B, 0x11},
    {0x568C, 0x11},
    {0x568D, 0x11},
    {0x568E, 0x11},
    {0x568F, 0x11},
	/* Not in Datasheet */
	{0x3612, 0x29},
	{0x3618, 0x00},
	{0x3620, 0x52},
	{0x3621, 0xE0},
	{0x3622, 0x01},
	{0x3630, 0x2E},
    {0x3632, 0xE2},
    {0x3633, 0x23},
	{0x3634, 0x40},
	{0x3635, 0x1C},
	{0x3703, 0x5A},
	{0x3704, 0xA0},
	{0x3708, 0x64},
    {0x3709, 0x52},
	{0x370C, 0x03},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x370B, 0x60},
    {0x3705, 0x1A},
	{0x371B, 0x20},
	{0x3731, 0x12},
	{0x3824, 0x02},
	{0x3901, 0x0A},
	{0x3905, 0x02},
    {0x3906, 0x10},
    {0x4050, 0x6E},
    {0x4051, 0x8F},
};

static const struct OV5640_Reg sc_jpeg_cfg[] = {
	{0x4300, 0x30},
    {0x501F, 0x01},
    {0x3035, 0x21},
    {0x3036, 0x69},
    {0x3C07, 0x07},
    {0x3820, 0x46},
    {0x3821, 0x20},
    {0x3814, 0x11},
    {0x3815, 0x11},
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, 0x0A},
    {0x3805, 0x3F},
    {0x3806, 0x07},
    {0x3807, 0x9F},
    {0x3808, 0x02},
    {0x3809, 0x80},
    {0x380A, 0x01},
    {0x380B, 0xE0},
    {0x380C, 0x0B},
    {0x380D, 0x1C},
    {0x380E, 0x07},
    {0x380F, 0xB0},
    {0x3813, 0x04},
    {0x3618, 0x04},
    {0x3612, 0x2B},
    {0x3709, 0x12},
    {0x370C, 0x00},
    {0x4004, 0x06},
    {0x3002, 0x00},
    {0x3006, 0xFF},
    {0x4713, 0x03},
    {0x4407, 0x01},
    {0x460B, 0x35},
    {0x460C, 0x22},
    {0x4837, 0x16},
    {0x3824, 0x02},
    {0x5001, 0xA3},
    {0x3503, 0x00},
};

static const struct OV5640_Reg sc_rgb565_cfg[] = {
	/* format control */
	{0x4300, 0x6F}, /* RGB565: {G[2:0],B[4:0]},{R[4:0],G[5:3]} */
    {0x501F, 0x01}, /* Format MUX Control: ISP RGB */
	/* Not in datasheet */
	{0x3612, 0x29},
    {0x3618, 0x00},
    {0x3709, 0x52},
    {0x370C, 0x03},
};

static bool s_ceu_capture_start;
static uint16_t s_ceu_current_fps;
static uint16_t s_ceu_current_length;
static uint16_t s_ceu_current_width;
static CameraDVPErrorCountType s_ceu_err_count;
static CameraRotationBufferType *s_ceu_rotation_buf;
static CameraMIPIErrrorCountType s_mipi_error_count;
static CameraPixelFormatEnum s_ceu_pixel_format;
static CameraResolutionEnum s_ceu_resolution;
static volatile bool s_ceu_capture_ready;
static volatile uint32_t s_ceu_capture_cnt;

static bool s_vin_capture_start;
static uint8_t *s_vin_ready_buf = NULL;
static uint16_t s_vin_current_fps;
static uint16_t s_vin_current_length;
static uint16_t s_vin_current_width;
static CameraResolutionEnum s_vin_resolution;
static volatile bool s_vin_capture_ready;
static volatile uint32_t s_vin_capture_cnt;

#if CAMERA_EN_DVP
static ceu_capture_format_t s_ceu_capture_format;
/* FSP runtime config */
static ceu_instance_ctrl_t *s_ceu_inst_ctrl = NULL;
static capture_cfg_t s_ceu_capture_cfg;
static ceu_extended_cfg_t s_ceu_extend_cfg;
#endif

#if CAMERA_EN_MIPI
/* FSP runtime config */
static vin_instance_ctrl_t *s_vin_inst_ctrl = NULL;
static capture_cfg_t s_vin_capture_cfg;
static vin_extended_cfg_t s_vin_extend_cfg;
static mipi_csi_instance_t s_mipi_csi_inst;
static mipi_csi_instance_ctrl_t s_mipi_csi_inst_ctrl;
static mipi_csi_cfg_t s_mipi_csi_cfg;
#endif


/*================================== Private Functions Prototypes =================================*/

static bool calcSysclk(const struct OV5640_ClockConfig *clock_cfg, uint32_t xclk, uint32_t *vco, uint32_t *sclk);
static void hardReset(CameraInterfaceEnum interface);
static void pinConfig(CameraInterfaceEnum interface);
static bool readClockConfig(struct OV5640_ClockConfig *clock_cfg);
static bool selectDVPClock(uint32_t target_sclk, struct OV5640_ClockConfig *clock_cfg);
static bool selectMIPIClock(uint32_t target_sclk, struct OV5640_ClockConfig *clock_cfg, uint32_t *p_sclk_min, uint32_t *p_sclk_max);
static uint32_t softReset(void);

#if CAMERA_EN_DVP
static void ceuCallback(capture_callback_args_t *p_args);
#endif

#if CAMERA_EN_MIPI
static void mipiCSICallback(mipi_csi_callback_args_t *p_args);
static void vinCallback(capture_callback_args_t *p_args);
#endif

/*================================== Public Functions =============================================*/

/**
 * @brief	查询某一个缓冲块是否处于空闲
 * @param	interface 接口类型
 * @param	buf       缓冲块指针
 * @param	idle      返回结果
 * @return	0: 成功
 * @note	对 DVP 接口，idle 永远设置为 false
 */
uint32_t OV5640_BufferIsIdle(CameraInterfaceEnum interface, uint8_t *buf, bool *idle)
{
	if (interface == CAMERA_INTERFACE_DVP) {
		*idle = false;
	}
	if (interface == CAMERA_INTERFACE_MIPI_CSI) {
#if CAMERA_EN_MIPI
		capture_status_t status;

		R_VIN_StatusGet(&s_vin_inst_ctrl, &status);

		if ((uint8_t *)status.p_buffer == buf) {
			*idle = false;
		}
		else {
			*idle = true;
		}
#else
	return FSP_ERR_UNSUPPORTED;
#endif
	}

	return 0;
}

/**
 * @brief	获取缓冲块数量
 * @param	interface 接口类型
 * @param	num       返回数量
 * @return	总是返回 0
 */
uint32_t OV5640_GetBufferNum(CameraInterfaceEnum interface, uint8_t *num)
{
	if (interface == CAMERA_INTERFACE_DVP) {
		*num = 1;
	}
	else {
	#if CAMERA_EN_MIPI
		*num = 1;
		if (s_vin_extend_cfg.output_ctrl.use_runtime_buffer == false) {
			if (s_vin_extend_cfg.output_ctrl.image_buffer[1] != s_vin_extend_cfg.output_ctrl.image_buffer[0]) {
				*num += 1;
			}
			if ((s_vin_extend_cfg.output_ctrl.image_buffer[2] != s_vin_extend_cfg.output_ctrl.image_buffer[0]) &&
				(s_vin_extend_cfg.output_ctrl.image_buffer[2] != s_vin_extend_cfg.output_ctrl.image_buffer[1])) {
				*num += 1;
			}
		}
	#endif
	}

	return 0;
}

/**
 * @brief	根据当前系统时钟和行、帧总长度计算理论帧率，并输出日志。
 * @param	interface 摄像头接口，用于查询对应的系统时钟。
 * @param	fps       理论帧率输出，单位为帧/秒；仅在成功时更新。
 * @return	成功返回 FSP_SUCCESS，否则返回参数、寄存器配置或 SCCB 读取错误。
 */
uint32_t OV5640_GetCalFPS(CameraInterfaceEnum interface, float *fps)
{
	uint8_t val;
	uint32_t err;
	uint32_t sysclk;
	uint32_t hts;
	uint32_t vts;

	IS_INVALID_POINT(fps);
	err = OV5640_GetSysclk(interface, &sysclk);
	UNLIKE_RETURN(err, FSP_SUCCESS, "GetSysclk failed: %" PRIu32, err);

	IIC_READ_CHECK(0x380C, &val);
	hts = (uint32_t)(val & 0x1FU) << 8;
	IIC_READ_CHECK(0x380D, &val);
	hts |= val;
	IIC_READ_CHECK(0x380E, &val);
	vts = (uint32_t)val << 8;
	IIC_READ_CHECK(0x380F, &val);
	vts |= val;

	if ((sysclk == 0U) || (hts == 0U) || (vts == 0U)) {
		OV5640_LOGE("Invalid timing: SYSCLK=%" PRIu32 " Hz, HTS=%" PRIu32 ", VTS=%" PRIu32, sysclk, hts, vts);
		return FSP_ERR_INVALID_DATA;
	}

	*fps = (float)sysclk / ((float)hts * (float)vts);
	OV5640_LOGI("SYSCLK=%" PRIu32 " Hz, HTS=%" PRIu32 ", VTS=%" PRIu32 ", theoretical FPS=%.3f", sysclk, hts, vts, *fps);

	return FSP_SUCCESS;
}

uint32_t OV5640_GetCaptueCnt(CameraInterfaceEnum interface, uint32_t *cnt)
{
	IS_INVALID_POINT(cnt);

	if (interface == CAMERA_INTERFACE_DVP) {
		*cnt = s_ceu_capture_cnt;
	}
	else {
		*cnt = s_vin_capture_cnt;
	}

	return 0;
}

uint32_t OV5640_GetDVPErrorCnt(CameraDVPErrorCountType *err_cnt)
{
	IS_INVALID_POINT(err_cnt);

	memcpy(err_cnt, &s_ceu_err_count, sizeof(s_ceu_err_count));

	return 0;
}

uint32_t OV5640_GetID(uint16_t *id)
{
	uint8_t val;
	fsp_err_t err;

	IIC_READ_CHECK(0x300A, &val);
	*id = (uint16_t)(((uint16_t)val) << 8);
	IIC_READ_CHECK(0x300B, &val);
	*id |= val;

	return err;
}

uint32_t OV5640_GetMIPIErrorCnt(CameraMIPIErrrorCountType *err_cnt)
{
	IS_INVALID_POINT(err_cnt);

	memcpy(err_cnt, &s_mipi_error_count, sizeof(s_mipi_error_count));

	return 0;
}

/**
 * @brief	获取 OV5640 实际的输出分辨率
 * @param	length 长度
 * @param	width  宽度
 * @return	FSP_SUCCESS: 成功；其它为读写失败 */
uint32_t OV5640_GetOutputResolution(uint16_t *length, uint16_t *width)
{
	uint8_t reg3808;
    uint8_t reg3809;
    uint8_t reg380A;
    uint8_t reg380B;
	fsp_err_t err;

	IS_INVALID_POINT(length);
	IS_INVALID_POINT(width);

	IIC_READ_CHECK(0x3808, &reg3808);
	IIC_READ_CHECK(0x3809, &reg3809);
	IIC_READ_CHECK(0x380A, &reg380A);
	IIC_READ_CHECK(0x380B, &reg380B);

	*length = (uint16_t)((reg3808 & 0x0F) << 8) | reg3809;
	*width = (uint16_t)((reg380A & 0x07) << 8) | reg380B;

	return err;
}

/**
 * @brief	获取完成扫描场的尺寸
 * @param	hts 水平方向扫描长度
 * @param	vts 垂直方向扫描长度
 * @return	FSP_SUCCESS: 成功；其它为读写失败 */
uint32_t OV5640_GetOutputSize(uint16_t *hts, uint16_t *vts)
{
	uint8_t reg380C;
	uint8_t reg380D;
	uint8_t reg380E;
	uint8_t reg380F;
	uint32_t err;

	IS_INVALID_POINT(hts);
	IS_INVALID_POINT(vts);

	IIC_READ_CHECK(0x380C, &reg380C);
	IIC_READ_CHECK(0x380D, &reg380D);
	IIC_READ_CHECK(0x380E, &reg380E);
	IIC_READ_CHECK(0x380F, &reg380F);
	*hts = (uint16_t)((reg380C & 0x1F) << 8) | reg380D;
	*vts = (uint16_t)((reg380E << 8) | reg380F);

	return err;
}

uint32_t OV5640_GetResolution(CameraInterfaceEnum interface, uint16_t *length, uint16_t *width, CameraResolutionEnum *res)
{
	if (interface == CAMERA_INTERFACE_DVP) {
		if (length != NULL) {
			*length = s_ceu_current_length;
		}
		if (width != NULL) {
			*width = s_ceu_current_width;
		}
		if (res != NULL) {
			*res = s_ceu_resolution;
		}
	}
	else {
		if (length != NULL) {
			*length = s_vin_current_length;
		}
		if (width != NULL) {
			*width = s_vin_current_width;
		}
		if (res != NULL) {
			*res = s_vin_resolution;
		}
	}

	return 0;
}

/**
 * @brief 	根据当前寄存器配置计算系统时钟。
 * @param 	interface 摄像头接口，用于选择对应的外部输入时钟。
 * @param 	clk       系统时钟输出，单位为 Hz；仅在成功时更新。
 * @return 	成功返回 FSP_SUCCESS，否则返回参数、寄存器配置或 SCCB 读取错误。
 */
uint32_t OV5640_GetSysclk(CameraInterfaceEnum interface, uint32_t *clk)
{
	uint8_t val;
	uint32_t err;
	uint32_t xclk;
	uint32_t multiplier;
	uint32_t pre_div;
	uint32_t sys_div;
	uint32_t pll_rdiv;
	uint32_t bit_div2x;
	uint32_t sclk_rdiv;
	uint32_t vco;
	uint32_t remainder;
	uint32_t sysclk;

	IS_INVALID_POINT(clk);
	if (interface == CAMERA_INTERFACE_DVP) {
		xclk = OV5640_DVP_XCLK_HZ;
	}
	else if (interface == CAMERA_INTERFACE_MIPI_CSI) {
		xclk = OV5640_MIPI_XCLK_HZ;
	}
	else {
		return FSP_ERR_INVALID_ARGUMENT;
	}

	IIC_READ_CHECK(0x3034, &val);
	bit_div2x = val & 0x0FU;
	if ((bit_div2x != 8U) && (bit_div2x != 10U)) {
		return FSP_ERR_INVALID_DATA;
	}
	bit_div2x /= 2U;

	IIC_READ_CHECK(0x3035, &val);
	sys_div = val >> 4;
	if (sys_div == 0U) {
		sys_div = 16U;
	}

	IIC_READ_CHECK(0x3036, &val);
	multiplier = val;
	IIC_READ_CHECK(0x3037, &val);
	pre_div = val & 0x0FU;
	pll_rdiv = ((val >> 4) & 0x01U) + 1U;
	if ((pre_div == 0U) || (multiplier == 0U)) {
		return FSP_ERR_INVALID_DATA;
	}

	IIC_READ_CHECK(0x3108, &val);
	sclk_rdiv = 1U << (val & 0x03U);

	/* 等价于 xclk * multiplier / pre_div，拆分商和余数以避免乘法中间值溢出。 */
	vco = xclk / pre_div;
	remainder = (xclk % pre_div) * multiplier / pre_div;
	if (vco > (UINT32_MAX - remainder) / multiplier) {
		return FSP_ERR_INVALID_DATA;
	}
	vco = vco * multiplier + remainder;

	sysclk = vco / sys_div / pll_rdiv;
	if (sysclk > UINT32_MAX / 2U) {
		return FSP_ERR_INVALID_DATA;
	}
	sysclk = sysclk * 2U / bit_div2x / sclk_rdiv;
	*clk = sysclk;

	return FSP_SUCCESS;
}

uint32_t OV5640_Init(CameraInterfaceEnum interface, CameraRotationBufferType *rb)
{
	uint8_t val;
	uint16_t i;
	uint16_t init_len;
	uint32_t err;
	float fps;

	const struct OV5640_Reg *p_init_table = NULL;

	pinConfig(interface);
	hardReset(interface);
	err = softReset();
	UNLIKE_RETURN(err, 0, "softReset() failed: %" PRIu32, err);
	R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);

	err = OV5640_GetID(&i);
	UNLIKE_RETURN(err, 0, "OV5640_GetID() failed: %" PRIu32, err);
	OV5640_LOGI("ID: 0x%04" PRIX16, i);

	if (interface == CAMERA_INTERFACE_DVP) {
		IIC_WRITE_CHECK(0x3103, 0x11);
		IIC_WRITE_CHECK(0x3008, 0x82);
		R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
		R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
		init_len = (uint16_t)(sizeof(sc_init_table_dvp) / sizeof(sc_init_table_dvp[0]));
		p_init_table = sc_init_table_dvp;
	}
	else {
		init_len = (uint16_t)(sizeof(sc_init_table_mipi) / sizeof(sc_init_table_mipi[0]));
		p_init_table = sc_init_table_mipi;
	}

    OV5640_LOGD("Init table length: %" PRIu32, init_len);
    for (i = 0; i < init_len; i++) {
    	IIC_WRITE_CHECK(p_init_table[i].reg, p_init_table[i].val);
	#if __OV5640_DEBUG
    	IIC_READ_CHECK(p_init_table[i].reg, &val);
    	if (val != p_init_table[i].val) {
    		OV5640_LOGW("REG 0x%" PRIX16 ": W[0x%" PRIX16 "] != R[0x%" PRIX16 "]", p_init_table[i].reg, p_init_table[i].val, val);
    	}
	#endif
    }
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);

    if (interface == CAMERA_INTERFACE_MIPI_CSI) {
		OV5640_SetMIPIChannel(0);
		OV5640_SetExposureLevel(EXPOSURE_LEVEL_8);
		err = OV5640_SetOutputSize(CAMERA_INTERFACE_MIPI_CSI, 1024, 600);
		UNLIKE_RETURN(err, FSP_SUCCESS, "SetOutputSize failed: %" PRIu32, err);
		OV5640_GetCalFPS(interface, &fps);
		s_vin_current_fps = (uint16_t)roundf(fps);
		s_vin_capture_start = false;
		memset(&s_mipi_error_count, 0, sizeof(s_mipi_error_count));
    }
    else {
		OV5640_SetOutputFormat(CAMARA_PIXEL_RGB565);
		OV5640_SetAutoFocus(1);
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
		OV5640_SetExposureLevel(EXPOSURE_LEVEL_8);
		err = OV5640_SetOutputSize(CAMERA_INTERFACE_DVP, 640, 480);
		UNLIKE_RETURN(err, FSP_SUCCESS, "SetOutputSize failed: %" PRIu32, err);
		s_ceu_capture_start = false;
		s_ceu_capture_cnt = 0;
		memset(&s_ceu_err_count, 0, sizeof(s_ceu_err_count));
		OV5640_GetCalFPS(interface, &fps);
		s_ceu_current_fps = (uint16_t)roundf(fps);
		s_ceu_rotation_buf = rb;
    }

	return err;
}

uint32_t OV5640_IsCaptureStart(CameraInterfaceEnum interface, bool *result)
{
	IS_INVALID_POINT(result);

	if (interface == CAMERA_INTERFACE_DVP) {
		*result = s_ceu_capture_start;
	}
	else {
		*result = s_vin_capture_start;
	}

	return 0;
}

uint32_t OV5640_IsImageReady(CameraInterfaceEnum interface, uint8_t **p_image)
{
	if (interface == CAMERA_INTERFACE_DVP) {
		if (s_ceu_capture_ready) {
			s_ceu_capture_ready = false;
			if (p_image != NULL) {
				if (s_ceu_rotation_buf->index == 0) {
					if (s_ceu_rotation_buf->length == 1) {
						*p_image = s_ceu_rotation_buf->buffer[s_ceu_rotation_buf->index];
					}
					else {
						*p_image = s_ceu_rotation_buf->buffer[s_ceu_rotation_buf->length - 1];
					}
				}
				else {
					*p_image = s_ceu_rotation_buf->buffer[s_ceu_rotation_buf->index - 1];
				}
			}
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		if (s_vin_capture_ready) {
			s_vin_capture_ready = false;
			if (p_image != NULL) {
				*p_image = s_vin_ready_buf;
			}
			return 1;
		}
		else {
			return 0;
		}
	}
}

uint32_t OV5640_IsPowerDown(CameraInterfaceEnum interface)
{
	bsp_io_port_pin_t pin;

	if (interface == CAMERA_INTERFACE_DVP) {
		pin = OV5640_PIN_CEU_PWR_ON;
		}
		else {
		pin = OV5640_PIN_VIN_PWR_ON;
		}

	if (R_BSP_PinRead(pin) == BSP_IO_LEVEL_HIGH) {
			return 1;
		}
		else {
			return 0;
		}
	}

uint32_t OV5640_PowerDown(CameraInterfaceEnum interface, uint8_t enable)
{
	uint8_t reg;
	uint32_t err;

	R_BSP_PinAccessEnable();
	if (interface == CAMERA_INTERFACE_DVP) {
		if (enable) {
			R_BSP_PinWrite(OV5640_PIN_CEU_PWR_ON, BSP_IO_LEVEL_HIGH);
		}
		else {
			R_BSP_PinWrite(OV5640_PIN_CEU_PWR_ON, BSP_IO_LEVEL_LOW);
			/* PWDN 后 OV5640 内部时钟、计数器会停止，IO 进入高阻态，需要时间恢复，否则前几帧会读到全黑的图像 */
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}
	else {
		if (enable) {
			IIC_READ_CHECK(0x300E, &reg);
			OV5640_LOGD("Before power down, 0x300E = 0x%04" PRIX32, reg);
			reg |= 0x18;
			IIC_WRITE_CHECK(0x300E, reg);
			R_BSP_PinWrite(OV5640_PIN_VIN_PWR_ON, BSP_IO_LEVEL_HIGH);
		}
		else {
			R_BSP_PinWrite(OV5640_PIN_VIN_PWR_ON, BSP_IO_LEVEL_LOW);
			R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
			IIC_READ_CHECK(0x300E, &reg);
			OV5640_LOGD("After power down, 0x300E = 0x%04" PRIX32, reg);
			reg &= 0xE7;
			IIC_WRITE_CHECK(0x300E, reg);
			/* PWDN 后 OV5640 内部时钟、计数器会停止，IO 进入高阻态，需要时间恢复，否则前几帧会读到全黑的图像 */
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);

			IIC_READ_CHECK(0x3008, &reg);
			OV5640_LOGD("0x3008 = 0x%04" PRIX32, reg);
			IIC_READ_CHECK(0x4202, &reg);
			OV5640_LOGD("0x4202 = 0x%04" PRIX32, reg);
		}
	}
	R_BSP_PinAccessDisable();

	return 0;
}

uint32_t OV5640_PowerOff(CameraInterfaceEnum interface, bool enable)
{
	if (interface == CAMERA_INTERFACE_DVP) {
		if (enable) {
			R_IOPORT_PinWrite(g_ioport.p_ctrl, OV5640_PIN_CEU_RESET, BSP_IO_LEVEL_LOW);
		}
		else {
			R_IOPORT_PinWrite(g_ioport.p_ctrl, OV5640_PIN_CEU_RESET, BSP_IO_LEVEL_HIGH);
		}
	}
	else {
		if (enable) {
			R_IOPORT_PinWrite(g_ioport.p_ctrl, OV5640_PIN_VIN_RESET, BSP_IO_LEVEL_LOW);
		}
		else {
			R_IOPORT_PinWrite(g_ioport.p_ctrl, OV5640_PIN_VIN_RESET, BSP_IO_LEVEL_HIGH);
		}
	}

	return 0;
}

uint32_t OV5640_SetAutoFocus(uint8_t enable)
{
	uint8_t val;
	uint16_t i, firmware_len;
	fsp_err_t err;

	uint16_t addr = OV5640_FIRMWARE_ADDR;
	uint16_t timeout = 5000;

	if (enable) {
		IIC_WRITE_CHECK(0x3000, 0x20);
		firmware_len = (uint16_t)(sizeof(sc_auto_focus_firmware) / sizeof(sc_auto_focus_firmware[0]));
		OV5640_LOGD("Firmware length: %" PRIu32, firmware_len);
		for (i = 0; i < firmware_len; i++) {
			IIC_WRITE_CHECK(addr, sc_auto_focus_firmware[i]);
			addr++;
		}
		IIC_WRITE_CHECK(0x3022, 0x00);
		IIC_WRITE_CHECK(0x3023, 0x00);
		IIC_WRITE_CHECK(0x3024, 0x00);
		IIC_WRITE_CHECK(0x3025, 0x00);
		IIC_WRITE_CHECK(0x3026, 0x00);
		IIC_WRITE_CHECK(0x3027, 0x00);
		IIC_WRITE_CHECK(0x3028, 0x00);
		IIC_WRITE_CHECK(0x3029, 0x7F);
		IIC_WRITE_CHECK(0x3000, 0x00);

		IIC_READ_CHECK(0x3029, &val);
		while (val != 0x70) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			IIC_READ_CHECK(0x3029, &val);
			if (timeout) {
				timeout--;
			}
			else {
				err = FSP_ERR_TIMEOUT;
				break;
			}
		}
	}
	else {
		err = FSP_ERR_UNSUPPORTED;
	}

	return err;
}

uint32_t OV5640_SetBinning(bool en_vertical, bool en_horizon)
{
	bool current;
	uint8_t reg3820;
	uint8_t reg3821;
	uint32_t err;

	IIC_READ_CHECK(0x3820, &reg3820);
	current = (reg3820 & 0x01) ? true : false;
	if (current != en_vertical) {
		if (en_vertical) {
			reg3820 |= 0x01;
		}
		else {
			reg3820 &= 0xFE;
		}
		IIC_WRITE_CHECK(0x3820, reg3820);
	}

	IIC_READ_CHECK(0x3821, &reg3821);
	current = (reg3821 & 0x01) ? true : false;
	if (current != en_horizon) {
		if (en_horizon) {
			reg3821 |= 0x01;
		}
		else {
			reg3821 &= 0xFE;
		}
		IIC_WRITE_CHECK(0x3821, reg3821);
	}

	return err;
}

uint32_t OV5640_SetExposureLevel(OV5640_ExposureLevelEnum level)
{
	fsp_err_t err;

	switch (level) {
	case EXPOSURE_LEVEL_0:
		IIC_WRITE_CHECK(0x3A0F, 0x10);
		IIC_WRITE_CHECK(0x3A10, 0x08);
		IIC_WRITE_CHECK(0x3A1B, 0x10);
		IIC_WRITE_CHECK(0x3A1E, 0x08);
		IIC_WRITE_CHECK(0x3A11, 0x20);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_1:
		IIC_WRITE_CHECK(0x3A0F, 0x18);
		IIC_WRITE_CHECK(0x3A10, 0x10);
		IIC_WRITE_CHECK(0x3A1B, 0x18);
		IIC_WRITE_CHECK(0x3A1E, 0x10);
		IIC_WRITE_CHECK(0x3A11, 0x30);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_2:
		IIC_WRITE_CHECK(0x3A0F, 0x20);
		IIC_WRITE_CHECK(0x3A10, 0x18);
		IIC_WRITE_CHECK(0x3A11, 0x41);
		IIC_WRITE_CHECK(0x3A1B, 0x20);
		IIC_WRITE_CHECK(0x3A1E, 0x18);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_3:
		IIC_WRITE_CHECK(0x3A0F, 0x28);
		IIC_WRITE_CHECK(0x3A10, 0x20);
		IIC_WRITE_CHECK(0x3A11, 0x51);
		IIC_WRITE_CHECK(0x3A1B, 0x28);
		IIC_WRITE_CHECK(0x3A1E, 0x20);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_4:
		IIC_WRITE_CHECK(0x3A0F, 0x30);
		IIC_WRITE_CHECK(0x3A10, 0x28);
		IIC_WRITE_CHECK(0x3A11, 0x61);
		IIC_WRITE_CHECK(0x3A1B, 0x30);
		IIC_WRITE_CHECK(0x3A1E, 0x28);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_5:
		IIC_WRITE_CHECK(0x3A0F, 0x38);
		IIC_WRITE_CHECK(0x3A10, 0x30);
		IIC_WRITE_CHECK(0x3A11, 0x61);
		IIC_WRITE_CHECK(0x3A1B, 0x38);
		IIC_WRITE_CHECK(0x3A1E, 0x30);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_6:
		IIC_WRITE_CHECK(0x3A0F, 0x40);
		IIC_WRITE_CHECK(0x3A10, 0x38);
		IIC_WRITE_CHECK(0x3A11, 0x71);
		IIC_WRITE_CHECK(0x3A1B, 0x40);
		IIC_WRITE_CHECK(0x3A1E, 0x38);
		IIC_WRITE_CHECK(0x3A1F, 0x10);
		break;
	case EXPOSURE_LEVEL_7:
		IIC_WRITE_CHECK(0x3A0F, 0x48);
		IIC_WRITE_CHECK(0x3A10, 0x40);
		IIC_WRITE_CHECK(0x3A11, 0x80);
		IIC_WRITE_CHECK(0x3A1B, 0x48);
		IIC_WRITE_CHECK(0x3A1E, 0x40);
		IIC_WRITE_CHECK(0x3A1F, 0x20);
		break;
	case EXPOSURE_LEVEL_8:
		IIC_WRITE_CHECK(0x3A0F, 0x50);
		IIC_WRITE_CHECK(0x3A10, 0x48);
		IIC_WRITE_CHECK(0x3A11, 0x90);
		IIC_WRITE_CHECK(0x3A1B, 0x50);
		IIC_WRITE_CHECK(0x3A1E, 0x48);
		IIC_WRITE_CHECK(0x3A1F, 0x20);
		break;
	case EXPOSURE_LEVEL_9:
		IIC_WRITE_CHECK(0x3A0F, 0x58);
		IIC_WRITE_CHECK(0x3A10, 0x50);
		IIC_WRITE_CHECK(0x3A11, 0x91);
		IIC_WRITE_CHECK(0x3A1B, 0x58);
		IIC_WRITE_CHECK(0x3A1E, 0x50);
		IIC_WRITE_CHECK(0x3A1F, 0x20);
		break;
	case EXPOSURE_LEVEL_10:
		IIC_WRITE_CHECK(0x3A0F, 0x60);
		IIC_WRITE_CHECK(0x3A10, 0x58);
		IIC_WRITE_CHECK(0x3A11, 0xA0);
		IIC_WRITE_CHECK(0x3A1B, 0x60);
		IIC_WRITE_CHECK(0x3A1E, 0x58);
		IIC_WRITE_CHECK(0x3A1F, 0x20);
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		break;
	}

	return err;
}

/**
 * @brief	DVP/MIPI 通过调整时钟设置帧率，保留分辨率、读出窗口及 HTS/VTS。
 * @param	interface    摄像头接口。
 * @param	target       目标帧率；在对应接口的有效时钟组合中选择最接近的频率。
 * @param	real         返回回读计算的基础理论帧率，四舍五入；允许为 NULL。
 * @param	auto_decrese 保留参数，当前忽略，不修改自动曝光或夜景模式。
 * @return	无法选择时钟返回 FSP_ERR_UNSUPPORTED；MIPI 采集中返回 FSP_ERR_IN_USE；其它失败返回相应错误。
 * @note	时钟搜索失败时不写寄存器；写入后回读校验，配置不一致只告警、不回滚。
 * @note	MIPI 使用 64 位目标计算，成功后更新 s_vin_current_fps；返回基础理论帧率，不保证实测帧率。
 */
uint32_t OV5640_SetFPS(CameraInterfaceEnum interface, uint16_t target, uint16_t *real, bool auto_decrese)
{
	fsp_err_t err = FSP_SUCCESS;

	(void)auto_decrese;

	if (target == 0) {
		return FSP_ERR_UNSUPPORTED;
	}

	if (interface == CAMERA_INTERFACE_DVP) {
		uint16_t real_hts, real_vts;
		uint32_t target_sclk;
		uint32_t sclk;
		uint32_t vco;
		uint32_t valid_fps;
		struct OV5640_ClockConfig clock_cfg;
		struct OV5640_ClockConfig readback;

		err = OV5640_GetOutputSize(&real_hts, &real_vts);
		UNLIKE_RETURN(err, 0, "GetOutputSize failed: %" PRIu32, err);
		target_sclk = (uint32_t)target * real_hts * real_vts;
		OV5640_LOGD("Target SCLK: %" PRIu32 " Hz", target_sclk);

		if (target_sclk > CAMERA_DVP_FREQ_LIMIT) {
			OV5640_LOGW("Target SCLK bigger than LIMIT(%" PRIu32 " Hz)", CAMERA_DVP_FREQ_LIMIT);
			valid_fps = (uint16_t)(CAMERA_DVP_FREQ_LIMIT / (uint32_t)(real_hts * real_vts));
			OV5640_LOGD("Maximum achievable frame rate: %" PRIu32, valid_fps);
			target_sclk = valid_fps * real_hts * real_vts;
		}

		if (selectDVPClock(target_sclk, &clock_cfg) == false) {
			OV5640_LOGE("Can't get an available SCLK");
			return FSP_ERR_UNSUPPORTED;
		}

		/* clock_cfg 由 selectDVPClock() 读回后只改分频字段，无关位保持芯片原值。 */
		IIC_WRITE_CHECK(0x3035, clock_cfg.reg3035);
		IIC_WRITE_CHECK(0x3036, clock_cfg.reg3036);
		IIC_WRITE_CHECK(0x3037, clock_cfg.reg3037);
		IIC_WRITE_CHECK(0x3108, clock_cfg.reg3108);

		/* 回读校验：理论帧率以芯片实际持有的分频配置为准。 */
		if (readClockConfig(&readback) == false) {
			OV5640_LOGE("Read back clock registers failed");
			return FSP_ERR_INVALID_DATA;
		}
		if (calcSysclk(&readback, OV5640_DVP_XCLK_HZ, &vco, &sclk) == false) {
			OV5640_LOGE("Read back clock configuration is invalid");
			return FSP_ERR_INVALID_DATA;
		}
		if ((readback.reg3035 != clock_cfg.reg3035) || (readback.reg3036 != clock_cfg.reg3036) ||
			(readback.reg3037 != clock_cfg.reg3037) || (readback.reg3108 != clock_cfg.reg3108)) {
			OV5640_LOGW("Clock registers differ from the requested configuration");
		}

		OV5640_LOGD("Real SCLK: %" PRIu32 " Hz, VCO: %" PRIu32 " Hz", sclk, vco);
		if (real != NULL) {
			*real = (uint16_t)roundf((float)sclk / ((float)real_hts * (float)real_vts));
		}
	}
	else if (interface == CAMERA_INTERFACE_MIPI_CSI) {
		uint16_t real_hts, real_vts;
		uint32_t new_target_fps;
		uint32_t frame_length;
		uint32_t real_fps;
		uint32_t sclk;
		uint32_t sclk_max, sclk_min;
		uint32_t vco;
		uint64_t target_sclk;
		struct OV5640_ClockConfig clock_cfg;
		struct OV5640_ClockConfig readback;

		if (s_vin_capture_start) {
			return FSP_ERR_IN_USE;
		}
		err = OV5640_GetOutputSize(&real_hts, &real_vts);
		UNLIKE_RETURN(err, FSP_SUCCESS, "GetOutputSize failed: %" PRIu32, err);
		if ((real_hts == 0U) || (real_vts == 0U)) {
			OV5640_LOGE("Invalid timing: HTS=%" PRIu16 ", VTS=%" PRIu16, real_hts, real_vts);
			return FSP_ERR_INVALID_DATA;
		}
		frame_length = (uint32_t)real_hts * real_vts;
		target_sclk = (uint64_t)target * frame_length;
		if (target_sclk > UINT32_MAX) {
			OV5640_LOGE("Target MIPI SCLK exceeds the calculation range");
			return FSP_ERR_UNSUPPORTED;
		}
		if (selectMIPIClock((uint32_t)target_sclk, &clock_cfg, &sclk_min, &sclk_max) == false) {
			OV5640_LOGW("Can't set target FPS: %" PRIu32, target);
			new_target_fps = sclk_max / frame_length;
			target_sclk = (uint64_t)new_target_fps * frame_length;
			if (target_sclk > UINT32_MAX) {
				OV5640_LOGE("Target MIPI SCLK exceeds the calculation range");
				return FSP_ERR_UNSUPPORTED;
			}
			OV5640_LOGI("Set new target FPS: %" PRIu32, new_target_fps);
			if (selectMIPIClock((uint32_t)target_sclk, &clock_cfg, &sclk_min, &sclk_max) == false) {
				OV5640_LOGE("Can't set target SCLK: %" PRIu32, (uint32_t)target_sclk);
				return FSP_ERR_UNSUPPORTED;
			}
		}

		/* 保持 HTS/VTS 及接口格式不变，同时更新与新时钟配套的 MIPI global timing。 */
		IIC_WRITE_CHECK(0x3035, clock_cfg.reg3035);
		IIC_WRITE_CHECK(0x3036, clock_cfg.reg3036);
		IIC_WRITE_CHECK(0x3037, clock_cfg.reg3037);
		IIC_WRITE_CHECK(0x3108, clock_cfg.reg3108);
		IIC_WRITE_CHECK(0x4837, clock_cfg.reg4837);

		if (readClockConfig(&readback) == false) {
			OV5640_LOGE("Read back MIPI clock registers failed");
			return FSP_ERR_INVALID_DATA;
		}
		IIC_READ_CHECK(0x4837, &readback.reg4837);
		if (calcSysclk(&readback, OV5640_MIPI_XCLK_HZ, &vco, &sclk) == false) {
			OV5640_LOGE("Read back MIPI clock configuration is invalid");
			return FSP_ERR_INVALID_DATA;
		}
		if ((readback.reg3034 != clock_cfg.reg3034) || (readback.reg3035 != clock_cfg.reg3035) ||
			(readback.reg3036 != clock_cfg.reg3036) || (readback.reg3037 != clock_cfg.reg3037) ||
			(readback.reg3108 != clock_cfg.reg3108) || (readback.reg4837 != clock_cfg.reg4837)) {
			OV5640_LOGW("MIPI clock registers differ from the requested configuration");
		}

		/* 正数四舍五入，先检查返回类型范围，再同步内部状态和可选输出。 */
		real_fps = (uint32_t)(((uint64_t)sclk + frame_length / 2U) / frame_length);
		if (real_fps > UINT16_MAX) {
			return FSP_ERR_INVALID_DATA;
		}
		s_vin_current_fps = (uint16_t)real_fps;
		if (real != NULL) {
			*real = (uint16_t)real_fps;
		}
		OV5640_LOGD("Real MIPI SCLK: %" PRIu32 " Hz, VCO: %" PRIu32 " Hz, FPS: %" PRIu32, sclk, vco, real_fps);
	}
	else {
		return FSP_ERR_INVALID_ARGUMENT;
	}

	return err;
}

uint32_t OV5640_SetMIPIChannel(uint8_t channel)
{
	uint8_t val;
	fsp_err_t err;

	IIC_READ_CHECK(0x4814, &val);
	val = val & (uint8_t)~(3 << 6);
	val = val | (uint8_t)(channel << 6);
	IIC_WRITE_CHECK(0x4814, val);

	return err;
}

uint32_t OV5640_SetOutputFormat(CameraPixelFormatEnum format)
{
	uint8_t val;
	uint16_t i;
	uint16_t table_len;
	uint32_t err;

	switch (format) {
	case CAMARA_PIXEL_RGB565:
		table_len = (uint16_t)(sizeof(sc_rgb565_cfg) / sizeof(sc_rgb565_cfg[0]));
		OV5640_LOGD("RGB565 reg config table length: %" PRIu32, table_len);
		for (i = 0; i < table_len; i++) {
			IIC_WRITE_CHECK(sc_rgb565_cfg[i].reg, sc_rgb565_cfg[i].val);
		#if __OV5640_DEBUG
			IIC_READ_CHECK(sc_rgb565_cfg[i].reg, &val);
			if (sc_rgb565_cfg[i].val != val) {
				OV5640_LOGW("REG 0x%" PRIX32 ": W[0x%" PRIX32 "] != R[0x%" PRIX32 "]", sc_rgb565_cfg[i].reg, sc_rgb565_cfg[i].val, val);
			}
		#endif
		}
		break;
	case CAMERA_PIXEL_JPEG:
		table_len = (uint16_t)(sizeof(sc_jpeg_cfg) / sizeof(sc_jpeg_cfg[0]));
		OV5640_LOGD("JPEG reg config table length: %" PRIu32, table_len);
		for (i = 0; i < table_len; i++) {
			IIC_WRITE_CHECK(sc_jpeg_cfg[i].reg, sc_jpeg_cfg[i].val);
		#if __OV5640_DEBUG
			IIC_READ_CHECK(sc_jpeg_cfg[i].reg, &val);
			if (sc_jpeg_cfg[i].val != val) {
				OV5640_LOGW("REG 0x%" PRIX32 ": W[0x%" PRIX32 "] != R[0x%" PRIX32 "]", sc_jpeg_cfg[i].reg, sc_jpeg_cfg[i].val, val);
			}
		#endif
		}
		break;
	case CAMERA_PIXEL_YUV422:
	case CAMERA_PIXEL_YUV420:
	default:
		err = FSP_ERR_UNSUPPORTED;
		break;
	}

	if (err == 0) {
		s_ceu_pixel_format = format;
	}

	return err;
}

#if OV5640_OUTPUT_SIZE_METHOD
/**
 * @brief	根据当前读出窗口、抽样及 scale 状态设置输出尺寸和 HTS/VTS。
 * @param	interface 摄像头接口。
 * @param	length    输出水平像素数。
 * @param	width     输出垂直像素数。
 * @return	成功返回 FSP_SUCCESS，否则返回尺寸、配置或 SCCB 错误。
 * @note	需先停止采集。仅写 0x3808~0x380F，保留窗口、偏移、抽样、scale 和时钟配置。
 * @note	scale 开启时允许非等比缩小，各方向最多 32 倍；不支持手动缩放比例模式。
 * @note	scale 关闭时从当前 ISP 偏移裁剪，不自动居中；HTS/VTS 按完整抽样后读出窗口计算。
 * @note	时序余量参考 OpenMV，配置后重新计算理论帧率，不修改时钟补偿。
 */
uint32_t OV5640_SetOutputSize(CameraInterfaceEnum interface, uint16_t length, uint16_t width)
{
	uint8_t regs[0x16];
	uint8_t reg3821, reg5001, reg501E;
	uint16_t x_start, y_start, x_end, y_end;
	uint16_t sensor_length, sensor_width;
	uint16_t readout_length, readout_width;
	uint16_t usable_length, usable_width;
	uint16_t x_offset, y_offset;
	uint8_t x_odd, x_even, y_odd, y_even;
	uint16_t hts, vts;
	uint16_t real_length, real_width;
	uint16_t real_hts, real_vts;
	uint32_t err, i;
	float fps;

	const uint16_t min_hts = (2624U + 252U) / 4U;
	const uint16_t min_vts = (1964U + 24U) / 8U;

	if ((interface != CAMERA_INTERFACE_DVP) && (interface != CAMERA_INTERFACE_MIPI_CSI)) {
		return FSP_ERR_INVALID_ARGUMENT;
	}
	if ((length < 320U) || (width < 240U)) {
		OV5640_LOGE("length or width too small");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length > OV5640_MAX_LENGTH) || (width > OV5640_MAX_WIDTH)) {
		OV5640_LOGE("length or width too big");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length * width % 8) != 0) {
		OV5640_LOGE("size(length * width) not 8-byte aliagnment");
		return FSP_ERR_INVALID_SIZE;
	}
	if (((length & 1U) != 0U) || ((width & 1U) != 0U)) {
		return FSP_ERR_INVALID_SIZE;
	}
	if (((interface == CAMERA_INTERFACE_DVP) && s_ceu_capture_start) ||
		((interface == CAMERA_INTERFACE_MIPI_CSI) && s_vin_capture_start)) {
		return FSP_ERR_IN_USE;
	}

	/* 尺寸相同也重新读取，避免外部修改窗口或抽样后沿用失效的缓存状态。 */
	for (i = 0U; i < sizeof(regs) / sizeof(regs[0]); i++) {
		IIC_READ_CHECK((uint16_t)(0x3800U + i), &regs[i]);
	}
	IIC_READ_CHECK(0x3821, &reg3821);
	IIC_READ_CHECK(0x5001, &reg5001);
	x_start = (uint16_t)((regs[0] & 0x0FU) << 8) | regs[1];
	y_start = (uint16_t)((regs[2] & 0x07U) << 8) | regs[3];
	x_end = (uint16_t)((regs[4] & 0x0FU) << 8) | regs[5];
	y_end = (uint16_t)((regs[6] & 0x07U) << 8) | regs[7];
	x_offset = (uint16_t)((regs[0x10] & 0x0FU) << 8) | regs[0x11];
	y_offset = (uint16_t)((regs[0x12] & 0x07U) << 8) | regs[0x13];
	x_odd = regs[0x14] >> 4;
	x_even = regs[0x14] & 0x0FU;
	y_odd = regs[0x15] >> 4;
	y_even = regs[0x15] & 0x0FU;
	if ((x_end < x_start) || (y_end < y_start) ||
		(x_end >= 2624U) || (y_end >= 1964U) ||
		(x_odd == 0U) || (x_even == 0U) || (y_odd == 0U) || (y_even == 0U)) {
		OV5640_LOGE("Invalid readout window or subsample increments");
		return FSP_ERR_INVALID_DATA;
	}
	sensor_length = x_end - x_start + 1U;
	sensor_width = y_end - y_start + 1U;
	/* 奇偶增量交替，平均步长为 (odd + even) / 2；不足完整周期时保守向下取整。
	 * 0x11 为原尺寸，0x31 为 1/2，水平和垂直独立计算；binning 不重复除一次。 */
	readout_length = (uint16_t)((uint32_t)sensor_length * 2U / (x_odd + x_even));
	readout_width = (uint16_t)((uint32_t)sensor_width * 2U / (y_odd + y_even));
	if ((length > readout_length) || (width > readout_width)) {
		OV5640_LOGE("Requested %ux%u exceeds sampled readout %ux%u",
				(unsigned)length, (unsigned)width, (unsigned)readout_length, (unsigned)readout_width);
		return FSP_ERR_INVALID_SIZE;
	}
	/* 如果开启了缩放(scale) */
	if ((reg5001 & 0x20U) != 0U) {
		/* 手册图 4-3：缩放输入需从抽样后窗口两侧各扣除一次偏移。 */
		if (((uint32_t)x_offset * 2U >= readout_length) || ((uint32_t)y_offset * 2U >= readout_width)) {
			return FSP_ERR_INVALID_SIZE;
		}
		usable_length = readout_length - x_offset * 2U;
		usable_width = readout_width - y_offset * 2U;
		if ((length > usable_length) || (width > usable_width) ||
			((uint32_t)usable_length > (uint32_t)length * 32U) ||
			((uint32_t)usable_width > (uint32_t)width * 32U)) {
			OV5640_LOGE("Cannot scale %ux%u to %ux%u within 1..32x downscale",
					(unsigned)usable_length, (unsigned)usable_width, (unsigned)length, (unsigned)width);
			return FSP_ERR_INVALID_SIZE;
		}
		IIC_READ_CHECK(0x501E, &reg501E);
		if ((reg501E & 0x40U) != 0U) {
			/* 仅修改输出尺寸无法同步手动比例，不能假定硬件会自动重算。 */
			OV5640_LOGE("Manual scale ratio enabled; output-size-only configuration unsupported");
			return FSP_ERR_UNSUPPORTED;
		}
	}
	/* 如果没有开启缩放，则直接裁剪。注意：直接从读取窗口起点进行裁剪，没有相对于读取窗口进行居中 */
	else {
		/* 手册图 4-2：直接从当前偏移截取目标矩形，不改变读出窗口或裁剪起点。 */
		if ((x_offset + length > readout_length) || (y_offset + width > readout_width)) {
			OV5640_LOGE("Crop at (%u,%u), %ux%u exceeds sampled readout %ux%u",
					(unsigned)x_offset, (unsigned)y_offset, (unsigned)length, (unsigned)width,
					(unsigned)readout_length, (unsigned)readout_width);
			return FSP_ERR_INVALID_SIZE;
		}
	}

	/* HTS 以抽样后的读出宽度为基础；JPEG 与 RGB565/YUV 的输出余量按 OpenMV 区分。 */
	hts = readout_length;
	if (((reg3821 & 0x20U) != 0U) ? (length <= 1280U) : (length > 640U)) {
		uint16_t output_hts = length * 2U + 8U;
		if (hts < output_hts) {
			hts = output_hts;
		}
	}
	if (length <= 640U) {
		hts += 160U;
	}
	hts += 252U;
	if (hts < min_hts) {
		hts = min_hts;
	}
	vts = readout_width + 24U;
	if (vts < min_vts) {
		vts = min_vts;
	}

	/* 所有可行性检查通过后，只写输出宽高及 HTS/VTS，保留高字节中的无关位。 */
	regs[8] = (regs[8] & 0xF0U) | ((length >> 8) & 0x0FU);
	regs[9] = (uint8_t)length;
	regs[10] = (regs[10] & 0xF8U) | ((width >> 8) & 0x07U);
	regs[11] = (uint8_t)width;
	regs[12] = (regs[12] & 0xE0U) | ((hts >> 8) & 0x1FU);
	regs[13] = (uint8_t)hts;
	regs[14] = (uint8_t)(vts >> 8);
	regs[15] = (uint8_t)vts;
	for (i = 8U; i <= 15U; i++) {
		IIC_WRITE_CHECK((uint16_t)(0x3800U + i), regs[i]);
	}

	R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
	err = OV5640_GetOutputResolution(&real_length, &real_width);
	UNLIKE_RETURN(err, 0, "GetOutputResolution failed: %" PRIu32, err);
	OV5640_LOGD("Target resolution: %" PRIu32 "x%" PRIu32, length, width);
	OV5640_LOGD("Real resolution  : %" PRIu32 "x%" PRIu32, real_length, real_width);
	if ((real_length != length) || (real_width != width)) {
		return FSP_ERR_INVALID_DATA;
	}

	err = OV5640_GetOutputSize(&real_hts, &real_vts);
	UNLIKE_RETURN(err, 0, "GetOutputSize failed: %" PRIu32, err);
	OV5640_LOGD("Target size: %" PRIu32 "x%" PRIu32, hts, vts);
	OV5640_LOGD("Real size  : %" PRIu32 "x%" PRIu32, real_hts, real_vts);
	if ((real_hts != hts) || (real_vts != vts)) {
		return FSP_ERR_INVALID_DATA;
	}

	err = OV5640_GetCalFPS(interface, &fps);
	UNLIKE_RETURN(err, FSP_SUCCESS, "GetCalFPS failed: %" PRIu32, err);
	if (interface == CAMERA_INTERFACE_DVP) {
		OV5640_LOGD("Old FPS: %" PRIu32 ", New FPS: %.3f", s_ceu_current_fps, fps);
		s_ceu_current_length = real_length;
		s_ceu_current_width = real_width;
		s_ceu_current_fps = (uint16_t)roundf(fps);
		s_ceu_resolution = CAMERA_RESOLUTION_CUSTOM;
	}
	else {
		OV5640_LOGD("Old FPS: %" PRIu32 ", New FPS: %.3f", s_vin_current_fps, fps);
		s_vin_current_length = real_length;
		s_vin_current_width = real_width;
		s_vin_current_fps = (uint16_t)roundf(fps);
		s_vin_resolution = CAMERA_RESOLUTION_CUSTOM;
	}

	return err;
}
#else
/**
 * @brief	设置通用输出尺寸及配套读出时序，保持当前时钟配置。
 * 			参考实现：https://github.com/openmv/openmv/blob/master/drivers/sensors/ov5640.c
 * @param	interface 摄像头接口。
 * @param	length    输出水平像素数。
 * @param	width     输出垂直像素数。
 * @return	成功返回 FSP_SUCCESS，否则返回尺寸、配置或 SCCB 错误。
 * @note	需先停止采集。采用 OpenMV 的默认完整读出窗口、抽样和居中缩放算法。
 * @note	HTS/VTS 随读出模式调整，不补偿帧率；配置后重新计算并记录理论帧率。
 */
uint32_t OV5640_SetOutputSize(CameraInterfaceEnum interface, uint16_t length, uint16_t width)
{
	uint8_t reg3820, reg3821, reg5001;
	uint8_t increment;
	uint16_t sensor_div;
	uint16_t readout_length, readout_width;
	uint16_t scaled_length, scaled_width;
	uint16_t x_offset, y_offset;
	uint16_t hts, vts;
	uint16_t real_length, real_width;
	uint16_t real_hts, real_vts;
	uint32_t err, i;
	float fps;

	/* OpenMV: 有效阵列 2592x1944，读出窗口额外包含 16 列、8 行同步余量。 */
	const uint16_t sensor_length = OV5640_MAX_LENGTH + 16U;
	const uint16_t sensor_width = OV5640_MAX_WIDTH + 8U;
	const uint16_t x_start = 8U;
	const uint16_t y_start = 2U;
	const uint16_t min_hts = (2624U + 252U) / 2U;
	const uint16_t min_vts = (1964U + 24U) / 8U;

	if ((interface != CAMERA_INTERFACE_DVP) && (interface != CAMERA_INTERFACE_MIPI_CSI)) {
		return FSP_ERR_INVALID_ARGUMENT;
	}
	if ((length < 320U) || (width < 240U)) {
		OV5640_LOGE("length or width too small");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length > OV5640_MAX_LENGTH) || (width > OV5640_MAX_WIDTH)) {
		OV5640_LOGE("length or width too big");
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length * width % 8) != 0) {
		OV5640_LOGE("size(length * width) not 8-byte aliagnment");
		return FSP_ERR_INVALID_SIZE;
	}
	if (((length & 1U) != 0U) || ((width & 1U) != 0U)) {
		return FSP_ERR_INVALID_SIZE;
	}
	if (((interface == CAMERA_INTERFACE_DVP) && s_ceu_capture_start) ||
		((interface == CAMERA_INTERFACE_MIPI_CSI) && s_vin_capture_start)) {
		return FSP_ERR_IN_USE;
	}

	/* 不按缓存尺寸提前返回，确保复位或切换预设模式后也重新配置完整读出路径。 */
	sensor_div = ((length > OV5640_MAX_LENGTH / 2U) || (width > OV5640_MAX_WIDTH / 2U)) ? 1U : 2U;
	readout_length = OV5640_MAX_LENGTH / sensor_div;
	readout_width = OV5640_MAX_WIDTH / sensor_div;
	increment = (uint8_t)(((sensor_div * 2U - 1U) << 4) | 1U);

	/* 等价于 OpenMV 的最小缩放比，使用整数运算避免浮点舍入导致窗口越界。 */
	if ((uint32_t)readout_length * width <= (uint32_t)readout_width * length) {
		scaled_length = readout_length;
		scaled_width = (uint16_t)((uint32_t)width * readout_length / length);
	}
	else {
		scaled_length = (uint16_t)((uint32_t)length * readout_width / width);
		scaled_width = readout_width;
	}
	x_offset = (sensor_length / sensor_div - scaled_length) / 2U;
	y_offset = (sensor_width / sensor_div - scaled_width) / 2U;

	IIC_READ_CHECK(0x3820, &reg3820);
	IIC_READ_CHECK(0x3821, &reg3821);
	IIC_READ_CHECK(0x5001, &reg5001);
	/* HTS 以抽样后的读出宽度为基础；JPEG 与 RGB565/YUV 的输出余量按 OpenMV 区分。 */
	hts = sensor_length / sensor_div;
	if (((reg3821 & 0x20U) != 0U) ? (length <= 1280U) : (length > 640U)) {
		uint16_t output_hts = length * 2U + 8U;
		if (hts < output_hts) {
			hts = output_hts;
		}
	}
	if (length <= 640U) {
		hts += 160U;
	}
	hts += 252U;
	if (hts < min_hts) {
		hts = min_hts;
	}
	vts = sensor_width / sensor_div + 24U;
	if (vts < min_vts) {
		vts = min_vts;
	}

	/* 只更新尺寸、读出窗口和时序，不写 PLL、PCLK 分频及曝光控制寄存器。 */
	const uint16_t timing[][2] = {
		{0x3800, x_start},
		{0x3802, y_start},
		{0x3804, x_start + sensor_length - 1U},
		{0x3806, y_start + sensor_width - 1U},
		{0x3808, length},
		{0x380A, width},
		{0x380C, hts},
		{0x380E, vts},
		{0x3810, x_offset},
		{0x3812, y_offset},
		{0x4602, length},
		{0x4604, width},
	};
	for (i = 0U; i < sizeof(timing) / sizeof(timing[0]); i++) {
		IIC_WRITE_CHECK(timing[i][0], (uint8_t)(timing[i][1] >> 8));
		IIC_WRITE_CHECK(timing[i][0] + 1U, (uint8_t)timing[i][1]);
	}
	IIC_WRITE_CHECK(0x3814, increment);
	IIC_WRITE_CHECK(0x3815, increment);
	IIC_WRITE_CHECK(0x3820, (reg3820 & 0xFEU) | (sensor_div > 1U));
	IIC_WRITE_CHECK(0x3821, (reg3821 & 0xFEU) | (sensor_div > 1U));
	/* 通用尺寸依赖 ISP 缩放，保留其余 ISP 功能开关。 */
	IIC_WRITE_CHECK(0x5001, reg5001 | 0x20U);

	R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
	err = OV5640_GetOutputResolution(&real_length, &real_width);
	UNLIKE_RETURN(err, 0, "GetOutputResolution failed: %" PRIu32, err);
	OV5640_LOGD("Target resolution: %" PRIu32 "x%" PRIu32, length, width);
	OV5640_LOGD("Real resolution  : %" PRIu32 "x%" PRIu32, real_length, real_width);
	if ((real_length != length) || (real_width != width)) {
		return FSP_ERR_INVALID_DATA;
	}

	err = OV5640_GetOutputSize(&real_hts, &real_vts);
	UNLIKE_RETURN(err, 0, "GetOutputSize failed: %" PRIu32, err);
	OV5640_LOGD("Target size: %" PRIu32 "x%" PRIu32, hts, vts);
	OV5640_LOGD("Real size  : %" PRIu32 "x%" PRIu32, real_hts, real_vts);
	if ((real_hts != hts) || (real_vts != vts)) {
		return FSP_ERR_INVALID_DATA;
	}

	err = OV5640_GetCalFPS(interface, &fps);
	UNLIKE_RETURN(err, FSP_SUCCESS, "GetCalFPS failed: %" PRIu32, err);
	if (interface == CAMERA_INTERFACE_DVP) {
		OV5640_LOGD("Old FPS: %" PRIu32 ", New FPS: %.3f", s_ceu_current_fps, fps);
		s_ceu_current_length = real_length;
		s_ceu_current_width = real_width;
		s_ceu_current_fps = (uint16_t)roundf(fps);
		s_ceu_resolution = CAMERA_RESOLUTION_CUSTOM;
	}
	else {
		OV5640_LOGD("Old FPS: %" PRIu32 ", New FPS: %.3f", s_vin_current_fps, fps);
		s_vin_current_length = real_length;
		s_vin_current_width = real_width;
		s_vin_current_fps = (uint16_t)roundf(fps);
		s_vin_resolution = CAMERA_RESOLUTION_CUSTOM;
	}

	return err;
}
#endif /* #if OV5640_OUTPUT_SIZE_METHOD */

/**
 * @brief	设置是否开启 ISP scale
 * @param	enable 选项，是否开启
 * @retval	FSP_SUCCESS: 成功
 * 			Other: 读写错误
 * @note	仅修改 0x5001[5]，不会修改其它任何寄存器 */
uint32_t OV5640_SetScale(bool enable)
{
	bool current_scale;
	uint8_t reg5001;
	uint32_t err;

	IIC_READ_CHECK(0x5001, &reg5001);
	current_scale = (reg5001 & 0x20) ? true : false;
	if (current_scale == enable) {
		OV5640_LOGD("Current scale match request, skip");
		return FSP_SUCCESS;
	}

	if (enable) {
		reg5001 |= 0x20;
	}
	else {
		reg5001 &= ~0x20;
	}
	IIC_WRITE_CHECK(0x5001, reg5001);

	return err;
}

/**
 * @brief	设置缩放模式
 * @param	manual 为 true 时，使用手动模式；false 使用自动模式
 * @return	FSP_SUCCESS 表示成功，其它为读写错误
 */
uint32_t OV5640_SetScaleMode(bool manual)
{
	bool current;
	uint8_t reg501e;
	uint32_t err;

	IIC_READ_CHECK(0x501E, &reg501e);
	current = reg501e & 0x40 ? true : false;
	if (current != manual) {
		if (manual) {
			reg501e |= 0x40;
		}
		else {
			reg501e &= 0xBE;
		}
		IIC_WRITE_CHECK(0x501E, reg501e);
	}

	return err;
}

uint32_t OV5640_SetSubsample(uint8_t horizon, uint8_t vertical)
{
	uint8_t reg3814;
	uint8_t reg3815;
	uint32_t err;

	IIC_READ_CHECK(0x3814, &reg3814);
	IIC_READ_CHECK(0x3815, &reg3815);
	reg3814 &= 0x0F;
	reg3814 |= (horizon << 4) & 0xF0;
	reg3815 &= 0x0F;
	reg3815 |= (vertical << 4) & 0xF0;
	IIC_WRITE_CHECK(0x3814, reg3814);
	IIC_WRITE_CHECK(0x3815, reg3815);

	return err;
}

/**
 * @brief	设置传感器读出窗口大小，自动添加同步余量。
 * @param	length 有效窗口水平像素数，不含额外的 16 列同步余量。
 * @param	width  有效窗口垂直像素数，不含额外的 8 行同步余量。
 * @param	center 是否在完整读出区域内居中，起点向下对齐到偶数坐标。
 * @retval	FSP_SUCCESS: 成功
 * @retval	FSP_ERR_INVALID_SIZE: 要求的尺寸不符合范围
 * @retval	Other: 读写错误 */
uint32_t OV5640_SetWindowSize(uint16_t length, uint16_t width, bool center)
{
	uint8_t regs[8];
	uint32_t i;
	uint32_t err;
	uint16_t x_end;
	uint16_t y_end;

	uint16_t x_start = 8;
	uint16_t y_start = 2;

	if ((length > OV5640_MAX_LENGTH) || (width > OV5640_MAX_WIDTH)) {
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length < 320) || (width < 240)) {
		return FSP_ERR_INVALID_SIZE;
	}

	for (i = 0; i < 8; i++) {
		IIC_READ_CHECK((uint16_t)(0x3800 + i), &regs[i]);
	}

	if (center) {
		/* 完整窗口和目标窗口具有相同的同步余量，差值用有效尺寸计算。
		 * 基准起点 (8, 2) 保持不变，两侧余量平分后按 Bayer 相位对齐。 */
		x_start += ((OV5640_MAX_LENGTH - length) / 4U) * 2U;
		y_start += ((OV5640_MAX_WIDTH - width) / 4U) * 2U;
	}
	/* 额外的同步余量 */
	length += 16;
	width += 8;

	x_end = x_start + length - 1;
	y_end = y_start + width - 1;
	regs[0] &= 0xF0;
	regs[0] |= (x_start >> 8) & 0x0F;
	regs[1] = x_start & 0xFF;
	regs[2] &= 0xF8;
	regs[2] |= (y_start >> 8) & 0x07;
	regs[3] = y_start & 0xFF;
	regs[4] &= 0xF0;
	regs[4] |= (x_end >> 8) & 0x0F;
	regs[5] = x_end & 0xFF;
	regs[6] &= 0xF8;
	regs[6] |= (y_end >> 8) & 0x07;
	regs[7] = y_end & 0xFF;

	for (i = 0; i < 8; i++) {
		OV5640_LOGD("REG0x%04" PRIX32 " = 0x%02" PRIX32, 0x3800 + i, regs[i]);
		IIC_WRITE_CHECK((uint16_t)(0x3800 + i), regs[i]);
	}

	return err;
}

uint32_t OV5640_StartCapture(const void *peri, CameraInterfaceEnum interface, uint8_t *image_buffer)
{
	IS_INVALID_POINT(peri);

	uint32_t err = FSP_SUCCESS;

#if CAMERA_EN_DVP
	if (interface == CAMERA_INTERFACE_DVP) {
		ceu_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
		const capture_cfg_t *p_cfg = ((capture_instance_t *)peri)->p_cfg;
		const ceu_extended_cfg_t *p_extend = (const ceu_extended_cfg_t *)p_cfg->p_extend;

		if (image_buffer == NULL) {
			for (int i = 0; i < 3; i++) {
				if (s_ceu_rotation_buf->buffer[i] == NULL) {
					return FSP_ERR_INVALID_POINTER;
				}
			}
		}

		if (s_ceu_inst_ctrl != p_ctrl) {
			memcpy(&s_ceu_extend_cfg, p_extend, sizeof(ceu_extended_cfg_t));
			memcpy(&s_ceu_capture_cfg, p_cfg, sizeof(capture_cfg_t));
			s_ceu_inst_ctrl = p_ctrl;
		}
		s_ceu_capture_start = false;
		s_ceu_capture_ready = false;
		if (s_ceu_inst_ctrl->open) {
			err = R_CEU_Close(s_ceu_inst_ctrl);
			UNLIKE_RETURN(err, 0, "CEU_Close failed: %" PRIu32, err);
			R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
		}
		s_ceu_capture_cfg.x_capture_pixels = s_ceu_current_length;
		s_ceu_capture_cfg.y_capture_pixels = s_ceu_current_width;
		s_ceu_capture_cfg.x_capture_start_pixel = 0;
		s_ceu_capture_cfg.y_capture_start_pixel = 0;
		s_ceu_capture_cfg.bytes_per_pixel = 2;
		s_ceu_capture_cfg.p_callback = ceuCallback;
		s_ceu_capture_cfg.p_context = p_ctrl;
		s_ceu_capture_cfg.p_extend = &s_ceu_extend_cfg;
		s_ceu_extend_cfg.image_area_size = s_ceu_current_length * s_ceu_current_width * 2;
		err = R_CEU_Open(s_ceu_inst_ctrl, &s_ceu_capture_cfg);
		UNLIKE_RETURN(err, 0, "CEU_Open failed: %" PRIu32, err);
		s_ceu_inst_ctrl->p_context = p_ctrl;
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
		s_ceu_capture_format = s_ceu_extend_cfg.capture_format;

		/* 兼容旧的代码 */
		s_ceu_rotation_buf->index = 0;
		if (image_buffer != NULL) {
			s_ceu_rotation_buf->buffer[0] = image_buffer;
			s_ceu_rotation_buf->buffer[1] = NULL;
			s_ceu_rotation_buf->buffer[2] = NULL;
			s_ceu_rotation_buf->index = 0;
			s_ceu_rotation_buf->length = 1;
			err = R_CEU_CaptureStart(s_ceu_inst_ctrl, image_buffer);
		}
		else {
			err = R_CEU_CaptureStart(s_ceu_inst_ctrl, s_ceu_rotation_buf->buffer[0]);
		}
		
		if (err) {
			OV5640_LOGE("CaptureStart failed: %" PRIu32, err);
			s_ceu_capture_start = false;
		}
		s_ceu_capture_start = true;
	}
#endif /* #if CAMERA_EN_DVP */

#if CAMERA_EN_MIPI
	if (interface == CAMERA_INTERFACE_MIPI_CSI) {
		vin_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
		const capture_cfg_t *p_cfg = ((capture_instance_t *)peri)->p_cfg;
		const vin_extended_cfg_t *p_extend = (const vin_extended_cfg_t *)p_cfg->p_extend;
		const mipi_csi_cfg_t *p_mipi_csi_cfg = (const mipi_csi_cfg_t *)p_extend->p_mipi_csi_instance->p_cfg;

		if (p_extend->output_ctrl.use_runtime_buffer) {
			IS_INVALID_POINT(image_buffer);
		}
		else {
			IS_INVALID_POINT(p_extend->output_ctrl.image_buffer[0]);
			IS_INVALID_POINT(p_extend->output_ctrl.image_buffer[1]);
			IS_INVALID_POINT(p_extend->output_ctrl.image_buffer[2]);
		}

		if (s_vin_inst_ctrl != p_ctrl) {
			memcpy(&s_mipi_csi_cfg, p_mipi_csi_cfg, sizeof(s_mipi_csi_cfg));
			memcpy(&s_vin_extend_cfg, p_cfg->p_extend, sizeof(s_vin_extend_cfg));
			memcpy(&s_vin_capture_cfg, p_cfg, sizeof(s_vin_capture_cfg));
			s_vin_inst_ctrl = p_ctrl;
		}

		if (p_ctrl->open) {
			err = R_VIN_Close(p_ctrl);
			UNLIKE_RETURN(err, 0, "VIN Close failed: %" PRIu32, err);
		}
		s_mipi_csi_cfg.p_callback = mipiCSICallback;
		s_mipi_csi_inst.p_api = &g_mipi_csi;
		s_mipi_csi_inst.p_cfg = &s_mipi_csi_cfg;
		s_mipi_csi_inst.p_ctrl = &s_mipi_csi_inst_ctrl;
		s_vin_extend_cfg.p_mipi_csi_instance = &s_mipi_csi_inst;
		s_vin_capture_cfg.p_callback = vinCallback;
		s_vin_capture_cfg.p_extend = &s_vin_extend_cfg;

		IIC_WRITE_CHECK(0x4202, 0x0F);
		err = R_VIN_Open(s_vin_inst_ctrl, &s_vin_capture_cfg);
		UNLIKE_RETURN(err, 0, "VIN Open failed: %" PRIu32, err);
		err = R_VIN_CaptureStart(p_ctrl, image_buffer);
		UNLIKE_RETURN(err, 0, "VIN CaptureStart failed: %" PRIu32, err);
		IIC_WRITE_CHECK(0x3008, 0x02);
		IIC_WRITE_CHECK(0x4202, 0x00);

		s_vin_capture_start = true;
		s_vin_capture_ready = false;
	}
#endif

	return err;
}

uint32_t OV5640_StopCapture(const void *peri, CameraInterfaceEnum interface)
{
	fsp_err_t err = FSP_SUCCESS;

#if CAMERA_EN_DVP
	if (interface == CAMERA_INTERFACE_DVP) {
		ceu_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
		err = R_CEU_Close(p_ctrl);
	}
#endif

#if CAMERA_EN_MIPI
	if (interface == CAMERA_INTERFACE_MIPI_CSI) {
		vin_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
		err = R_VIN_Close(p_ctrl);
	}
#endif

	return err;
}

/*================================== Private Functions ============================================*/

/**
 * @brief	按分频配置计算 PLL VCO 和系统时钟，不访问寄存器。
 * @param	clock_cfg 分频配置，字段对应 REG 0x3034~0x3037、0x3108。
 * @param	xclk      外部输入时钟，单位 Hz。
 * @param	vco       PLL VCO 输出，单位 Hz；仅在成功时更新。
 * @param	sclk      系统时钟输出，单位 Hz；仅在成功时更新。
 * @return	分频参数合法返回 true，否则返回 false，不修改输出。
 * @note	依据数据手册 2.5、表 7-1、表 7-2 和 DVP 应用指南第 56~57 页：
 *			VCO  = XCLK * pll_multiplier / pll_pre_divider
 *			SCLK = VCO * 2 / (pll_rdiv * system_clock_divider * bit_div2x * sclk_rdiv)
 *			其中 pll_rdiv 为 pll_root_divider + 1，system_clock_divider 为 0 时按 16 计算，
 *			bit_div2x 为 4（8-bit mode）或 5（10-bit mode）；即 MIPI bit mode 一级的实际
 *			分频为 2 / bit_div2x，8-bit 时 /2，10-bit 时 /2.5。
 * @note	违反数据手册表 7-1 的寄存器取值限制（预分频不是 1, 2, 3, 4, 6, 8 之一、
 *			倍频超出 4~252 或大于 127 取奇数、MIPI bit mode 非 8/10）或 VCO 超过
 *			OV5640_VCO_MAX_HZ 时返回 false；数据手册给出的各分辨率推荐最大帧率不在此处检查。
 */
static bool calcSysclk(const struct OV5640_ClockConfig *clock_cfg, uint32_t xclk, uint32_t *vco, uint32_t *sclk)
{
	uint32_t pre_div;
	uint32_t pll_mult;
	uint32_t pll_rdiv;
	uint32_t sys_div;
	uint32_t bit_div2x;
	uint32_t sclk_rdiv;
	uint32_t div_all;
	uint64_t vco_cal;

	if ((clock_cfg == NULL) || (vco == NULL) || (sclk == NULL)) {
		return false;
	}
	if (xclk == 0U) {
		return false;
	}

	pre_div = clock_cfg->reg3037_b.pll_pre_divider;
	pll_mult = clock_cfg->reg3036_b.pll_multiplier;
	pll_rdiv = (uint32_t)clock_cfg->reg3037_b.pll_root_divider + 1U;
	sys_div = clock_cfg->reg3035_b.system_clock_divider;
	sclk_rdiv = 1U << clock_cfg->reg3108_b.sclk_root_divider;
	bit_div2x = clock_cfg->reg3034_b.mipi_bit_mode;

	/* 预分频只有 1, 2, 3, 4, 6, 8 六个合法值。 */
	if ((pre_div != 1U) && (pre_div != 2U) && (pre_div != 3U) && (pre_div != 4U) &&
		(pre_div != 6U) && (pre_div != 8U)) {
		return false;
	}
	/* 倍频 4~252，且大于 127 时只能取偶数。 */
	if ((pll_mult < OV5640_PLL_MULT_MIN) || (pll_mult > OV5640_PLL_MULT_MAX) ||
		((pll_mult >= OV5640_PLL_MULT_EVEN_FROM) && ((pll_mult & 1U) != 0U))) {
		return false;
	}
	if (bit_div2x == 8U) {
		bit_div2x = 4U;
	}
	else if (bit_div2x == 10U) {
		bit_div2x = 5U;
	}
	else {
		return false;
	}
	if (sys_div == 0U) {
		sys_div = 16U;
	}

	/* 一次完成实数除法，避免逐级整除的截断误差；中间值用 64 位防溢出。 */
	vco_cal = (uint64_t)xclk * pll_mult / pre_div;
	if (vco_cal > OV5640_VCO_MAX_HZ) {
		return false;
	}

	div_all = pll_rdiv * sys_div * bit_div2x * sclk_rdiv;
	*vco = (uint32_t)vco_cal;
	*sclk = (uint32_t)(vco_cal * 2U / div_all);

	return true;
}

/**
 * @brief	读回时钟相关的 REG 0x3034~0x3037、0x3108，不修改无关字段。
 * @param	clock_cfg 成功时返回读回的寄存器原始值，失败时不修改。
 * @return	全部读取成功返回 true，任一寄存器读取失败返回 false。
 * @note	本函数返回 bool，故不使用会返回错误码的 IIC_READ_CHECK()，避免读取失败被当成成功。
 * @note	reg4837 初始化为 0；仅 MIPI 搜索需要读取并计算该寄存器。
 */
static bool readClockConfig(struct OV5640_ClockConfig *clock_cfg)
{
	static const uint16_t CLOCK_REG[] = {0x3034, 0x3035, 0x3036, 0x3037, 0x3108};

	uint8_t value[sizeof(CLOCK_REG) / sizeof(CLOCK_REG[0])];
	uint32_t i;
	uint32_t err;

	if (clock_cfg == NULL) {
		return false;
	}

	for (i = 0U; i < (uint32_t)(sizeof(CLOCK_REG) / sizeof(CLOCK_REG[0])); i++) {
		err = iicRead(CLOCK_REG[i], &value[i]);
		if (err != FSP_SUCCESS) {
			OV5640_LOGE("Read REG 0x%04" PRIX16 " failed: %" PRIu32, CLOCK_REG[i], err);
			return false;
		}
	}

	clock_cfg->reg3034 = value[0];
	clock_cfg->reg3035 = value[1];
	clock_cfg->reg3036 = value[2];
	clock_cfg->reg3037 = value[3];
	clock_cfg->reg3108 = value[4];
	clock_cfg->reg4837 = 0U;

	return true;
}

/**
 * @brief	搜索最接近目标频率的 DVP 时钟组合，不访问寄存器。
 * 			参考实现：https://github.com/torvalds/linux/blob/master/drivers/media/i2c/ov5640.c
 * @param	target_sclk 目标 SYSCLK，按当前 HTS * VTS * target 计算，单位 Hz。
 * @param	clock_cfg   成功时返回分频配置，无关位保持芯片当前值；实际 SCLK 由 calcSysclk() 算出。
 * @return	找到可用组合返回 true，否则返回 false，此时不修改输出配置。
 * @note	依据数据手册 2.5、表 7-1 和 DVP 应用指南第 56~57 页；参考 Linux 驱动 ov5640.c 的做法
 *			固定预分频（3）、PLL root divider（/2）与 SCLK root divider（/2），只搜索倍频
 *			4~252（大于 127 取偶数，且受 VCO 上限约束）与 system_clock_divider 1~16；
 *			MIPI bit mode 沿用芯片当前值 0x8/0xA。不考虑数据手册给出的各分辨率推荐最大帧率，
 *			也不考虑 CEU 带宽等系统限制。
 * @note	频率误差相同时，优先选择需要改写的寄存器个数最少的一组。
 */
#if OV5640_CLOCK_FIXED_DIV
static bool selectDVPClock(uint32_t target_sclk, struct OV5640_ClockConfig *clock_cfg)
{
	struct OV5640_ClockConfig origin;
	struct OV5640_ClockConfig work;
	struct OV5640_ClockConfig best;
	uint32_t mult_candidate[3];
	uint32_t bit_div2x;
	uint32_t candidate_cnt;
	uint32_t changes;
	uint64_t div_all;
	uint32_t i;
	uint32_t mult;
	uint32_t mult_cap;
	uint32_t mult_center;
	uint32_t mult_ideal;
	uint32_t offset;
	uint32_t sclk_cal;
	uint32_t sys_div;
	uint32_t vco;
	uint32_t best_changes;
	uint32_t best_offset;

	if (clock_cfg == NULL) {
		return false;
	}
	if (readClockConfig(&origin) == false) {
		return false;
	}

	if (origin.reg3034_b.mipi_bit_mode == 8U) {
		bit_div2x = 4U;
	}
	else if (origin.reg3034_b.mipi_bit_mode == 10U) {
		bit_div2x = 5U;
	}
	else {
		OV5640_LOGE("Unexpected mipi_bit_mode: %" PRIu32, origin.reg3034_b.mipi_bit_mode);
		return false;
	}

	/* 预分频、PLL root divider 与 SCLK root divider 固定，其余位保持芯片当前值。 */
	work = origin;
	work.reg3037_b.pll_pre_divider = (uint8_t)OV5640_DVP_PLL_PRE_DIV;
	work.reg3037_b.pll_root_divider = (uint8_t)OV5640_DVP_PLL_ROOT_DIV;
	work.reg3108_b.sclk_root_divider = (uint8_t)OV5640_DVP_SCLK_ROOT_DIV;

	/* 倍频上限取 VCO 上限与手册倍频上限中的较小者。 */
	mult_cap = (uint32_t)((uint64_t)OV5640_VCO_MAX_HZ * OV5640_DVP_PLL_PRE_DIV / OV5640_DVP_XCLK_HZ);
	if (mult_cap > OV5640_PLL_MULT_MAX) {
		mult_cap = OV5640_PLL_MULT_MAX;
	}
	/* 大于 127 的倍频只能取偶数，上限也压到合法值。 */
	if ((mult_cap >= OV5640_PLL_MULT_EVEN_FROM) && ((mult_cap & 1U) != 0U)) {
		mult_cap--;
	}
	if (mult_cap < OV5640_PLL_MULT_MIN) {
		OV5640_LOGE("No legal multiplier for pre_div %" PRIu32, OV5640_DVP_PLL_PRE_DIV);
		return false;
	}

	best_offset = UINT32_MAX;
	best_changes = UINT32_MAX;
	best = work;

	for (sys_div = 1U; sys_div <= 16U; sys_div++) {
		div_all = (uint64_t)OV5640_DVP_PLL_PRE_DIV * (OV5640_DVP_PLL_ROOT_DIV + 1U) * sys_div *
					bit_div2x * (1U << OV5640_DVP_SCLK_ROOT_DIV);

		/* 反解最接近目标的倍频值，再裁剪到可行区间。 */
		mult_ideal = (uint32_t)(((uint64_t)target_sclk * div_all + OV5640_DVP_XCLK_HZ) /
								(2ULL * OV5640_DVP_XCLK_HZ));
		mult_center = (mult_ideal > mult_cap) ? mult_cap : mult_ideal;
		if (mult_center < OV5640_PLL_MULT_MIN) {
			mult_center = OV5640_PLL_MULT_MIN;
		}

		/* 取整与裁剪后最优值必落在中心值相邻处，三个候选即可覆盖。 */
		candidate_cnt = 0U;
		if (mult_center > OV5640_PLL_MULT_MIN) {
			mult_candidate[candidate_cnt] = mult_center - 1U;
			candidate_cnt++;
		}
		mult_candidate[candidate_cnt] = mult_center;
		candidate_cnt++;
		if (mult_center < mult_cap) {
			mult_candidate[candidate_cnt] = mult_center + 1U;
			candidate_cnt++;
		}

		for (i = 0U; i < candidate_cnt; i++) {
			mult = mult_candidate[i];
			work.reg3036_b.pll_multiplier = (uint8_t)mult;
			work.reg3035_b.system_clock_divider = (uint8_t)(sys_div & 0x0FU);

			/* 违反表 7-1 寄存器取值或 VCO 超限的候选在此被丢弃。 */
			if (calcSysclk(&work, OV5640_DVP_XCLK_HZ, &vco, &sclk_cal) == false) {
				continue;
			}

			offset = (sclk_cal > target_sclk) ? (sclk_cal - target_sclk) : (target_sclk - sclk_cal);
			changes = 0U;
			changes += (work.reg3035 != origin.reg3035) ? 1U : 0U;
			changes += (work.reg3036 != origin.reg3036) ? 1U : 0U;
			changes += (work.reg3037 != origin.reg3037) ? 1U : 0U;
			changes += (work.reg3108 != origin.reg3108) ? 1U : 0U;

			if ((offset < best_offset) ||
				((offset == best_offset) && (changes < best_changes))) {
				best_offset = offset;
				best_changes = changes;
				best = work;
			}
		}
	}

	if (best_offset == UINT32_MAX) {
		OV5640_LOGE("Can't get a valid clock for %" PRIu32 " Hz", target_sclk);
		return false;
	}

	/* 用最终配置反算一次，日志与调用方都以它为准。 */
	if (calcSysclk(&best, OV5640_DVP_XCLK_HZ, &vco, &sclk_cal) == false) {
		OV5640_LOGE("Can't calculate SCLK of the result");
		return false;
	}

	*clock_cfg = best;

	OV5640_LOGD("Target SCLK: %" PRIu32 " Hz, real SCLK: %" PRIu32 " Hz, VCO: %" PRIu32 " Hz",
				target_sclk, sclk_cal, vco);
	OV5640_LOGD("mult: %" PRIu32 ", sys_div: %" PRIu32, best.reg3036_b.pll_multiplier,
				(best.reg3035_b.system_clock_divider == 0U) ? 16U : (uint32_t)best.reg3035_b.system_clock_divider);

	return true;
}
#else
static bool selectDVPClock(uint32_t target_sclk, struct OV5640_ClockConfig *clock_cfg)
{
	static const uint8_t PLL_PRE_DIVIDER[] = {1, 2, 3, 4, 6, 8};

	struct OV5640_ClockConfig origin;
	struct OV5640_ClockConfig work;
	struct OV5640_ClockConfig best;
	uint32_t mult_candidate[3];
	uint32_t bit_div2x;
	uint32_t candidate_cnt;
	uint32_t changes;
	uint64_t div_all;
	uint32_t i;
	uint32_t mult;
	uint32_t mult_cap;
	uint32_t mult_center;
	uint32_t mult_ideal;
	uint32_t offset;
	uint32_t pll_rdiv;
	uint32_t pre_div;
	uint32_t pre_div_idx;
	uint32_t sclk_cal;
	uint32_t sclk_rdiv_sel;
	uint32_t sys_div;
	uint32_t vco;
	uint32_t best_changes;
	uint32_t best_offset;

	if (clock_cfg == NULL) {
		return false;
	}
	if (readClockConfig(&origin) == false) {
		return false;
	}

	if (origin.reg3034_b.mipi_bit_mode == 8U) {
		bit_div2x = 4U;
	}
	else if (origin.reg3034_b.mipi_bit_mode == 10U) {
		bit_div2x = 5U;
	}
	else {
		OV5640_LOGE("Unexpected mipi_bit_mode: %" PRIu32, origin.reg3034_b.mipi_bit_mode);
		return false;
	}

	best_offset = UINT32_MAX;
	best_changes = UINT32_MAX;
	best = origin;
	work = origin;

	for (pre_div_idx = 0U; pre_div_idx < (uint32_t)(sizeof(PLL_PRE_DIVIDER) / sizeof(PLL_PRE_DIVIDER[0])); pre_div_idx++) {
		pre_div = PLL_PRE_DIVIDER[pre_div_idx];

		/* 该预分频下倍频的上限取 VCO 上限与手册倍频上限中的较小者。 */
		mult_cap = (uint32_t)((uint64_t)OV5640_VCO_MAX_HZ * pre_div / OV5640_DVP_XCLK_HZ);
		if (mult_cap > OV5640_PLL_MULT_MAX) {
			mult_cap = OV5640_PLL_MULT_MAX;
		}
		/* 大于 127 的倍频只能取偶数，上限也压到合法值。 */
		if ((mult_cap >= OV5640_PLL_MULT_EVEN_FROM) && ((mult_cap & 1U) != 0U)) {
			mult_cap--;
		}
		if (mult_cap < OV5640_PLL_MULT_MIN) {
			continue;
		}

		for (pll_rdiv = 1U; pll_rdiv <= 2U; pll_rdiv++) {
			for (sys_div = 1U; sys_div <= 16U; sys_div++) {
				for (sclk_rdiv_sel = 0U; sclk_rdiv_sel < 4U; sclk_rdiv_sel++) {
					div_all = (uint64_t)pre_div * pll_rdiv * sys_div * bit_div2x * (1U << sclk_rdiv_sel);

					/* 反解最接近目标的倍频值，再裁剪到本组合的可行区间。 */
					mult_ideal = (uint32_t)(((uint64_t)target_sclk * div_all + OV5640_DVP_XCLK_HZ) /
											(2ULL * OV5640_DVP_XCLK_HZ));
					mult_center = (mult_ideal > mult_cap) ? mult_cap : mult_ideal;
					if (mult_center < OV5640_PLL_MULT_MIN) {
						mult_center = OV5640_PLL_MULT_MIN;
					}

					/* 取整与裁剪后最优值必落在中心值相邻处，三个候选即可覆盖。 */
					candidate_cnt = 0U;
					if (mult_center > OV5640_PLL_MULT_MIN) {
						mult_candidate[candidate_cnt] = mult_center - 1U;
						candidate_cnt++;
					}
					mult_candidate[candidate_cnt] = mult_center;
					candidate_cnt++;
					if (mult_center < mult_cap) {
						mult_candidate[candidate_cnt] = mult_center + 1U;
						candidate_cnt++;
					}

					for (i = 0U; i < candidate_cnt; i++) {
						mult = mult_candidate[i];
						work.reg3036_b.pll_multiplier = (uint8_t)mult;
						work.reg3037_b.pll_pre_divider = (uint8_t)pre_div;
						work.reg3037_b.pll_root_divider = (uint8_t)(pll_rdiv - 1U);
						work.reg3035_b.system_clock_divider = (uint8_t)(sys_div & 0x0FU);
						work.reg3108_b.sclk_root_divider = (uint8_t)sclk_rdiv_sel;

						/* 违反表 7-1 寄存器取值或 VCO 超限的候选在此被丢弃。 */
						if (calcSysclk(&work, OV5640_DVP_XCLK_HZ, &vco, &sclk_cal) == false) {
							continue;
						}

						offset = (sclk_cal > target_sclk) ? (sclk_cal - target_sclk) : (target_sclk - sclk_cal);
						changes = 0U;
						changes += (work.reg3035 != origin.reg3035) ? 1U : 0U;
						changes += (work.reg3036 != origin.reg3036) ? 1U : 0U;
						changes += (work.reg3037 != origin.reg3037) ? 1U : 0U;
						changes += (work.reg3108 != origin.reg3108) ? 1U : 0U;

						if ((offset < best_offset) ||
							((offset == best_offset) && (changes < best_changes))) {
							best_offset = offset;
							best_changes = changes;
							best = work;
						}
					}
				}
			}
		}
	}

	if (best_offset == UINT32_MAX) {
		OV5640_LOGE("Can't get a valid clock for %" PRIu32 " Hz", target_sclk);
		return false;
	}

	/* 用最终配置反算一次，日志与调用方都以它为准。 */
	if (calcSysclk(&best, OV5640_DVP_XCLK_HZ, &vco, &sclk_cal) == false) {
		OV5640_LOGE("Can't calculate SCLK of the result");
		return false;
	}

	*clock_cfg = best;

	OV5640_LOGD("Target SCLK: %" PRIu32 " Hz, real SCLK: %" PRIu32 " Hz, VCO: %" PRIu32 " Hz",
				target_sclk, sclk_cal, vco);
	OV5640_LOGD("pre_div: %" PRIu32 ", mult: %" PRIu32 ", pll_rdiv: %" PRIu32 ", sys_div: %" PRIu32 ", sclk_rdiv: %" PRIu32,
				best.reg3037_b.pll_pre_divider, best.reg3036_b.pll_multiplier,
				(uint32_t)best.reg3037_b.pll_root_divider + 1U,
				(best.reg3035_b.system_clock_divider == 0U) ? 16U : (uint32_t)best.reg3035_b.system_clock_divider,
				1U << best.reg3108_b.sclk_root_divider);

	return true;
}
#endif /* #if OV5640_CLOCK_FIXED_DIV */

/**
 * @brief	搜索最接近目标 SCLK 的 MIPI 时钟组合，只读取寄存器，不写入。
 * @param	target_sclk 目标 SCLK，单位 Hz；必须位于全部有效候选的最小值与最大值之间。
 * @param	clock_cfg   成功时返回时钟配置及 REG 0x4837，无关位保持芯片当前值。
 * @return	找到可用组合返回 true；参数、读取或范围检查失败返回 false，且不修改输出配置。
 * @note	输入时钟及固定分频取 OV5640_MIPI_XCLK_HZ、OV5640_MIPI_PLL_PRE_DIV、
 *			OV5640_MIPI_PLL_ROOT_DIV、OV5640_MIPI_SCLK_ROOT_DIV；不受 DVP 的固定分频开关影响。
 * @note	沿用当前 bit mode 和 MIPI scale divider，遍历倍频与 SysDiv 1~16，依次检查
 *			PLL/VCO、每 lane 位速率和 0x4837 可表示范围，再统计实际可达 SCLK 范围。
 *			范围内允许选择最近值；误差相同时，优先选择需要改写的寄存器个数较少的配置。
 * @note	F = VCO / SysDiv，每 lane 位速率 = F / MIPI scale divider；参考 Linux
 *			ov5640_set_mipi_pclk()，sample_rate = F * lane_count / 8，0x4837 = 2e9 / sample_rate。
 *			该时序算法及接收端固定 PHY timing 仍需上板验证；不检查图像负载或 VIN/内存吞吐。
 */
static bool selectMIPIClock(uint32_t target_sclk, struct OV5640_ClockConfig *clock_cfg, uint32_t *p_sclk_min, uint32_t *p_sclk_max)
{
	struct OV5640_ClockConfig origin;
	struct OV5640_ClockConfig work;
	struct OV5640_ClockConfig best;
	uint32_t best_changes = UINT32_MAX;
	uint32_t best_offset = UINT32_MAX;
	uint32_t changes;
	uint32_t lane_div;
	uint32_t mipi_div;
	uint32_t mult;
	uint32_t offset;
	uint32_t sclk;
	uint32_t sclk_max = 0U;
	uint32_t sclk_min = UINT32_MAX;
	uint32_t sys_div;
	uint32_t vco;
	uint64_t pclk_period;
	uint64_t sample_rate;
	bool found = false;

	if ((clock_cfg == NULL) || (target_sclk == 0U)) {
		return false;
	}
	/* 写入位域前检查宏，避免调试时越界的配置被静默截断。预分频合法值由 calcSysclk() 检查。 */
	if (((uint32_t)OV5640_MIPI_PLL_PRE_DIV > 0x0FU) ||
		((uint32_t)OV5640_MIPI_PLL_ROOT_DIV > 1U) ||
		((uint32_t)OV5640_MIPI_SCLK_ROOT_DIV > 3U) ||
		((OV5640_MIPI_LANE_COUNT != 1U) && (OV5640_MIPI_LANE_COUNT != 2U)) ||
		(OV5640_MIPI_LANE_RATE_MIN_BPS == 0U) ||
		(OV5640_MIPI_LANE_RATE_MIN_BPS > OV5640_MIPI_LANE_RATE_MAX_BPS) ||
		(OV5640_PLL_MULT_MIN > OV5640_PLL_MULT_MAX) || (OV5640_PLL_MULT_MAX > UINT8_MAX)) {
		OV5640_LOGE("Invalid MIPI clock macros");
		return false;
	}
	if (readClockConfig(&origin) == false) {
		return false;
	}
	if (iicRead(0x4837, &origin.reg4837) != FSP_SUCCESS) {
		OV5640_LOGE("Read REG 0x4837 failed");
		return false;
	}
	mipi_div = origin.reg3035_b.scale_divider_for_mipi;
	/* 不推测 MIPI scale divider 为 0 时的行为，避免除零或误用 SysDiv 的 /16 编码。 */
	if (mipi_div == 0U) {
		OV5640_LOGE("Invalid MIPI scale divider: 0");
		return false;
	}

	work = origin;
	work.reg3037_b.pll_pre_divider = (uint8_t)(OV5640_MIPI_PLL_PRE_DIV & 0x0FU);
	work.reg3037_b.pll_root_divider = (uint8_t)(OV5640_MIPI_PLL_ROOT_DIV & 0x01U);
	work.reg3108_b.sclk_root_divider = (uint8_t)(OV5640_MIPI_SCLK_ROOT_DIV & 0x03U);
	best = work;

	for (sys_div = 1U; sys_div <= 16U; sys_div++) {
		work.reg3035_b.system_clock_divider = (uint8_t)(sys_div & 0x0FU);
		lane_div = sys_div * mipi_div;
		for (mult = OV5640_PLL_MULT_MIN; mult <= OV5640_PLL_MULT_MAX; mult++) {
			work.reg3036_b.pll_multiplier = (uint8_t)mult;
			if (calcSysclk(&work, OV5640_MIPI_XCLK_HZ, &vco, &sclk) == false) {
				continue;
			}
			/* 用交叉乘法比较位速率，避免整除把略高于上限的候选误判为合法。 */
			if (((uint64_t)vco < (uint64_t)OV5640_MIPI_LANE_RATE_MIN_BPS * lane_div) ||
				((uint64_t)vco > (uint64_t)OV5640_MIPI_LANE_RATE_MAX_BPS * lane_div)) {
				continue;
			}
			sample_rate = (uint64_t)vco * OV5640_MIPI_LANE_COUNT / (sys_div * 8U);
			if (sample_rate == 0U) {
				continue;
			}
			pclk_period = 2000000000ULL / sample_rate;
			if ((pclk_period == 0U) || (pclk_period > UINT8_MAX)) {
				continue;
			}
			work.reg4837 = (uint8_t)pclk_period;

			/* 范围从全部通过筛选的候选计算，随输入时钟、固定分频和限制宏自动变化。 */
			if (sclk < sclk_min) {
				sclk_min = sclk;
			}
			if (sclk > sclk_max) {
				sclk_max = sclk;
			}
			offset = (sclk > target_sclk) ? (sclk - target_sclk) : (target_sclk - sclk);
			changes = 0U;
			changes += (work.reg3035 != origin.reg3035) ? 1U : 0U;
			changes += (work.reg3036 != origin.reg3036) ? 1U : 0U;
			changes += (work.reg3037 != origin.reg3037) ? 1U : 0U;
			changes += (work.reg3108 != origin.reg3108) ? 1U : 0U;
			changes += (work.reg4837 != origin.reg4837) ? 1U : 0U;
			if (!found || (offset < best_offset) || ((offset == best_offset) && (changes < best_changes))) {
				best_offset = offset;
				best_changes = changes;
				best = work;
			}
			found = true;
		}
	}

	if (!found) {
		OV5640_LOGE("No valid MIPI clock configuration");
		if (p_sclk_max != NULL) {
			*p_sclk_max = 0;
		}
		if (p_sclk_min != NULL) {
			*p_sclk_min = 0;
		}
		return false;
	}
	OV5640_LOGD("MIPI SCLK range: %" PRIu32 "~%" PRIu32 " Hz", sclk_min, sclk_max);
	if ((target_sclk < sclk_min) || (target_sclk > sclk_max)) {
		OV5640_LOGE("Target SCLK %" PRIu32 " Hz is outside the MIPI range", target_sclk);
		if (p_sclk_max != NULL) {
			*p_sclk_max = sclk_max;
		}
		if (p_sclk_min != NULL) {
			*p_sclk_min = sclk_min;
		}
		return false;
	}
	if (calcSysclk(&best, OV5640_MIPI_XCLK_HZ, &vco, &sclk) == false) {
		if (p_sclk_max != NULL) {
			*p_sclk_max = sclk_max;
		}
		if (p_sclk_min != NULL) {
			*p_sclk_min = sclk_min;
		}
		return false;
	}
	*clock_cfg = best;
	sys_div = (best.reg3035_b.system_clock_divider == 0U) ? 16U : best.reg3035_b.system_clock_divider;
	OV5640_LOGD("Target SCLK: %" PRIu32 " Hz, real SCLK: %" PRIu32 " Hz, VCO: %" PRIu32 " Hz", target_sclk, sclk, vco);
	OV5640_LOGD("MIPI mult: %" PRIu32 ", sys_div: %" PRIu32 ", lane rate: %" PRIu32 " bps, REG 0x4837: 0x%02X", best.reg3036_b.pll_multiplier, sys_div, vco / sys_div / mipi_div, best.reg4837);

	if (p_sclk_max != NULL) {
		*p_sclk_max = sclk_max;
	}
	if (p_sclk_min != NULL) {
		*p_sclk_min = sclk_min;
	}

	return true;
}

static void hardReset(CameraInterfaceEnum interface)
{
	R_BSP_PinAccessEnable();
	if (interface == CAMERA_INTERFACE_DVP) {
		R_BSP_PinWrite(OV5640_PIN_CEU_RESET, BSP_IO_LEVEL_LOW);
		R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
		R_BSP_PinWrite(OV5640_PIN_CEU_PWR_ON, BSP_IO_LEVEL_LOW);
		R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
		R_BSP_PinWrite(OV5640_PIN_CEU_RESET, BSP_IO_LEVEL_HIGH);
		R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
	}
	else {
		R_BSP_PinWrite(OV5640_PIN_VIN_RESET, BSP_IO_LEVEL_LOW);
		R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
		R_BSP_PinWrite(OV5640_PIN_VIN_RESET, BSP_IO_LEVEL_HIGH);
		R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
	}
	R_BSP_PinAccessDisable();
}

static void pinConfig(CameraInterfaceEnum interface)
{
	ioport_instance_ctrl_t pin_ctrl;
	ioport_pin_cfg_t pin_cfg_data[13];
    ioport_cfg_t pin_cfg;

    if (interface == CAMERA_INTERFACE_DVP) {
    	pin_cfg_data[0].pin = BSP_IO_PORT_04_PIN_14;
		pin_cfg_data[0].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[1].pin = BSP_IO_PORT_04_PIN_00;
		pin_cfg_data[1].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[2].pin = BSP_IO_PORT_04_PIN_01;
		pin_cfg_data[2].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[3].pin = BSP_IO_PORT_04_PIN_05;
		pin_cfg_data[3].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[4].pin = BSP_IO_PORT_04_PIN_06;
		pin_cfg_data[4].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[5].pin = BSP_IO_PORT_07_PIN_00;
		pin_cfg_data[5].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[6].pin = BSP_IO_PORT_07_PIN_01;
		pin_cfg_data[6].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[7].pin = BSP_IO_PORT_07_PIN_02;
		pin_cfg_data[7].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[8].pin = BSP_IO_PORT_07_PIN_03;
		pin_cfg_data[8].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[9].pin = BSP_IO_PORT_04_PIN_15;
		pin_cfg_data[9].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
		pin_cfg_data[10].pin = BSP_IO_PORT_07_PIN_08;
		pin_cfg_data[10].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;

		pin_cfg_data[11].pin = OV5640_PIN_CEU_PWR_ON;
		pin_cfg_data[11].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW;
		pin_cfg_data[12].pin = OV5640_PIN_CEU_RESET;
		pin_cfg_data[12].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW;
		pin_cfg.number_of_pins = 13;
    }
    else {
    	pin_cfg_data[0].pin = OV5640_PIN_VIN_PWR_ON;
		pin_cfg_data[0].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW;
		pin_cfg_data[1].pin = OV5640_PIN_VIN_RESET;
		pin_cfg_data[1].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW;
		pin_cfg.number_of_pins = 2;
    }

    pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&pin_cfg_data;
    pin_cfg.p_extend = NULL;
    R_IOPORT_Open(&pin_ctrl, &pin_cfg);
}

static uint32_t softReset(void)
{
	uint8_t val;
	uint32_t err;

	IIC_READ_CHECK(0x3103, &val);
	val &= ~(0x01 << 1);
	IIC_WRITE_CHECK(0x3103, val);
	IIC_WRITE_CHECK(0x3008, 0x82);
	R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MILLISECONDS);

	return err;
}

#if CAMERA_EN_DVP
static void ceuCallback(capture_callback_args_t *p_args)
{
	uint32_t err;

	bool has_err = false;

	if (p_args->event == CEU_EVENT_NONE) {}
	if (p_args->event & CEU_EVENT_VD) {}
	if (p_args->event & CEU_EVENT_CRAM_OVERFLOW) {
		s_ceu_err_count.overflow++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_HD_MISMATCH) {
		s_ceu_err_count.hd_mismatch++;
		// has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_MISMATCH) {
		s_ceu_err_count.vd_mismatch++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_ERROR) {
		s_ceu_err_count.vd_error++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_FIREWALL) {
		s_ceu_err_count.firewall++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_HD_MISSING) {
		s_ceu_err_count.hd_missing++;
		// has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_MISSING) {
		s_ceu_err_count.vd_missing++;
		has_err = true;
	}

	if ((p_args->event & CEU_EVENT_FRAME_END) &&  (has_err == false)) {
		s_ceu_capture_cnt++;
		s_ceu_capture_ready = true;
		if (s_ceu_capture_format == CEU_CAPTURE_FORMAT_DATA_SYNCHRONOUS) {
			/* FSP 的这个模式只采集一帧就停了，需要再次发起采集 */
			if (s_ceu_capture_start) {
				s_ceu_rotation_buf->index++;
				if (s_ceu_rotation_buf->index >= s_ceu_rotation_buf->length) {
					s_ceu_rotation_buf->index = 0;
				}
				if (s_ceu_rotation_buf->buffer[s_ceu_rotation_buf->index] == NULL) {
					OV5640_LOGE("Buffer rotation error");
					s_ceu_capture_ready = false;
					s_ceu_capture_start = false;
					return;
				}
				err = R_CEU_CaptureStart(p_args->p_context, s_ceu_rotation_buf->buffer[s_ceu_rotation_buf->index]);
				if (err) {
					OV5640_LOGE("CaptureStart failed: %" PRIu32, err);
					s_ceu_capture_ready = false;
					s_ceu_capture_start = false;
				}
			}
		}
	}
}
#endif

#if CAMERA_EN_MIPI
static void mipiCSICallback(mipi_csi_callback_args_t *p_args)
{
	(void)p_args;
}

static void vinCallback(capture_callback_args_t *p_args)
{
	vin_interrupt_status_t status = (vin_interrupt_status_t)p_args->interrupt_status;

	if (p_args->event == VIN_EVENT_NONE) {}
	if (p_args->event & VIN_EVENT_NOTIFY) {
		if (status.bits.fifo_overfow) {
			s_mipi_error_count.fifo_overflow++;
		}
		if (status.bits.frame_complete) {
			s_vin_capture_ready = true;
			s_vin_ready_buf = p_args->p_buffer;
			s_vin_capture_cnt++;
		}
		if (status.bits.preclip_h_err) {
			s_mipi_error_count.preclip_h_err++;
		}
		if (status.bits.preclip_v_err) {
			s_mipi_error_count.preclip_v_err++;
		}
		if (status.bits.axi_err) {
			s_mipi_error_count.axi_err++;
		}
		if (status.bits.resp_overflow) {
			s_mipi_error_count.resp_overflow++;
		}
	}
	if (p_args->event & VIN_EVENT_ERROR) {}
}
#endif /* #if CAMERA_EN_MIPI */

#endif /* #if CAMERA_EN_DVP || CAMERA_EN_MIPI */
