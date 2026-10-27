#include "lcd_ek79007ad2.h"

#if LCD_EN_EK79007AD2

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "hal_data.h"

/*================================== MACROS =======================================================*/

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef EK79007AD2_LAYER_1_CACHE_NUM
#define EK79007AD2_LAYER_1_CACHE_NUM		1
#endif

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 2->Framebuffer->Number of framebuffers.
 * Set to 0 only when Graphics Layer 2 is also disabled in FSP. */
#ifndef EK79007AD2_LAYER_2_CACHE_NUM
#define EK79007AD2_LAYER_2_CACHE_NUM		1
#endif

#define EK79007AD2_PIN_BACKLIGHT			BSP_IO_PORT_00_PIN_12
#define EK79007AD2_PIN_RESET				BSP_IO_PORT_00_PIN_13

#define MIPI_CMD_ID_DELAY					((mipi_cmd_id_t)0xFE)

#ifndef __EK79007AD2_DEBUG
#define __EK79007AD2_DEBUG	1
#endif

#if (EK79007AD2_LAYER_1_CACHE_NUM != 1) && (EK79007AD2_LAYER_1_CACHE_NUM != 2)
#error "EK79007AD2_LAYER_1_CACHE_NUM must be 1 or 2"
#endif

#if (EK79007AD2_LAYER_2_CACHE_NUM < 0) || (EK79007AD2_LAYER_2_CACHE_NUM > 2)
#error "EK79007AD2_LAYER_2_CACHE_NUM must be 0, 1 or 2"
#endif

#if __EK79007AD2_DEBUG
#include "utils/log.h"
#define EK79007AD2_LOGD(msg, ...)           LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGE(msg, ...)   		LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGI(msg, ...)           LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79007AD2_LOGW(msg, ...)           LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#else
#define EK79007AD2_LOGD(msg, ...)
#define EK79007AD2_LOGE(msg, ...)
#define EK79007AD2_LOGI(msg, ...)
#define EK79007AD2_LOGW(msg, ...)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#endif

/*================================== TYPES ========================================================*/

struct mipi_init_table {
	uint8_t size;
	uint8_t buffer[10];
	mipi_cmd_id_t cmd_id;
	mipi_dsi_cmd_flag_t flags;
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[2];
static display_runtime_cfg_t s_layer1_input_cfg;
static display_runtime_cfg_t s_layer2_input_cfg;

static const struct mipi_init_table sc_mipi_init_table[] = {
	{2, {0xB2, 0x10}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x80, 0xAC}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x81, 0xB8}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x82, 0x09}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x83, 0x78}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x84, 0x7F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x85, 0xBB}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
	{2, {0x86, 0x70}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER}
};

static volatile bool s_mipi_cmd_tx_done;

/*================================== Private Functions Prototypes =================================*/

static void glcdcCallback(display_callback_args_t *p_args);
static void mipiCallback(mipi_dsi_callback_args_t *p_args);
static uint32_t pinConfig(void);

/*================================== Public Functions =============================================*/

/**
 * @brief	初始化 EK79007AD2、GLCDC 和 MIPI，绘图及提交使用 lcd.c 的默认实现。
 * @param	lcd LCD 实例，peri_1 为 GLCDC，peri_2 为 MIPI DSI。
 * @return	成功返回 FSP_SUCCESS，失败返回参数、配置或底层驱动错误码。
 */
