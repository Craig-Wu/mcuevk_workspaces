#include "lcd_st7796u.h"

#if LCD_EN_ST7796U

/*================================== INCLUDES =====================================================*/

#if defined(__ARMCOMPILER_VERSION)
#elif defined(__clang_version__)
#include <byteswap.h>
#else
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "hal_data.h"
#include "utils/util.h"

/*================================== MACROS =======================================================*/

#define ST7796U_INTERFACE_4SPI_NO_READ      0x01
#define ST7796U_INTERFACE_MIPI              0x02
#define ST7796U_INTERFACE_RGB               0x03

#ifndef __ST7796U_DEBUG
#define __ST7796U_DEBUG			            1
#endif

#ifndef __ST7796U_INTERFACE
#define __ST7796U_INTERFACE                 ST7796U_INTERFACE_4SPI_NO_READ
#endif

#ifndef __ST7796U_DEFAULT_TIMEOUT
#define __ST7796U_DEFAULT_TIMEOUT           500
#endif

#define ST7796U_PIN_BACKLIGHT	            BSP_IO_PORT_00_PIN_12
#define ST7796U_PIN_RESET		            BSP_IO_PORT_00_PIN_13
#define ST7796U_X_LENGTH                    222
#define ST7796U_X_OFFSET                    49
#define ST7796U_Y_LENGTH                    480
#define ST7796U_Y_OFFSET                    0

#if __ST7796U_DEBUG
#include "perf_counter/perf_counter.h"
#include "utils/log.h"
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define ST7796U_LOGD(msg, ...)              LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGI(msg, ...)              LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGW(msg, ...)              LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define ST7796U_LOGE(msg, ...)              LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define ST7796U_LOGD(msg, ...)
#define ST7796U_LOGI(msg, ...)
#define ST7796U_LOGW(msg, ...)
#define ST7796U_LOGE(msg, ...)
#endif

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef ST7796U_CACHE_NUM
#define ST7796U_CACHE_NUM			1
#endif

/* Only for SPI interface */
#ifndef ST7796U_EN_CACHE
#define ST7796U_EN_CACHE            1
#endif

#if ST7796U_EN_CACHE
#define ST7796U_CACHE_SIZE          (ST7796U_X_LENGTH * ST7796U_Y_LENGTH * 2)
#endif

#define ST7796U_PIN_SPI_CLK		    BSP_IO_PORT_05_PIN_14
#define ST7796U_PIN_SPI_MOSI	    BSP_IO_PORT_07_PIN_14
#define ST7796U_PIN_SPI_CS          BSP_IO_PORT_05_PIN_15
#define ST7796U_PIN_DCX             BSP_IO_PORT_07_PIN_15
#define SPI_TRANS_QUEUE_LEN			8
/* DTC 单次最多传输 65536 字节，缓存已超过这个范围，必须分块发送，为了像素数据不出错，这个宏必须是偶数 */
#define SPI_TRANS_MAX_BYTES			60000

/*================================== TYPES ========================================================*/

#if LCD_EN_PERI_MIPI
struct mipi_init_table {
	uint8_t size;
	uint8_t buffer[15];
	mipi_cmd_id_t cmd_id;
	mipi_dsi_cmd_flag_t flags;
};
#endif

#if LCD_EN_PERI_SCI_SPI
struct spi_trans_contex {
	union {
		uint16_t val;
		struct {
			uint16_t used : 1;		/* 如果需要传输大于 DTC 上限的数据，再设置这个位 */
			uint16_t wrmemc : 1;	/* 为 1 则先发送 WRMEMC 命令 */
			uint16_t : 14;
		} b;
	};
	uint32_t remain;
	uint8_t *p_data;
	const spi_instance_t *inst;		/* 在初始化时务必给出 SCI_SPI 实例 */
};

/* If cmd == 0xFF, that means delay len ms */
struct spi_init_table {
    uint8_t cmd;
    uint8_t len;
    uint8_t val[16];
};
#endif /* #if LCD_EN_PERI_SCI_SPI */

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[24];

