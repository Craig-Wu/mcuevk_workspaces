#include "lcd_ek79001.h"

#if LCD_EN_EK79001

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <string.h>
#include "hal_data.h"

/*================================== MACROS =======================================================*/

/* For RGB interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef EK79001_LAYER_1_CACHE_NUM
#define EK79001_LAYER_1_CACHE_NUM			1
#endif

/* For RGB interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 2->Framebuffer->Number of framebuffers.
 * Set to 0 only when Graphics Layer 2 is also disabled in FSP. */
#ifndef EK79001_LAYER_2_CACHE_NUM
#define EK79001_LAYER_2_CACHE_NUM			1
#endif

#define EK79001_PIN_BACKLIGHT				BSP_IO_PORT_00_PIN_12
#define EK79001_PIN_RESET					BSP_IO_PORT_00_PIN_13

#ifndef __EK79001_DEBUG
#define __EK79001_DEBUG	1
#endif

#if (EK79001_LAYER_1_CACHE_NUM != 1) && (EK79001_LAYER_1_CACHE_NUM != 2)
#error "EK79001_LAYER_1_CACHE_NUM must be 1 or 2"
#endif

#if (EK79001_LAYER_2_CACHE_NUM < 0) || (EK79001_LAYER_2_CACHE_NUM > 2)
#error "EK79001_LAYER_2_CACHE_NUM must be 0, 1 or 2"
#endif

#if __EK79001_DEBUG
#include "utils/log.h"
#define EK79001_LOGD(msg, ...)           	LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGE(msg, ...)   			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGI(msg, ...)           	LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define EK79001_LOGW(msg, ...)           	LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#else
#define EK79001_LOGD(msg, ...)
#define EK79001_LOGE(msg, ...)
#define EK79001_LOGI(msg, ...)
#define EK79001_LOGW(msg, ...)
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[32];
static display_runtime_cfg_t s_layer1_input_cfg;
static display_runtime_cfg_t s_layer2_input_cfg;

/*================================== Private Functions Prototypes =================================*/

static void glcdcCallback(display_callback_args_t *p_args);
static uint32_t pinConfig(void);

/*================================== Public Functions =============================================*/

/**
 * @brief	初始化 EK79001 和 GLCDC，绘图及提交使用 lcd.c 的默认实现。
 * @param	lcd LCD 实例，peri_1 为 GLCDC。
 * @return	成功返回 FSP_SUCCESS，失败返回参数、配置或底层驱动错误码。
 */
uint32_t LCD_EK79001_Init(LCD_DeviceType *lcd)
{
	uint32_t err;
	uint32_t close_err;
	uint32_t frame_bytes;

	if (lcd == NULL) {
		return FSP_ERR_ASSERTION;
	}
	if (lcd->peri_model_1 != LCD_PERI_GLCDC) {
		return FSP_ERR_UNSUPPORTED;
	}
	if (lcd->peri_1 == NULL) {
		return FSP_ERR_ASSERTION;
	}

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	if ((p_glcdc->p_ctrl == NULL) || (p_glcdc->p_cfg == NULL) || (p_glcdc->p_api == NULL)) {
		return FSP_ERR_ASSERTION;
	}
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
	const display_input_cfg_t *p_input = &p_glcdc->p_cfg->input[0];

	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		return FSP_ERR_ASSERTION;
	}

	err = pinConfig();
	UNLIKE_RETURN(err, FSP_SUCCESS, "Pin config failed: %" PRIu32, err);
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79001_PIN_BACKLIGHT, BSP_IO_LEVEL_LOW);
	R_BSP_PinWrite(EK79001_PIN_RESET, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(EK79001_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		EK79001_LOGI("GLCDC already open");
		err = p_glcdc->p_api->close(p_glcdc_ctrl);
		UNLIKE_RETURN(err, FSP_SUCCESS, "Close failed: %" PRIu32, err);
	}
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;

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
#if EK79001_LAYER_1_CACHE_NUM == 1
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
	memset(lcd->layer_1.graphic_mem_1, 0x00, frame_bytes);
	if (lcd->layer_1.graphic_mem_2 != NULL) {
		memset(lcd->layer_1.graphic_mem_2, 0x00, frame_bytes);
	}

	/* Layer 2 未配置时保留为空，并清除重复初始化前的状态。 */
	lcd->layer_2.index = 2;
	lcd->layer_2.mem1_read_only = true;
	lcd->layer_2.mem2_read_only = true;
#if EK79001_LAYER_2_CACHE_NUM
	p_input = &p_glcdc->p_cfg->input[1];
	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		EK79001_LOGW("Layer 2 setting has error, ignore");
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
#if EK79001_LAYER_2_CACHE_NUM == 1
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
	memset(lcd->layer_2.graphic_mem_1, 0x00, frame_bytes);
	if (lcd->layer_2.graphic_mem_2 != NULL) {
		memset(lcd->layer_2.graphic_mem_2, 0x00, frame_bytes);
	}
#endif /* #if EK79001_LAYER_2_CACHE_NUM */

#if EK79001_LAYER_2_CACHE_NUM
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
		EK79001_LOGE("Start failed: %" PRIu32, err);
		goto CLOSE_GLCDC;
	}
	R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);

	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(EK79001_PIN_BACKLIGHT, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();

	return err;

CLOSE_GLCDC:
	close_err = p_glcdc->p_api->close(p_glcdc_ctrl);
	if (close_err != FSP_SUCCESS) {
		EK79001_LOGE("Close after init failure failed: %" PRIu32, close_err);
	}
	return err;
}