uint32_t LCD_EK79007AD2_Init(LCD_DeviceType *lcd)
{
	uint32_t err;
	uint32_t close_err;
	uint32_t frame_bytes;
	size_t i, init_table_len;
	mipi_dsi_cmd_t msg;

	if (lcd == NULL) {
		return FSP_ERR_ASSERTION;
	}
	if ((lcd->peri_model_1 != LCD_PERI_GLCDC) || (lcd->peri_model_2 != LCD_PERI_MIPI)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if ((lcd->peri_1 == NULL) || (lcd->peri_2 == NULL)) {
		return FSP_ERR_ASSERTION;
	}

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	if ((p_glcdc->p_ctrl == NULL) || (p_glcdc->p_cfg == NULL) || (p_glcdc->p_api == NULL)) {
		return FSP_ERR_ASSERTION;
	}
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
	const display_input_cfg_t *p_input = &p_glcdc->p_cfg->input[0];
	const mipi_dsi_instance_t *p_mipi = (const mipi_dsi_instance_t *)lcd->peri_2;
	if ((p_mipi->p_ctrl == NULL) || (p_mipi->p_cfg == NULL) || (p_mipi->p_api == NULL)) {
		return FSP_ERR_ASSERTION;
	}
	mipi_dsi_instance_ctrl_t *p_mipi_ctrl = (mipi_dsi_instance_ctrl_t *)p_mipi->p_ctrl;

	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		return FSP_ERR_ASSERTION;
	}

	err = pinConfig();
	UNLIKE_RETURN(err, FSP_SUCCESS, "Pin config failed: %" PRIu32, err);
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79007AD2_PIN_RESET, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(EK79007AD2_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinWrite(EK79007AD2_PIN_BACKLIGHT, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		EK79007AD2_LOGI("GLCDC already open");
		err = p_glcdc->p_api->close(p_glcdc_ctrl);
		UNLIKE_RETURN(err, FSP_SUCCESS, "Close failed: %" PRIu32, err);
	}
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;
	lcd->mipi_timing_err_cnt = 0;
	lcd->mipi_underflow_cnt = 0;
	lcd->mipi_overflow_cnt = 0;
	p_mipi_ctrl->p_callback = mipiCallback;
	p_mipi_ctrl->p_context = lcd;

	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	msg.channel = 0;
	msg.p_rx_buffer = NULL;
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
		R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	}

	lcd->orientation = LCD_ORIENTATION_HORIZONTAL;
	lcd->vsync_cnt = 0;
	lcd->gr1_underflow_cnt = 0;
	lcd->gr2_underflow_cnt = 0;
	memset(&lcd->layer_1, 0, sizeof(lcd->layer_1));
	memset(&lcd->layer_2, 0, sizeof(lcd->layer_2));

	lcd->layer_1.index = 1;
	lcd->layer_1.x_length = p_input->hsize;
	lcd->layer_1.x_dummy = (uint16_t)(p_input->hstride - p_input->hsize);
	lcd->layer_1.y_length = p_input->vsize;
	lcd->layer_1.y_dummy = 0;

	switch (p_input->format) {
	case DISPLAY_IN_FORMAT_32BITS_ARGB8888:
		lcd->layer_1.bytes_per_pixel = 4;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB8888;
		break;
	case DISPLAY_IN_FORMAT_32BITS_RGB888:
		lcd->layer_1.bytes_per_pixel = 4;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_RGB888;
		break;
	case DISPLAY_IN_FORMAT_16BITS_RGB565:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_RGB565;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB1555:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB1555;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB4444:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB4444;
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		goto CLOSE_GLCDC;
	}

	frame_bytes = p_input->hstride * p_input->vsize * lcd->layer_1.bytes_per_pixel;
	lcd->layer_1.graphic_mem_1 = (uint8_t *)p_input->p_base;
	lcd->layer_1.mem1_read_only = false;
#if EK79007AD2_LAYER_1_CACHE_NUM == 1
	lcd->layer_1.graphic_mem_2 = NULL;
	lcd->layer_1.mem2_read_only = true;
	lcd->layer_1.graphic_writeable = lcd->layer_1.graphic_mem_1;
#else
	lcd->layer_1.graphic_mem_2 = &lcd->layer_1.graphic_mem_1[frame_bytes];
	lcd->layer_1.mem2_read_only = false;
	lcd->layer_1.graphic_writeable = lcd->layer_1.graphic_mem_2;
#endif
	lcd->layer_1.graphic_displayed = lcd->layer_1.graphic_mem_1;
	lcd->layer_1.graphic_to_present = NULL;
	lcd->layer_1.graphic_submitted = NULL;
	lcd->layer_1.dirty_current.valid = false;
	lcd->layer_1.dirty_submitted.valid = false;
	lcd->layer_1.present_pending = false;
	memset(lcd->layer_1.graphic_mem_1, 0xFF, frame_bytes);
	if (lcd->layer_1.graphic_mem_2 != NULL) {
		memset(lcd->layer_1.graphic_mem_2, 0xFF, frame_bytes);
	}

	/* Layer 2 未配置时保留为空，并清除重复初始化前的状态。 */
	lcd->layer_2.index = 2;
	lcd->layer_2.mem1_read_only = true;
	lcd->layer_2.mem2_read_only = true;
#if EK79007AD2_LAYER_2_CACHE_NUM
	p_input = &p_glcdc->p_cfg->input[1];
	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		EK79007AD2_LOGW("Layer 2 setting has error, ignore");
		goto START_GLCDC;
	}
	lcd->layer_2.x_length = p_input->hsize;
	lcd->layer_2.x_dummy = (uint16_t)(p_input->hstride - p_input->hsize);
	lcd->layer_2.y_length = p_input->vsize;
	lcd->layer_2.y_dummy = 0;

	switch (p_input->format) {
	case DISPLAY_IN_FORMAT_32BITS_ARGB8888:
		lcd->layer_2.bytes_per_pixel = 4;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB8888;
		break;
	case DISPLAY_IN_FORMAT_32BITS_RGB888:
		lcd->layer_2.bytes_per_pixel = 4;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_RGB888;
		break;
	case DISPLAY_IN_FORMAT_16BITS_RGB565:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_RGB565;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB1555:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB1555;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB4444:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB4444;
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		goto CLOSE_GLCDC;
	}

	frame_bytes = p_input->hstride * p_input->vsize * lcd->layer_2.bytes_per_pixel;
	lcd->layer_2.graphic_mem_1 = (uint8_t *)p_input->p_base;
	lcd->layer_2.mem1_read_only = false;
#if EK79007AD2_LAYER_2_CACHE_NUM == 1
	lcd->layer_2.graphic_mem_2 = NULL;
	lcd->layer_2.mem2_read_only = true;
	lcd->layer_2.graphic_writeable = lcd->layer_2.graphic_mem_1;