#if LCD_EN_PERI_MIPI
static const struct mipi_init_table sc_mipi_init_table[] = {
	{2, {0x11, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xF0, 0xC3}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xF0, 0x96}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x36, 0x48}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3A, 0x55}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB4, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {4, {0xB6, 0x8A, 0x07, 0x3B}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB7, 0xC6}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {3, {0xB9, 0x02, 0xE0}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {3, {0xC0, 0xC0, 0x64}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC1, 0x1D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC2, 0xA7}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC5, 0x18}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {9, {0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {15, {0xE0, 0xF0, 0x0B, 0x12, 0x09, 0x0A, 0x26, 0x39, 0x54, 0x4E, 0x38, 0x13, 0x13, 0x2E, 0x34}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {15, {0xE1, 0xF0, 0x10, 0x15, 0x0D, 0x0C, 0x07, 0x38, 0x43, 0x4D, 0x3A, 0x16, 0x15, 0x30, 0x35}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xF0, 0x3C}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xF0, 0x69}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x35, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x29, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x21, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {5, {0x2A, 0x00, 0x31, 0x01, 0x0E}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {5, {0x2B, 0x00, 0x00, 0x01, 0xDF}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
};

static volatile bool s_mipi_cmd_tx_done;
#endif

#if LCD_EN_PERI_SCI_SPI

static const struct spi_init_table sc_spi_init_table[] = {
    {0xFF, 120, {0x00}},
    {0x11, 0  , {0x00}},
    {0xFF, 120, {0x00}},
    {0xF0, 1  , {0xC3}},
    {0xF0, 1  , {0x96}},
    {0x36, 1  , {0x48}},
    {0x3A, 1  , {0x55}},
    {0xB4, 1  , {0x01}},
    {0xB6, 3  , {0x8A, 0x07, 0x3B}},
    {0xB7, 1  , {0xC6}},
    {0xB9, 2  , {0x02, 0xE0}},
    {0xC0, 2  , {0xC0, 0x64}},
    {0xC1, 1  , {0x1D}},
    {0xC2, 1  , {0xA7}},
    {0xC5, 1  , {0x18}},
    {0xE8, 8  , {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}},
    {0xE0, 14 , {0xF0, 0x0B, 0x12, 0x09, 0x0A, 0x26, 0x39, 0x54, 0x4E, 0x38, 0x13, 0x13, 0x2E, 0x34}},
    {0xE1, 14 , {0xF0, 0x10, 0x15, 0x0D, 0x0C, 0x07, 0x38, 0x43, 0x4D, 0x3A, 0x16, 0x15, 0x30, 0x35}},
    {0x35, 1  , {0x00}},
    {0x21, 0  , {0x00}},
    {0x2A, 4  , {(ST7796U_Y_OFFSET >> 8) & 0xFF, ST7796U_Y_OFFSET & 0xFF, ((ST7796U_Y_LENGTH + ST7796U_Y_OFFSET - 1) >> 8) & 0xFF, (ST7796U_Y_LENGTH + ST7796U_Y_OFFSET - 1) & 0xFF}},
    {0x2B, 4  , {(ST7796U_X_OFFSET >> 8) & 0xFF, ST7796U_X_OFFSET & 0xFF, ((ST7796U_X_LENGTH + ST7796U_X_OFFSET - 1) >> 8) & 0xFF, (ST7796U_X_LENGTH + ST7796U_X_OFFSET - 1) & 0xFF}},
	{0xF0, 1  , {0x3C}},
    {0xF0, 1  , {0x69}},
	{0x29, 0  , {0x00}},
};

static volatile bool s_spi_transfer_done;
static struct spi_trans_contex s_spi_contex;

#if ST7796U_EN_CACHE
static uint8_t s_cache[ST7796U_CACHE_SIZE];
#endif

#endif /* #if LCD_EN_PERI_SCI_SPI */

/*================================== Private Functions Prototypes =================================*/

static void hardReset(void);
static void pinConfig(LCD_PeriModelEnum model);

#if LCD_EN_PERI_GLCDC
static void dirtyRectClear(LCD_DirtyRectType *rect);
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width);
static void glcdcCallback(display_callback_args_t *p_args);
static uint32_t prepareWriteBuffer(LCD_DeviceType *lcd, bool overwrite_all);
#endif

#if LCD_EN_PERI_MIPI
static void mipiCallback(mipi_dsi_callback_args_t *p_args);
#endif

#if LCD_EN_PERI_SCI_SPI
static uint32_t setCursor(const spi_instance_t *inst, LCD_OrientationEnum orientation, uint16_t x, uint16_t y);
static uint32_t waitTransferDone(const spi_instance_t *inst, uint32_t timeout_ms);
static uint32_t writeMemory(const spi_instance_t *inst, const uint8_t *val, uint32_t len);
static uint32_t writeRegister(const spi_instance_t *inst, uint8_t reg, const uint8_t *val, uint8_t len);
static void spiCallback(spi_callback_args_t *p_args);
#if ST7796U_EN_CACHE == 0
static uint32_t setRenderArea(const spi_instance_t *inst, LCD_OrientationEnum orientation, uint16_t x, uint16_t y, uint16_t x_len, uint16_t y_len);
#endif
#endif /* #if LCD_EN_PERI_SCI_SPI */

#ifndef bswap_16
static uint16_t bswap_16(uint16_t val);
#endif

/*================================== Public Functions =============================================*/

uint32_t LCD_ST7796U_DrawBitmap(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, const uint8_t *bitmap)
{
	uint16_t i;

    uint32_t err = FSP_SUCCESS;

    if ((length == 0) || (width == 0)) {
        return FSP_SUCCESS;
    }
    if (x >= lcd->x_length) {
        ST7796U_LOGW("X [%" PRIu16 "] out of range", x);
        return FSP_ERR_ASSERTION;
    }
    if (y >= lcd->y_length) {
        ST7796U_LOGW("Y [%" PRIu16 "] out of range", y);
        return FSP_ERR_ASSERTION;
    }

    if ((x + length) > lcd->x_length) {
        length = lcd->x_length - x;
    }
    if ((y + width) > lcd->y_length) {
        width = lcd->y_length - y;
    }

#if LCD_EN_PERI_GLCDC
    if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		err = prepareWriteBuffer(lcd, false);
		if (err != FSP_SUCCESS) {
			return err;
		}

    	uint16_t *p16_bg = (uint16_t *)lcd->graphic_writeable;
    	uint16_t *p16_bp = (uint16_t *)bitmap;

    	p16_bg = &p16_bg[y * (lcd->x_length + lcd->x_dummy) + x];
    	for (i = 0; i < width; i++) {
    		memcpy(p16_bg, p16_bp, length * 2);
    		p16_bg = &p16_bg[lcd->x_length + lcd->x_dummy];
    		p16_bp = &p16_bp[length];
    	}
		dirtyRectMerge(&lcd->dirty_current, x, y, length, width);
    }
#endif

#if LCD_EN_PERI_SCI_SPI
    uint16_t j;
    uint16_t src_length = length;
    const uint16_t *p16_bitmap = (const uint16_t *)bitmap;
    if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
	#if ST7796U_EN_CACHE
		uint16_t *p16_cache = (uint16_t *)s_cache;
		uint32_t index = x + y * lcd->x_length;

		for (i = 0; i < width; i++) {
			for (j = 0; j < length; j++) {
				p16_cache[index + j] = p16_bitmap[i * src_length + j] >> 8;
				p16_cache[index + j] |= p16_bitmap[i * src_length + j] << 8;
			}
			index += lcd->x_length;
		}

		setCursor(lcd->peri_1, lcd->orientation, 0, y);
		err = writeMemory(lcd->peri_1, (uint8_t *)&p16_cache[y * lcd->x_length], width * lcd->x_length * 2);
	#else
		uint16_t color;

		const spi_instance_t *p_inst = (const spi_instance_t *)lcd->peri_1;

		setRenderArea(p_inst, lcd->orientation, x, y, length, width);
		writeRegister(p_inst, 0x2C, NULL, 0);
		R_BSP_PinAccessEnable();
		R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
		R_BSP_PinAccessDisable();
		for (i = 0; i < width; i++) {
			for (j = 0; j < length; j++) {
				color = p16_bitmap[i * src_length + j] >> 8;
				color |= p16_bitmap[i * src_length + j] << 8;
				err = p_inst->p_api->write(p_inst->p_ctrl, &color, 2, SPI_BIT_WIDTH_8_BITS);
				UNLIKE_RETURN(err, 0, "write color failed: %" PRIu32, err);
				err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
				UNLIKE_RETURN(err, 0, "Timeout when wait writing color");
			}
		}
	#endif /* #if ST7796U_EN_CACHE */
    }
#endif /* #if LCD_EN_PERI_SCI_SPI */

    return err;
}