/*================================== Private Functions ============================================*/

static uint32_t pinConfig(void)
{
	s_pin_cfg_data[0].pin = EK79001_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = EK79001_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[2].pin = BSP_IO_PORT_05_PIN_15;
	s_pin_cfg_data[2].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[3].pin = BSP_IO_PORT_09_PIN_14;
	s_pin_cfg_data[3].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[4].pin = BSP_IO_PORT_09_PIN_15;
	s_pin_cfg_data[4].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[5].pin = BSP_IO_PORT_09_PIN_03;
	s_pin_cfg_data[5].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[6].pin = BSP_IO_PORT_09_PIN_02;
	s_pin_cfg_data[6].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[7].pin = BSP_IO_PORT_09_PIN_10;
	s_pin_cfg_data[7].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[8].pin = BSP_IO_PORT_09_PIN_11;
	s_pin_cfg_data[8].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[9].pin = BSP_IO_PORT_09_PIN_12;
	s_pin_cfg_data[9].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[10].pin = BSP_IO_PORT_09_PIN_13;
	s_pin_cfg_data[10].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[11].pin = BSP_IO_PORT_09_PIN_04;
	s_pin_cfg_data[11].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[12].pin = BSP_IO_PORT_02_PIN_07;
	s_pin_cfg_data[12].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[13].pin = BSP_IO_PORT_11_PIN_07;
	s_pin_cfg_data[13].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[14].pin = BSP_IO_PORT_11_PIN_06;
	s_pin_cfg_data[14].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[15].pin = BSP_IO_PORT_11_PIN_05;
	s_pin_cfg_data[15].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[16].pin = BSP_IO_PORT_11_PIN_01;
	s_pin_cfg_data[16].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[17].pin = BSP_IO_PORT_11_PIN_04;
	s_pin_cfg_data[17].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[18].pin = BSP_IO_PORT_11_PIN_03;
	s_pin_cfg_data[18].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[19].pin = BSP_IO_PORT_11_PIN_02;
	s_pin_cfg_data[19].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[20].pin = BSP_IO_PORT_11_PIN_00;
	s_pin_cfg_data[20].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[21].pin = BSP_IO_PORT_07_PIN_07;
	s_pin_cfg_data[21].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[22].pin = BSP_IO_PORT_07_PIN_11;
	s_pin_cfg_data[22].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[23].pin = BSP_IO_PORT_07_PIN_12;
	s_pin_cfg_data[23].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[24].pin = BSP_IO_PORT_07_PIN_13;
	s_pin_cfg_data[24].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[25].pin = BSP_IO_PORT_07_PIN_14;
	s_pin_cfg_data[25].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[26].pin = BSP_IO_PORT_07_PIN_15;
	s_pin_cfg_data[26].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[27].pin = BSP_IO_PORT_07_PIN_10;
	s_pin_cfg_data[27].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[28].pin = BSP_IO_PORT_08_PIN_06;
	s_pin_cfg_data[28].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[29].pin = BSP_IO_PORT_08_PIN_05;
	s_pin_cfg_data[29].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[30].pin = BSP_IO_PORT_08_PIN_07;
	s_pin_cfg_data[30].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;
	s_pin_cfg_data[31].pin = BSP_IO_PORT_05_PIN_13;
	s_pin_cfg_data[31].pin_cfg = (uint32_t)IOPORT_CFG_DRIVE_HIGH | (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_LCD_GRAPHICS;

	s_pin_cfg.number_of_pins = 32;
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

#endif /* #if LCD_EN_EK79001 */