#else
	lcd->layer_2.graphic_mem_2 = &lcd->layer_2.graphic_mem_1[frame_bytes];
	lcd->layer_2.mem2_read_only = false;
	lcd->layer_2.graphic_writeable = lcd->layer_2.graphic_mem_2;
#endif
	lcd->layer_2.graphic_displayed = lcd->layer_2.graphic_mem_1;
	lcd->layer_2.graphic_to_present = NULL;
	lcd->layer_2.graphic_submitted = NULL;
	lcd->layer_2.dirty_current.valid = false;
	lcd->layer_2.dirty_submitted.valid = false;
	lcd->layer_2.present_pending = false;
	memset(lcd->layer_2.graphic_mem_1, 0xFF, frame_bytes);
	if (lcd->layer_2.graphic_mem_2 != NULL) {
		memset(lcd->layer_2.graphic_mem_2, 0xFF, frame_bytes);
	}
#endif /* #if EK79007AD2_LAYER_2_CACHE_NUM */

#if EK79007AD2_LAYER_2_CACHE_NUM
START_GLCDC:
#endif
	lcd->layer_1.runtime_cfg = &s_layer1_input_cfg;
	s_layer1_input_cfg.input = p_glcdc->p_cfg->input[0];
	s_layer1_input_cfg.layer = p_glcdc->p_cfg->layer[0];
	s_layer1_input_cfg.input.p_base = (uint32_t *)lcd->layer_1.graphic_displayed;
	lcd->layer_2.runtime_cfg = &s_layer2_input_cfg;
	s_layer2_input_cfg.input = p_glcdc->p_cfg->input[1];
	s_layer2_input_cfg.layer = p_glcdc->p_cfg->layer[1];
	s_layer2_input_cfg.input.p_base = (uint32_t *)lcd->layer_2.graphic_displayed;

	__DSB();
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	err = p_glcdc->p_api->start(p_glcdc_ctrl);
	if (err != FSP_SUCCESS) {
		EK79007AD2_LOGE("Start failed: %" PRIu32, err);
		goto CLOSE_GLCDC;
	}

	return err;

CLOSE_GLCDC:
	close_err = p_glcdc->p_api->close(p_glcdc_ctrl);
	if (close_err != FSP_SUCCESS) {
		EK79007AD2_LOGE("Close after init failure failed: %" PRIu32, close_err);
	}
	return err;
}

/*================================== Private Functions ============================================*/

static uint32_t pinConfig(void)
{
	s_pin_cfg_data[0].pin = EK79007AD2_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = EK79007AD2_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg.number_of_pins = 2;
	s_pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&s_pin_cfg_data;
	s_pin_cfg.p_extend = NULL;

	uint32_t err = R_IOPORT_Open(&s_pin_ctrl, &s_pin_cfg);
	if (err == FSP_ERR_ALREADY_OPEN) {
		err = R_IOPORT_PinsCfg(&s_pin_ctrl, &s_pin_cfg);
	}
	return err;
}

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
		/* 两层独立完成交接，一层尚未生效时仍检查另一层。 */
		for (uint8_t i = 0; i < 2; i++) {
			LCD_LayerType *layer = (i == 0) ? &lcd->layer_1 : &lcd->layer_2;
			if (layer->present_pending == false) {
				continue;
			}

			uint8_t *submitted = layer->graphic_submitted;
			/* LCD_MoveLayer() 不会动显存，所以 graphic_submitted 会为 NULL。 */
			if (submitted == NULL) {
				__DMB();
				layer->present_pending = false;
				continue;
			}
			/* 与 TFP410 相同，以对应层的 FLM2 地址确认提交目标。 */
			if (R_GLCDC->GR[i].FLM2 != (uintptr_t)submitted) {
				continue;
			}

			layer->graphic_displayed = submitted;
			if ((layer->graphic_mem_1 != NULL) && (layer->mem1_read_only == false) &&
				((layer->graphic_mem_1 != submitted) || (layer->graphic_mem_2 == NULL))) {
				layer->graphic_writeable = layer->graphic_mem_1;
			}
			else if ((layer->graphic_mem_2 != NULL) && (layer->mem2_read_only == false) &&
				((layer->graphic_mem_2 != submitted) || (layer->graphic_mem_1 == NULL))) {
				layer->graphic_writeable = layer->graphic_mem_2;
			}
			else {
				layer->graphic_writeable = NULL;
			}
			if ((layer->graphic_writeable == NULL) || (layer->graphic_writeable == submitted)) {
				layer->dirty_submitted.valid = false;
			}
			layer->graphic_submitted = NULL;
			display_runtime_cfg_t *p_runtime = (display_runtime_cfg_t *)layer->runtime_cfg;
			if (p_runtime != NULL) {
				p_runtime->input.p_base = (uint32_t *)layer->graphic_displayed;
			}

			__DMB();
			layer->present_pending = false;
		}
		break;
	case DISPLAY_EVENT_FRAME_END:
		break;
	}
}

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

#endif /* #if LCD_EN_EK79007AD2 */