uint32_t LCD_ST7796U_DrawPoint(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint32_t color)
{
    uint32_t err = FSP_SUCCESS;

    if ((x >= lcd->x_length) || (y >= lcd->y_length)) {
        return FSP_ERR_ASSERTION;
    }

#if LCD_EN_PERI_GLCDC
    if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		err = prepareWriteBuffer(lcd, false);
		if (err != FSP_SUCCESS) {
			return err;
		}

    	uint16_t *p16_bg = (uint16_t *)lcd->graphic_writeable;
    	p16_bg[y * (lcd->x_length + lcd->x_dummy) + x] = (uint16_t)color;
		dirtyRectMerge(&lcd->dirty_current, x, y, 1, 1);
    }
#endif

#if LCD_EN_PERI_SCI_SPI
    if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
    #if ST7796U_EN_CACHE
        uint16_t *p16_cache = (uint16_t *)s_cache;
        uint32_t index = x + y * lcd->x_length;

        setCursor(lcd->peri_1, lcd->orientation, x, y);
        p16_cache[index] = bswap_16((uint16_t)color);
        err = writeMemory(lcd->peri_1, (uint8_t *)&p16_cache[index], 2);
    #else
        uint16_t color_16bit;

        setCursor(lcd->peri_1, lcd->orientation, x, y);
        color_16bit = bswap_16((uint16_t)color);
        err = writeMemory(lcd->peri_1, (uint8_t *)&color_16bit, 2);
    #endif /* #if ST7796U_EN_CACHE */
    }
#endif /* #if LCD_EN_PERI_SCI_SPI */

    return err;
}

uint32_t LCD_ST7796U_Fill(LCD_DeviceType *lcd, uint32_t color)
{
	uint32_t i;

	uint32_t err = FSP_SUCCESS;

#if LCD_EN_PERI_GLCDC
    if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		uint16_t *p16_bg;
		uint16_t total_length = lcd->x_length + lcd->x_dummy;
		uint16_t total_width = lcd->y_length + lcd->y_dummy;
		uint32_t total = total_length * total_width;

		/* 整帧填充会覆盖所有旧像素，无需先同步上一帧脏矩形。 */
		err = prepareWriteBuffer(lcd, true);
		if (err != FSP_SUCCESS) {
			return err;
		}

		p16_bg = (uint16_t *)lcd->graphic_writeable;
    	for (i = 0; i < total; i++) {
    		p16_bg[i] = (uint16_t)color;
    	}
		dirtyRectMerge(&lcd->dirty_current, 0, 0, total_length, total_width);
    }
#endif

#if LCD_EN_PERI_SCI_SPI
    uint8_t wcache[4];
    uint16_t row_start, row_end;
    uint16_t column_start, column_end;

    if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
		const spi_instance_t *p_inst = (const spi_instance_t *)lcd->peri_1;
	#if ST7796U_EN_CACHE
		uint16_t *p_color = NULL;
		uint16_t *p_cache = (uint16_t *)s_cache;
	#endif

		if ((lcd->orientation == LCD_ORIENTATION_HORIZONTAL) || (lcd->orientation == LCD_ORIENTATION_HORIZONTAL_FLIP)) {
			row_start = ST7796U_X_OFFSET;
			row_end = lcd->y_length + ST7796U_X_OFFSET;
			column_start = ST7796U_Y_OFFSET;
			column_end = lcd->x_length + ST7796U_Y_OFFSET;
		}
		else {
			row_start = ST7796U_Y_OFFSET;
			row_end = lcd->y_length + ST7796U_Y_OFFSET;
			column_start = ST7796U_X_OFFSET;
			column_end = lcd->x_length + ST7796U_X_OFFSET;
		}

		err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
		wcache[0] = (row_start >> 8) & 0xFF;
		wcache[1] = row_start & 0xFF;
		wcache[2] = ((row_end - 1) >> 8) & 0xFF;
		wcache[3] = (row_end - 1) & 0xFF;
		err = writeRegister(p_inst, 0x2B, wcache, 4);
		UNLIKE_RETURN(err, 0, "Write RASET failed: %" PRIu32, err);
		err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Timeout when wait writing RASET");

		wcache[0] = (column_start >> 8) & 0xFF;
		wcache[1] = column_start & 0xFF;
		wcache[2] = ((column_end - 1) >> 8) & 0xFF;
		wcache[3] = (column_end - 1) & 0xFF;
		err = writeRegister(p_inst, 0x2A, wcache, 4);
		UNLIKE_RETURN(err, 0, "Write CASET failed: %" PRIu32, err);
		err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Timeout when wait writing CASET");

		wcache[0] = (color >> 8) & 0xFF;
		wcache[1] = color & 0xFF;
		R_BSP_PinAccessEnable();
		R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
		R_BSP_PinAccessDisable();
	#if ST7796U_EN_CACHE
		p_color = (uint16_t *)wcache;
		for (i = 0; i < (ST7796U_CACHE_SIZE / 2); i++) {
			p_cache[i] = *p_color;
		}
		err = writeMemory(p_inst, s_cache, ST7796U_CACHE_SIZE);
		UNLIKE_RETURN(err, 0, "Write memory failed: %" PRIu32, err);
	#else
		err = setRenderArea(lcd->peri_1, lcd->orientation, 0, 0, lcd->x_length, lcd->y_length);
		UNLIKE_RETURN(err, 0, "Set render area failed: %" PRIu32, err);
		err = writeRegister(p_inst, 0x2C, NULL, 0);
		UNLIKE_RETURN(err, 0, "Write RAMWR failed: %" PRIu32, err);
		err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Timeout when wait writing RAMWR");
		R_BSP_PinAccessEnable();
		R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
		for (i = 0; i < (lcd->x_length * lcd->y_length); i++) {
			s_spi_transfer_done = false;
			err = p_inst->p_api->write(p_inst->p_ctrl, wcache, 2, SPI_BIT_WIDTH_8_BITS);
			UNLIKE_RETURN(err, 0, "Write memory failed: %" PRIu32, err);
			err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
			UNLIKE_RETURN(err, 0, "Timeout when wait writing color");
		}
		R_BSP_PinAccessDisable();
	#endif
    }

#endif /* #if LCD_EN_PERI_SCI_SPI */

    return err;
}

uint32_t LCD_ST7796U_FillRectangle(LCD_DeviceType *lcd, uint16_t x, uint16_t y, uint16_t length, uint16_t width, uint32_t color)
{
	uint32_t err = FSP_SUCCESS;

    if ((length == 0) || (width == 0)) {
        return FSP_SUCCESS;
    }
    if ((x >= lcd->x_length) || (y >= lcd->y_length)) {
        return FSP_ERR_ASSERTION;
    }

    if ((x + length) > lcd->x_length) {
        length = lcd->x_length - x;
    }
    if ((y + width) > lcd->y_length) {
        width = lcd->y_length - y;
    }

#if LCD_EN_PERI_GLCDC
    if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
    	uint32_t i, j;
		uint16_t *p16_bg;

		err = prepareWriteBuffer(lcd, false);
		if (err != FSP_SUCCESS) {
			return err;
		}

		p16_bg = (uint16_t *)lcd->graphic_writeable;
    	p16_bg = &p16_bg[y * (lcd->x_length + lcd->x_dummy)];
    	for (i = 0; i < width; i++) {
    		for (j = x; j < (x + length); j++) {
    			p16_bg[j] = (uint16_t)color;
    		}
    		p16_bg = &p16_bg[lcd->x_length + lcd->x_dummy];
    	}
		dirtyRectMerge(&lcd->dirty_current, x, y, length, width);
    }
#endif

#if LCD_EN_PERI_SCI_SPI
    if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
    #if ST7796U_EN_CACHE
        uint16_t _color;
        uint32_t i, j, index;

        uint16_t row_num = 0;
        uint16_t *p16_cache = (uint16_t *)s_cache;

        _color = bswap_16((uint16_t)color);
        index = x + y * lcd->x_length;
        for (i = 0; i < width; i++) {
            for (j = 0; j < length; j++) {
                p16_cache[index + j] = _color;
            }
            index += lcd->x_length;
            row_num++;
        }
        setCursor(lcd->peri_1, lcd->orientation, 0, y);
        err = writeMemory(lcd->peri_1, (uint8_t *)&p16_cache[y * lcd->x_length], row_num * lcd->x_length * 2);
        UNLIKE_RETURN(err, 0, "Write memory failed: %" PRIu32, err);
    #else
        uint32_t i;

        uint16_t _color = bswap_16((uint16_t)color);
        const spi_instance_t *p_inst = (const spi_instance_t *)lcd->peri_1;

        setRenderArea(p_inst, lcd->orientation, x, y, length, width);
        UNLIKE_RETURN(err, 0, "Set render area failed: %" PRIu32, err);
        err = writeRegister(p_inst, 0x2C, NULL, 0);
		UNLIKE_RETURN(err, 0, "Write RAMWR failed: %" PRIu32, err);
		err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Timeout when wait writing RAMWR");
		R_BSP_PinAccessEnable();
		R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
        for (i = 0; i < (length * width); i++) {
        	s_spi_transfer_done = false;
			err = p_inst->p_api->write(p_inst->p_ctrl, &_color, 2, SPI_BIT_WIDTH_8_BITS);
			UNLIKE_RETURN(err, 0, "Write memory failed: %" PRIu32, err);
			err = waitTransferDone(p_inst, __ST7796U_DEFAULT_TIMEOUT);
			UNLIKE_RETURN(err, 0, "Timeout when wait writing color");
        }
        R_BSP_PinAccessDisable();
    #endif
    }
#endif

    return err;
}

uint32_t LCD_ST7796U_Init(LCD_DeviceType *lcd)
{
    uint32_t err;
    size_t i, init_table_len;

#if LCD_EN_PERI_GLCDC
	dirtyRectClear(&lcd->dirty_current);
	dirtyRectClear(&lcd->dirty_submitted);
	lcd->present_pending = false;
#endif

    /* SPI interface */
    if ((lcd->peri_model_1 == LCD_PERI_SCI_SPI) && (lcd->peri_model_2 == LCD_PERI_SCI_SPI)) {
    	pinConfig(LCD_PERI_SCI_SPI);
    }
    /* RGB interface */
    else if ((lcd->peri_model_1 == LCD_PERI_GLCDC) && (lcd->peri_model_2 == LCD_PERI_SPI)) {
    	pinConfig(LCD_PERI_GLCDC);
    }
    /* MIPI interface */
    else {
    	pinConfig(LCD_PERI_MIPI);
    }
    hardReset();

#if LCD_EN_PERI_MIPI
    if (lcd->peri_model_2 == LCD_PERI_MIPI) {
    	mipi_dsi_cmd_t msg;
    	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
    	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
    	const mipi_dsi_instance_t *p_mipi = (const mipi_dsi_instance_t *)lcd->peri_2;
    	mipi_dsi_instance_ctrl_t *p_mipi_ctrl = (mipi_dsi_instance_ctrl_t *)p_mipi->p_ctrl;
    	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
    		ST7796U_LOGI("GLCDC already open");
    		p_glcdc->p_api->close(p_glcdc_ctrl);
    	}
    	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
    	UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
    	p_glcdc_ctrl->p_callback = glcdcCallback;
    	p_glcdc_ctrl->p_context = lcd;
    	p_mipi_ctrl->p_callback = mipiCallback;
    	p_mipi_ctrl->p_context = lcd;
    	msg.channel = 0;
    	init_table_len = sizeof(sc_mipi_init_table) / sizeof(sc_mipi_init_table[0]);
    	for (i = 0; i < init_table_len; i++) {
    		msg.cmd_id = sc_mipi_init_table[i].cmd_id;
    		msg.flags = sc_mipi_init_table[i].flags;
    		msg.p_tx_buffer = sc_mipi_init_table[i].buffer;
    		msg.tx_len = sc_mipi_init_table[i].size;
    		s_mipi_cmd_tx_done = false;
    		err = p_mipi->p_api->command(p_mipi->p_ctrl, &msg);
    		UNLIKE_RETURN(err, 0, "MIPI Command Error: %" PRIu32, err);
    		while (s_mipi_cmd_tx_done == false) {
    		#ifdef __OPTIMIZE__
				__nop();
			#endif
    		}
    	}
    	R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
    	p_glcdc->p_api->start(p_glcdc_ctrl);

    	lcd->x_dummy = 2;
    	lcd->y_dummy = 0;
	#if ST7796U_CACHE_NUM == 1
    	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
    	lcd->graphic_mem_2 = NULL;
    	lcd->graphic_writeable = lcd->graphic_mem_1;
	#elif ST7796U_CACHE_NUM == 2
    	lcd->graphic_mem_1 = (uint8_t *)p_glcdc->p_cfg->input[0].p_base;
    	lcd->graphic_mem_2 = &lcd->graphic_mem_1[224 * 480 * 2];
    	lcd->graphic_writeable = lcd->graphic_mem_2;
	#else
    	lcd->graphic_mem_1 = NULL;
    	lcd->graphic_mem_2 = NULL;
    	lcd->graphic_writeable = NULL;
	#endif
    }
#endif

#if LCD_EN_PERI_SCI_SPI
    if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
    	const spi_instance_t *p_inst = (const spi_instance_t *)lcd->peri_1;
    	sci_b_spi_instance_ctrl_t *p_ctrl = (sci_b_spi_instance_ctrl_t *)p_inst->p_ctrl;
    	s_spi_transfer_done = true;
    	s_spi_contex.inst = (const spi_instance_t *)lcd->peri_1;
    	if (p_ctrl->open) {
			ST7796U_LOGI("peri_inst already open");
			p_inst->p_api->close(p_inst->p_ctrl);
		}
		err = p_inst->p_api->open(p_inst->p_ctrl, p_inst->p_cfg);
		UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
		p_inst->p_api->callbackSet(p_inst->p_ctrl, spiCallback, &s_spi_contex, NULL);

		init_table_len = sizeof(sc_spi_init_table) / sizeof(sc_spi_init_table[0]);
		ST7796U_LOGD("init_table_len: %" PRIu32, (uint32_t)init_table_len);
		for (i = 0; i < init_table_len; i++) {
			if (sc_spi_init_table[i].cmd == 0xFF) {
				R_BSP_SoftwareDelay(sc_spi_init_table[i].len, BSP_DELAY_UNITS_MILLISECONDS);
			}
			else {
				err = writeRegister(lcd->peri_1, sc_spi_init_table[i].cmd, sc_spi_init_table[i].val, sc_spi_init_table[i].len);
				UNLIKE_RETURN(err, 0, "Failed when write register. err: %" PRIu32, err);
			}
		}

		lcd->x_dummy = 0;
		lcd->y_dummy = 0;
		lcd->graphic_mem_1 = NULL;
		lcd->graphic_mem_2 = NULL;
    }
#endif

    lcd->x_length = ST7796U_X_LENGTH;
	lcd->y_length = ST7796U_Y_LENGTH;
    lcd->color_format = LCD_COLOR_FORMAT_RGB565;
    lcd->orientation = LCD_ORIENTATION_VERTICAL;

    LCD_ST7796U_Fill(lcd, 0xFFFF);

    uint32_t retry_delay = 10;
    err = LCD_ST7796U_Present(lcd);
    while (err) {
    	R_BSP_SoftwareDelay(retry_delay, BSP_DELAY_UNITS_MILLISECONDS);
    	retry_delay *= 2;
    	err = LCD_ST7796U_Present(lcd);
    }

    return err;
}

/**
 * @brief 查询当前接口操作是否已完成
 * @details 在 SCI SPI 模式下，空闲表示异步 SPI 传输已经完成。
 *          在包含 MIPI 视频模式的 GLCDC 路径下，LCD_Present() 成功返回后进入非空闲状态，
 *          到达下一个帧边界时恢复空闲。双帧缓冲时，这表示最近提交的帧缓冲区已完成锁存，
 *          并且下一个后台缓冲区已经可以写入；单帧缓冲时，仅表示 Present 之后已经到达下一个帧边界，
 *          不表示帧缓冲区未被 GLCDC 读取，也不保证无画面撕裂。即使本函数报告空闲，GLCDC 和 MIPI 视频输出仍会继续扫描。
 * @param[in]  lcd   LCD 设备实例
 * @param[out] idle  接口对应的操作已经完成时设为 true，否则设为 false
 * @retval FSP_SUCCESS       成功返回当前状态
 * @retval FSP_ERR_ASSERTION lcd 或 idle 为 NULL
 */
uint32_t LCD_ST7796U_IsIdle(LCD_DeviceType *lcd, bool *idle)
{
	if ((lcd == NULL) || (idle == NULL)) {
		return FSP_ERR_ASSERTION;
	}

#if LCD_EN_PERI_GLCDC
	if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		*idle = lcd->present_pending ? false : true;
	}
#endif

#if LCD_EN_PERI_SCI_SPI
	if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
		*idle = s_spi_transfer_done ? true : false;
	}
#endif

	return FSP_SUCCESS;
}

uint32_t LCD_ST7796U_Present(LCD_DeviceType *lcd)
{
	if (lcd == NULL) {
		return FSP_ERR_ASSERTION;
	}

#if LCD_EN_PERI_GLCDC
	if (lcd->peri_model_1 == LCD_PERI_GLCDC) {
		uint32_t err;
		uint8_t *submitted = lcd->graphic_writeable;
		const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;

		if (lcd->present_pending) {
			return FSP_ERR_IN_USE;
		}
		if (lcd->graphic_mem_2 == NULL) {
			dirtyRectClear(&lcd->dirty_current);
			dirtyRectClear(&lcd->dirty_submitted);
			__DSB();
			lcd->present_pending = true;

			return FSP_SUCCESS;
		}
		if ((submitted != lcd->graphic_mem_1) && (submitted != lcd->graphic_mem_2)) {
			ST7796U_LOGE("graphic_writeable is not equal to mem_1 or mem_2 !");
			return FSP_ERR_ASSERTION;
		}
		if (lcd->dirty_current.valid == false) {
			/* 避免在没有新画面时切换到尚未同步的后台缓冲。 */
			return FSP_SUCCESS;
		}

		__DSB();
		err = p_glcdc->p_api->bufferChange(p_glcdc->p_ctrl, submitted, DISPLAY_FRAME_LAYER_1);
		if (err != FSP_SUCCESS) {
			if (err == FSP_ERR_INVALID_UPDATE_TIMING) {
				return FSP_ERR_IN_USE;
			}
			else {
				ST7796U_LOGE("Buffer change failed: %" PRIu32, err);
				return err;
			}
		}

		lcd->dirty_submitted = lcd->dirty_current;
		dirtyRectClear(&lcd->dirty_current);
		__DMB();
		lcd->present_pending = true;

		return FSP_SUCCESS;
	}
#endif

#if LCD_EN_PERI_SCI_SPI
	if (lcd->peri_model_1 == LCD_PERI_SCI_SPI) {
		return FSP_SUCCESS;
	}
#endif

	return FSP_ERR_UNSUPPORTED;
}

/*================================== Private Functions ============================================*/

#if LCD_EN_PERI_GLCDC
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

	right = ((uint32_t)rect->x + rect->length) > ((uint32_t)x + length) ?
			(uint32_t)rect->x + rect->length : (uint32_t)x + length;
	bottom = ((uint32_t)rect->y + rect->width) > ((uint32_t)y + width) ?
			 (uint32_t)rect->y + rect->width : (uint32_t)y + width;
	left = rect->x < x ? rect->x : x;
	top = rect->y < y ? rect->y : y;
	rect->x = left;
	rect->y = top;
	rect->length = (uint16_t)(right - rect->x);
	rect->width = (uint16_t)(bottom - rect->y);
}

/**
 * @brief 在绘制前准备可写帧缓冲
 * @details 局部绘制时同步上一已提交帧的脏矩形；整帧覆盖时直接丢弃待同步脏矩形。
 * @param[in,out] lcd LCD 设备实例
 * @param[in] overwrite_all 本次绘制将覆盖整个可写帧缓冲时设为 true
 * @retval FSP_SUCCESS       可写帧缓冲已准备完成
 * @retval FSP_ERR_ASSERTION 可写帧缓冲指针无效
 * @retval FSP_ERR_IN_USE    上一次 Present 尚未在帧边界完成锁存
 */
static uint32_t prepareWriteBuffer(LCD_DeviceType *lcd, bool overwrite_all)
{
	LCD_DirtyRectType *rect = &lcd->dirty_submitted;
	uint16_t *p16_dst;
	uint16_t *p16_src;
	uint16_t stride;
	uint8_t *source;

	if (lcd->present_pending) {
		return FSP_ERR_IN_USE;
	}
	if ((lcd->graphic_mem_2 == NULL) || (rect->valid == false)) {
		return FSP_SUCCESS;
	}
	if (overwrite_all) {
		dirtyRectClear(rect);

		return FSP_SUCCESS;
	}

	if (lcd->graphic_writeable == lcd->graphic_mem_1) {
		source = lcd->graphic_mem_2;
	}
	else if (lcd->graphic_writeable == lcd->graphic_mem_2) {
		source = lcd->graphic_mem_1;
	}
	else {
		ST7796U_LOGE("graphic_writeable is not equal to mem_1 or mem_2 !");
		return FSP_ERR_ASSERTION;
	}

	stride = lcd->x_length + lcd->x_dummy;
	p16_src = (uint16_t *)source + rect->y * stride + rect->x;
	p16_dst = (uint16_t *)lcd->graphic_writeable + rect->y * stride + rect->x;
	for (uint16_t row = 0; row < rect->width; row++) {
		memcpy(p16_dst, p16_src, rect->length * sizeof(uint16_t));
		p16_src = &p16_src[stride];
		p16_dst = &p16_dst[stride];
	}
	dirtyRectClear(rect);

	return FSP_SUCCESS;
}
#endif /* #if LCD_EN_PERI_GLCDC */

static void hardReset(void)
{
    R_BSP_PinAccessEnable();
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(4, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, ST7796U_PIN_RESET, BSP_IO_LEVEL_HIGH);
}

static void pinConfig(LCD_PeriModelEnum model)
{
	s_pin_cfg_data[0].pin = ST7796U_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = ST7796U_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;

    if (model == LCD_PERI_GLCDC) {}
    else if (model == LCD_PERI_MIPI) {
    	s_pin_cfg.number_of_pins = 2;
    }
    else {
		s_pin_cfg_data[2].pin = ST7796U_PIN_DCX;
		s_pin_cfg_data[2].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH | IOPORT_CFG_DRIVE_HIGH;
		s_pin_cfg_data[3].pin = ST7796U_PIN_SPI_CLK;
		s_pin_cfg_data[3].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8;
		s_pin_cfg_data[4].pin = ST7796U_PIN_SPI_MOSI;
		s_pin_cfg_data[4].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8;
		s_pin_cfg_data[5].pin = ST7796U_PIN_SPI_CS;
		s_pin_cfg_data[5].pin_cfg = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_PERIPHERAL_SCI0_2_4_6_8;
		s_pin_cfg.number_of_pins = 5;
    }
    s_pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&s_pin_cfg_data;
	s_pin_cfg.p_extend = NULL;

    R_IOPORT_Open(&s_pin_ctrl, &s_pin_cfg);
}

#if LCD_EN_PERI_GLCDC
static void glcdcCallback(display_callback_args_t *p_args)
{
	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case DISPLAY_EVENT_GR1_UNDERFLOW:
		lcd->gr1_underflow_cnt++;
		break;
	case DISPLAY_EVENT_GR2_UNDERFLOW:
		lcd->gr2_underflow_cnt++;
		break;
	case DISPLAY_EVENT_LINE_DETECTION:
		lcd->vsync_cnt++;
		if (lcd->present_pending) {
			if (lcd->graphic_mem_2 != NULL) {
				if (lcd->graphic_writeable == lcd->graphic_mem_1) {
					lcd->graphic_writeable = lcd->graphic_mem_2;
				}
				else if (lcd->graphic_writeable == lcd->graphic_mem_2) {
					lcd->graphic_writeable = lcd->graphic_mem_1;
				}
			}

			__DMB();
			lcd->present_pending = false;
		}
		break;
	case DISPLAY_EVENT_FRAME_END:
		break;
	}
}
#endif /* #if LCD_EN_PERI_GLCDC */

#if LCD_EN_PERI_MIPI
static void mipiCallback(mipi_dsi_callback_args_t *p_args)
{
	uint32_t status;

	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case MIPI_DSI_EVENT_SEQUENCE_0:
		s_mipi_cmd_tx_done = true;
		break;
	case MIPI_DSI_EVENT_SEQUENCE_1:
		break;
	case MIPI_DSI_EVENT_VIDEO:
		status = (uint32_t)p_args->video_status;
		if (status & MIPI_DSI_VIDEO_STATUS_TIMING_ERROR) {
			lcd->mipi_timing_err_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_UNDERFLOW) {
			lcd->mipi_underflow_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_OVERFLOW) {
			lcd->mipi_overflow_cnt++;
		}
		break;
	case MIPI_DSI_EVENT_RECEIVE:
		break;
	case MIPI_DSI_EVENT_FATAL:
		break;
	case MIPI_DSI_EVENT_PHY:
		break;
	case MIPI_DSI_EVENT_POST_OPEN:
		break;
	case MIPI_DSI_EVENT_PRE_START:
		break;
	}
}
#endif /* #if LCD_EN_PERI_MIPI */

#if LCD_EN_PERI_SCI_SPI
static uint32_t setCursor(const spi_instance_t *inst, LCD_OrientationEnum orientation, uint16_t x, uint16_t y)
{
    uint8_t wcache[2];
    uint32_t err;

    if ((orientation == LCD_ORIENTATION_HORIZONTAL) || (orientation == LCD_ORIENTATION_HORIZONTAL_FLIP)) {
        y += ST7796U_X_OFFSET;
    }
    else {
        x += ST7796U_X_OFFSET;
    }

    wcache[0] = (x >> 8) & 0xFF;
    wcache[1] = x & 0xFF;
    err = writeRegister(inst, 0x2A, wcache, 2);
    UNLIKE_RETURN(err, 0, "Write CASET failed: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing CASET");

    wcache[0] = (y >> 8) & 0xFF;
    wcache[1] = y & 0xFF;
    err = writeRegister(inst, 0x2B, wcache, 2);
    UNLIKE_RETURN(err, 0, "Write RASET failed: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing RASET");

    return err;
}

static uint32_t waitTransferDone(const spi_instance_t *inst, uint32_t timeout_ms)
{
    volatile uint32_t us = timeout_ms * 1000;
    sci_b_spi_instance_ctrl_t *p_ctrl = (sci_b_spi_instance_ctrl_t *)inst->p_ctrl;

    while (us) {
        if (s_spi_transfer_done == false) {
            R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
            us--;
        }
        else {
            break;
        }
    }

    while (us) {
        if ((p_ctrl->p_reg->CCR0 & (R_SCI_B0_CCR0_RE_Msk | R_SCI_B0_CCR0_TE_Msk)) != 0) {
            R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
            us--;
        }
        else {
            return FSP_SUCCESS;
        }
    }

    ST7796U_LOGW("Timeout");

    return FSP_ERR_TIMEOUT;
}

static uint32_t writeMemory(const spi_instance_t *inst, const uint8_t *val, uint32_t len)
{
    uint32_t err;

    uint8_t wcache = 0x2C;
    const spi_cfg_t *p_cfg = (const spi_cfg_t *)inst->p_cfg;

    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait last transmit");
    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_LOW);
    R_BSP_PinAccessDisable();
    s_spi_transfer_done = false;
    err = inst->p_api->write(inst->p_ctrl, &wcache, 1, SPI_BIT_WIDTH_8_BITS);
    UNLIKE_RETURN(err, 0, "Failed when write RAMWR. err: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing RAMWR");

    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
    R_BSP_PinAccessDisable();
    s_spi_transfer_done = false;
    if ((p_cfg->p_transfer_tx == NULL) || (len <= SPI_TRANS_MAX_BYTES)) {
    	s_spi_contex.b.used = 0;
    	err = inst->p_api->write(inst->p_ctrl, val, len, SPI_BIT_WIDTH_8_BITS);
    }
    else {
    	s_spi_contex.p_data = (uint8_t *)val;
    	s_spi_contex.remain = len - SPI_TRANS_MAX_BYTES;
    	s_spi_contex.b.used = 1;
    	s_spi_contex.b.wrmemc = 1;
    	err = inst->p_api->write(inst->p_ctrl, val, SPI_TRANS_MAX_BYTES, SPI_BIT_WIDTH_8_BITS);
    }
    UNLIKE_RETURN(err, 0, "Failed when write val. err: %" PRIu32, err);

    return err;
}

static uint32_t writeRegister(const spi_instance_t *inst, uint8_t reg, const uint8_t *val, uint8_t len)
{
    uint32_t err;

    uint8_t wcache = reg;

    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait last transmit");
    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_LOW);
    R_BSP_PinAccessDisable();
    s_spi_transfer_done = false;
    err = inst->p_api->write(inst->p_ctrl, &wcache, 1, SPI_BIT_WIDTH_8_BITS);
    UNLIKE_RETURN(err, 0, "Failed when write reg. err: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing reg");
    if ((val != NULL) && (len != 0)) {
        R_BSP_PinAccessEnable();
        R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
        R_BSP_PinAccessDisable();
        s_spi_transfer_done = false;
        err = inst->p_api->write(inst->p_ctrl, val, len, SPI_BIT_WIDTH_8_BITS);
        UNLIKE_RETURN(err, 0, "Failed when write val. err: %" PRIu32, err);
    }

    return err;
}

static void spiCallback(spi_callback_args_t *p_args)
{
	static uint8_t wrmemc = 0x3C;

	uint32_t err;

	struct spi_trans_contex *ctx = (struct spi_trans_contex *)p_args->p_context;

    switch (p_args->event) {
    case SPI_EVENT_TRANSFER_COMPLETE:
    	if (ctx->b.used) {
    		if (ctx->b.wrmemc) {
    			R_BSP_PinAccessEnable();
    			R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_LOW);
    			R_BSP_PinAccessEnable();
    			err = ctx->inst->p_api->write(ctx->inst->p_ctrl, &wrmemc, 1, SPI_BIT_WIDTH_8_BITS);
    			if (err) {
    				ST7796U_LOGE("Write wrmemc failed: %" PRIu32, err);
    				ctx->b.used = 0;
    				s_spi_transfer_done = true;
    			}
    			else {
    				ctx->b.wrmemc = 0;
    			}
    		}
    		else {
    			R_BSP_PinAccessEnable();
    			R_BSP_PinWrite(ST7796U_PIN_DCX, BSP_IO_LEVEL_HIGH);
    			R_BSP_PinAccessEnable();
    			if (ctx->remain <= SPI_TRANS_MAX_BYTES) {
    				err = ctx->inst->p_api->write(ctx->inst->p_ctrl, &ctx->p_data[SPI_TRANS_MAX_BYTES], ctx->remain, SPI_BIT_WIDTH_8_BITS);
    				ctx->remain = 0;
    				ctx->b.used = 0;
    			}
    			else {
    				err = ctx->inst->p_api->write(ctx->inst->p_ctrl, &ctx->p_data[SPI_TRANS_MAX_BYTES], SPI_TRANS_MAX_BYTES, SPI_BIT_WIDTH_8_BITS);
    				ctx->remain -= SPI_TRANS_MAX_BYTES;
    				ctx->p_data = &ctx->p_data[SPI_TRANS_MAX_BYTES];
    				ctx->b.wrmemc = 1;
    			}
    			if (err) {
    				ST7796U_LOGE("Write data failed: %" PRIu32, err);
    				ctx->b.used = 0;
    				s_spi_transfer_done = true;
    			}
    		}
    	}
    	else {
    		s_spi_transfer_done = true;
    	}
        break;
    case SPI_EVENT_TRANSFER_ABORTED:
        ST7796U_LOGW("Aborted");
        break;
    case SPI_EVENT_ERR_MODE_FAULT:
        ST7796U_LOGW("Mode Fault");
        break;
    case SPI_EVENT_ERR_READ_OVERFLOW:
        ST7796U_LOGW("Read Overflow");
        break;
    case SPI_EVENT_ERR_PARITY:
        ST7796U_LOGW("Parity");
        break;
    case SPI_EVENT_ERR_OVERRUN:
        ST7796U_LOGW("Overrun");
        break;
    case SPI_EVENT_ERR_FRAMING:
        ST7796U_LOGW("Framing");
        break;
    case SPI_EVENT_ERR_MODE_UNDERRUN:
        ST7796U_LOGW("Mode Underrun");
        break;
    default:
        ST7796U_LOGW("Uncheck event: %d", p_args->event);
        break;
    }
}

#if ST7796U_EN_CACHE == 0
static uint32_t setRenderArea(const spi_instance_t *inst, LCD_OrientationEnum orientation, uint16_t x, uint16_t y, uint16_t x_len, uint16_t y_len)
{
    uint8_t wcache[4];
    uint32_t err;

    if ((orientation == LCD_ORIENTATION_HORIZONTAL) || (orientation == LCD_ORIENTATION_HORIZONTAL_FLIP)) {
        y += ST7796U_X_OFFSET;
    }
    else {
        x += ST7796U_X_OFFSET;
    }

    wcache[0] = (x >> 8) & 0xFF;
    wcache[1] = x & 0xFF;
    wcache[2] = ((x + x_len - 1) >> 8) & 0xFF;
    wcache[3] = (x + x_len - 1) & 0xFF;
    err = writeRegister(inst, 0x2A, wcache, 4);
    UNLIKE_RETURN(err, 0, "Write CASET failed: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing CASET");

    wcache[0] = (y >> 8) & 0xFF;
    wcache[1] = y & 0xFF;
    wcache[2] = ((y + y_len - 1) >> 8) & 0xFF;
    wcache[3] = (y + y_len - 1) & 0xFF;
    err = writeRegister(inst, 0x2B, wcache, 4);
    UNLIKE_RETURN(err, 0, "Write RASET failed: %" PRIu32, err);
    err = waitTransferDone(inst, __ST7796U_DEFAULT_TIMEOUT);
    UNLIKE_RETURN(err, 0, "Timeout when wait writing RASET");

    return err;
}
#endif

#endif /* #if LCD_EN_PERI_SCI_SPI */

#ifndef bswap_16
static uint16_t bswap_16(uint16_t val)
{
    uint16_t _val;

    _val = (val >> 8) & 0xFF;
    _val |= (val << 8) & 0xFF00;

    return _val;
}
#endif

#endif /* #if LCD_EN_ST7796U */
