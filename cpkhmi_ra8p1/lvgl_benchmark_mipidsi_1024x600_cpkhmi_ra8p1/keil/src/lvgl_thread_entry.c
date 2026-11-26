#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "console.h"
#include "i2c.h"
#include "lvgl.h"
#include "lvgl_thread.h"
#include "lv_examples.h"
#include "lv_demos.h"
#include "nor_flash.h"
#include "sdram.h"
#include "touch.h"
#include "usb_hid_code.h"
#include "usb_thread.h"

#include "lcd/lcd.h"
#include "perf_counter/perf_counter.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "utils/log.h"

/* FSP may using another name, confirm this macro is same as the FSP setting */
#ifndef FRAME_BUFFER_NAME
#define FRAME_BUFFER_NAME	fb_background
#endif

#define BM_EK79001		1
#define BM_EK79007AD2	2

#define BM_MODE			BM_EK79007AD2

static I2C_DeviceType s_i2c0;
static I2C_ConfigType s_i2c0_cfg;
static TOUCH_DeviceType s_touch;
static TOUCH_ConfigType s_touch_cfg;
static USB_THREAD_ParamType s_usb_thread_param;
static TaskHandle_t s_led_thread_handle;
static lv_indev_t *s_lvgl_mouse = NULL;
static lv_point_t s_mouse_point;
static lv_obj_t *s_mouse_cursor = NULL;
static lv_indev_state_t s_mouse_state = LV_INDEV_STATE_RELEASED;

#if BM_MODE == BM_EK79001
static LCD_DeviceType s_lcd_ek79001;
static LCD_ConfigType s_ek79001_cfg;
#elif BM_MODE == BM_EK79007AD2
static LCD_DeviceType s_lcd_ek79007ad2;
static LCD_ConfigType s_ek79007ad2_cfg;
#endif

static void diplayInfo(LCD_DeviceType *lcd);
static void ledThread(void *param);
static void lvgl_register_mouse(void);
static int mpuGetFrameBufferAttr(uint32_t *free_region, uint32_t *used_attrs);
static void mpuSetFrameBufferWT(void);

static void lvgl_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
static uint32_t touch_i2c_read(uint16_t reg_addr, uint8_t *data, uint8_t len);
static uint32_t touch_i2c_write(uint16_t reg_addr, uint8_t *data, uint8_t len);

#if LV_USE_DEMO_BENCHMARK
static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary);
#endif

void lvgl_thread_entry(void * pvParameters)
{
	EventBits_t eventbit;
	
	uint32_t err = FSP_SUCCESS;
	
	FSP_PARAMETER_NOT_USED(pvParameters);
	
	perfc_init(false);
#if CONSOLE_CFG_USE_RTT == 0
	SEGGER_RTT_Init();
#endif
	CONSOLE_Init();

	/* Clean screen */
#if CONSOLE_CFG_USE_RTT
    puts(RTT_CTRL_RESET);
    puts(RTT_CTRL_CLEAR);
#else
    puts("\x1b[2J\x1b[H");
    puts("\x1B[0m");
#endif
    printf("FSP Version: %s\r\n", FSP_VERSION_STRING);

    /* We hope GLCDC framebuffer is Write-Through mode and other memory region is
     * Write-Back mode when DCache enable, so we need reconfig MPU */
    mpuSetFrameBufferWT();

    NorFlash_Init();
#if TEST_EN_NOR_FLASH
    TestNorFlash(NORFLASH_MAP_START_ADDR, NORFLASH_SIZE);
#endif

    lv_init();

#if BM_MODE == BM_EK79001
    vTaskDelay(10000);
    s_ek79001_cfg.model = LCD_MODEL_EK79001;
    s_ek79001_cfg.peri_1 = &g_lvgl_port_ctrl;
    s_ek79001_cfg.peri_model_1 = LCD_PERI_LVGL;
    s_ek79001_cfg.peri_2 = NULL;
    s_ek79001_cfg.peri_model_2 = LCD_PERI_UNUSED;
    LCD_Initialize(&s_lcd_ek79001, &s_ek79001_cfg);
    vTaskDelay(1000);
#elif BM_MODE == BM_EK79007AD2
    s_ek79007ad2_cfg.model = LCD_MODEL_EK79007AD2;
    s_ek79007ad2_cfg.peri_1 = &g_lvgl_port_ctrl;
    s_ek79007ad2_cfg.peri_model_1 = LCD_PERI_LVGL;
    s_ek79007ad2_cfg.peri_2 = &g_mipi_dsi_ek79007;
    s_ek79007ad2_cfg.peri_model_2 = LCD_PERI_MIPI;
    LCD_Initialize(&s_lcd_ek79007ad2, &s_ek79007ad2_cfg);
#endif

    /* If the EK79001 is used, its touch IC shares the same I2C address as the TFP410.
	 * Make sure to set P005=LOW before calling to disable the TFP410's I2C interface. */
    s_i2c0_cfg.inst = &g_i2c_master0;
    s_i2c0_cfg.model = I2C_MODEL_IIC_MASTER;
    I2C_Init(&s_i2c0, &s_i2c0_cfg);
	
	s_usb_thread_param.i2c_device = &s_i2c0;
	s_usb_thread_param.i2c_sem = xSemaphoreCreateMutex();
	xTaskCreate(ledThread, "ledThread", 0x100, NULL, 2, &s_led_thread_handle);
	xTaskCreate(USB_Thread, "USB_Thread", 0x400, &s_usb_thread_param, 3, &g_usb_thread_handle);

#if BM_MODE == BM_EK79001
    s_touch_cfg.model = TOUCH_MODEL_FT5X16;
#elif BM_MODE == BM_EK79007AD2
    s_touch_cfg.model = TOUCH_MODEL_GT911;
#endif
    s_touch_cfg.i2c_read = touch_i2c_read;
    s_touch_cfg.i2c_write = touch_i2c_write;
    err = TOUCH_Init(&s_touch, &s_touch_cfg);
    if (err == FSP_SUCCESS) {
    #if TEST_EN_TOUCH
        TestTouch(&s_touch, 20);
	#endif
        TOUCH_RegisterLVGLTouchpad(&s_touch, 20);
    }
    if (err != FSP_SUCCESS) {
        LOG_E(__FUNCTION__, "Touch startup failed: %" PRIu32, err);
    }
	
	eventbit = xEventGroupGetBits(g_usb_thread_event);
	if (eventbit & USB_THREAD_MSG_ERROR_EXIT) {
		LOG_E(__FUNCTION__, "USB Thread exit due to error");
	}
	else {
		lvgl_register_mouse();
	}
	
	diplayInfo(&s_lcd_ek79007ad2);

#if LV_BUILD_DEMOS

#if LV_USE_DEMO_BENCHMARK
    lv_demo_benchmark_set_end_cb(benchmark_end_cb);
    lv_demo_benchmark();
#elif LV_USE_DEMO_MUSIC
	lv_demo_music();
#endif

#elif LV_BUILD_EXAMPLES
	lv_example_button_1();
#endif

	while(1) {
		lv_timer_handler();
		vTaskDelay(1);
	}
}

void GLCDC_Callback(display_callback_args_t *p_args)
{
	(void)p_args;
}

#if LCD_EN_PERI_MIPI
void MIPI_DSI_Callback(mipi_dsi_callback_args_t *p_args)
{
	(void)p_args;
}
#endif

#if LOG_CFG_EN_TIMESTAMP
void LOG_GetTime(uint32_t *s, uint32_t *ms)
{
	int64_t t = get_system_ms();
	*s = (uint32_t)(t / 1000);
	*ms = (uint32_t)(t % 1000);
}
#endif

static void diplayInfo(LCD_DeviceType *lcd)
{
	(void)lcd;
	
	printf("Color Format   : %u\r\n", LV_COLOR_DEPTH);
	
#if LV_USE_DRAW_DAVE2D
	printf("DAVE2D         : ");
	if (lv_subject_get_int((lv_subject_t *)gp_dave2d_enable)) {
		puts("Enable");
	}
	else {
		puts("Disable");
	}
#endif
	
	printf("Soft Accelerate: ");
#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NONE
	puts("None");
#elif LV_USE_DRAW_SW_ASM == LV_USE_DRAW_ARM2D_SYNC
	puts("ARM2D");
#elif LV_USE_DRAW_SW_ASM == LV_USE_NATIVE_HELIUM_ASM
	puts("Helium");
#elif LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_CUSTOM
	puts("Custom");
#endif
	
	fsp_priv_source_clock_t source = (fsp_priv_source_clock_t)R_SYSTEM->LCDCKCR_b.CKSEL;
	uint32_t divider = R_FSP_ClockDividerGet(R_SYSTEM->LCDCKDIVCR_b.CKDIV);
	uint32_t lcdclk = R_BSP_SourceClockHzGet(source) / divider;
	uint32_t glcdc_div = R_GLCDC->SYSCNT.PANEL_CLK_b.DCDR;
	uint32_t pixel_clk = lcdclk / glcdc_div;
	
	const display_instance_t *p_glcdc = NULL;
	if (lcd->peri_model_1 == LCD_PERI_LVGL) {
		p_glcdc = (const display_instance_t *)g_lvgl_port_cfg.p_display_instance;
	}
	else {
		p_glcdc = (const display_instance_t *)lcd->peri_1;
	}
	const display_cfg_t *p_glcdc_cfg = (const display_cfg_t *)p_glcdc->p_cfg;
	uint32_t hfps = pixel_clk / (p_glcdc_cfg->output.htiming.total_cyc * p_glcdc_cfg->output.vtiming.total_cyc);

	printf("Hardware FPS   : %u\r\n", hfps);
	printf("LVGL FPS       : %u\r\n", 1000 / LV_DEF_REFR_PERIOD);
	
	printf("Compiler       : ");
#if defined(__ARMCOMPILER_VERSION)
	puts("ARMCC");
#elif defined(__clang_version__)
	puts("LLVM");
#else
	puts("GCC");
#endif
}

static void ledThread(void *param)
{
	(void)param;

	while (1) {
		R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);
		vTaskDelay(500);
		R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW);
		vTaskDelay(500);
	}
}

static void lvgl_register_mouse(void)
{
	lv_display_t *disp = lv_display_get_default();
	
	s_lvgl_mouse = lv_indev_create();
	if (s_lvgl_mouse == NULL) {
		LOG_E(__FUNCTION__, "lv_indev_create() failed");
		return;
	}
	
	s_mouse_point.x = lv_display_get_horizontal_resolution(disp) / 2;
	s_mouse_point.y = lv_display_get_vertical_resolution(disp) / 2;
	lv_indev_set_type(s_lvgl_mouse, LV_INDEV_TYPE_POINTER);
	lv_indev_set_display(s_lvgl_mouse, disp);
	lv_indev_set_read_cb(s_lvgl_mouse, lvgl_mouse_read_cb);
	lv_timer_set_period(lv_indev_get_read_timer(s_lvgl_mouse), 10);
	
	/* Mouse style */
	s_mouse_cursor = lv_obj_create(lv_display_get_layer_sys(disp));
	lv_obj_set_size(s_mouse_cursor, 16, 16);
	lv_obj_set_style_radius(s_mouse_cursor, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(s_mouse_cursor, lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_mouse_cursor, LV_OPA_COVER, 0);
	lv_obj_add_flag(s_mouse_cursor, LV_OBJ_FLAG_HIDDEN);
	lv_indev_set_cursor(s_lvgl_mouse, s_mouse_cursor);
}

static int mpuGetFrameBufferAttr(uint32_t *free_region, uint32_t *used_attrs)
{
	const uint32_t first = (uint32_t)(uintptr_t)FRAME_BUFFER_NAME;
	const uint32_t last = first + sizeof(FRAME_BUFFER_NAME) - 1U;
	int attr = -1;

	*free_region = UINT32_MAX;
	*used_attrs = 0U;

	const uint32_t count = _FLD2VAL(MPU_TYPE_DREGION, MPU->TYPE);
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	const uint32_t saved_rnr = MPU->RNR;

	for (uint32_t i = 0; i < count; ++i) {
		MPU->RNR = i;
		const uint32_t rlar = MPU->RLAR;
		if ((rlar & MPU_RLAR_EN_Msk) == 0U) {
			if (*free_region == UINT32_MAX) {
				*free_region = i;
			}
			continue;
		}

		const uint32_t index = _FLD2VAL(MPU_RLAR_AttrIndx, rlar);
		*used_attrs |= 1U << index;
		const uint32_t base = MPU->RBAR & MPU_RBAR_BASE_Msk;
		const uint32_t limit = (rlar & MPU_RLAR_LIMIT_Msk) | 0x1FU;
		if (limit < first || base > last) {
			continue;
		}
		if (attr >= 0 || base > first || limit < last) {
			attr = -2;
			break;
		}

		const uint32_t mair = (index < 4U) ? MPU->MAIR0 : MPU->MAIR1;
		attr = (int)((mair >> (8U * (index % 4U))) & 0xFFU);
	}

	MPU->RNR = saved_rnr;
	__set_PRIMASK(primask);
	return attr;
}

static void mpuSetFrameBufferWT(void)
{
	/* Check whether D-Cache is enable
	 * We check FSP config macro rather then SCB register
	 * because FSP bsp_init_mpu() will be called only when this macro is 1 */
#if BSP_CFG_DCACHE_ENABLED == 0
	LOG_E(__FUNCTION__, "Please turn on D-Cache via FSP. Set FrameBuffer memory attribute failed");
	return;
#endif

	/* The MPU of ARMv8.1 does not support area coverage
	 * If there is an overlap of areas, a MPU fault will be triggered during access.
	 * If FrameBuffer is set to an uncacheable position("_nocache suffix") in FSP,
	 * then in bsp_mpu_init(), an area will be assigned to it.
	 * So we need to check this case */
	const uint8_t wt_attr = ARM_MPU_ATTR(MPU_ATTR_NORMAL_OUTER_WT_RA, MPU_ATTR_NORMAL_INNER_WT_RA);
	uint32_t region, used_attrs;
	const int current_attr = mpuGetFrameBufferAttr(&region, &used_attrs);
	if (current_attr == 0x44) {
		LOG_E(__FUNCTION__, "FrameBuffer already be place in a Non-Cacheable area by FSP, please modify this setting");
		return;
	}
	if (current_attr != -1) {
		if (current_attr != wt_attr) {
			LOG_E(__FUNCTION__, "FrameBuffer overlaps an existing MPU region");
		}
		return;
	}

#if BSP_CFG_DCACHE_FORCE_WRITETHROUGH
	LOG_W(__FUNCTION__, "FSP forces Write-Through globally; disable Force Write-Through in the BSP to allow Write-Back elsewhere");
#endif

	uint32_t base = (uint32_t)(uintptr_t)FRAME_BUFFER_NAME;
	uint32_t limit = base + sizeof(FRAME_BUFFER_NAME); /* Exclusive end of both buffers. */

	base &= ~31U;
	limit = (limit + 31U) & ~31U;
	/* RLAR encodes the base of the last 32-byte block. */
	limit -= 32U;

	if (region == UINT32_MAX) {
		LOG_E(__FUNCTION__, "No free MPU region for FrameBuffer");
		return;
	}

	/* FSP reserves MAIR attribute 0 for non-cacheable memory. */
	uint8_t index = 1U;
	while (index < 8U && (used_attrs & (1U << index)) != 0U) {
		const uint32_t mair = MPU->MAIR[index / 4U];
		if ((uint8_t)(mair >> (8U * (index % 4U))) == wt_attr) {
			break;
		}
		++index;
	}
	if (index == 8U) {
		LOG_E(__FUNCTION__, "No free MPU memory attribute for FrameBuffer");
		return;
	}

	/* At startup this thread owns the buffers until LCD_Initialize(). */
	SCB_CleanInvalidateDCache_by_Addr((void *)(uintptr_t)base, (int32_t)(limit - base + 32U));

	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	const uint32_t saved_rnr = MPU->RNR;
	if ((used_attrs & (1U << index)) == 0U) {
		ARM_MPU_SetMemAttr(index, wt_attr);
	}
	uint32_t rbar = ARM_MPU_RBAR(base, ARM_MPU_SH_NON, ARM_MPU_AP_RW, ARM_MPU_AP_PO, ARM_MPU_XN);
	uint32_t rlar = ARM_MPU_RLAR(limit, index);
	ARM_MPU_SetRegion(region, rbar, rlar);
	MPU->RNR = saved_rnr;
	__DSB();
	__ISB();
	__set_PRIMASK(primask);
}

static void lvgl_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
	int8_t dx, dy;
	int32_t max_x;
	int32_t max_y;
	BaseType_t xReturn;
	UBaseType_t queue_msg_num;
	USB_HID_QueueContentType report;
	
	(void)indev;

	bool mouse_attach = false;
	EventBits_t eventbit = xEventGroupGetBits(g_usb_thread_event);
	lv_display_t *disp = lv_display_get_default();
	
	if ((eventbit & USB_THREAD_MSG_MOUSE_ATTACH) && ((eventbit & USB_THREAD_MSG_ERROR_EXIT) == 0)) {
		mouse_attach = true;
	}
	
	if (mouse_attach) {
		lv_obj_set_flag(s_mouse_cursor, LV_OBJ_FLAG_HIDDEN, false);
		xReturn = xQueueReceive(g_usb_data_queue, &report, 0);
		if (xReturn == pdTRUE) {
			if (report.device_type == USB_HID_TYPE_MOUSE) {
				max_x = lv_display_get_horizontal_resolution(disp) - 1;
				max_y = lv_display_get_vertical_resolution(disp) - 1;
				dx = (int8_t)report.data[1];
				dy = (int8_t)report.data[2];
				s_mouse_point.x = LV_CLAMP(0, s_mouse_point.x + dx, max_x);
				s_mouse_point.y = LV_CLAMP(0, s_mouse_point.y + dy, max_y);
				s_mouse_state = report.data[0] & 0x01 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
			}
		}
	}
	else {
		lv_obj_set_flag(s_mouse_cursor, LV_OBJ_FLAG_HIDDEN, true);
		s_mouse_state = LV_INDEV_STATE_RELEASED;
	}
	
	data->point = s_mouse_point;
	data->state = s_mouse_state;
	
	queue_msg_num = uxQueueMessagesWaiting(g_usb_data_queue);
	data->continue_reading = queue_msg_num ? true : false;
}

static uint32_t touch_i2c_read(uint16_t reg_addr, uint8_t *data, uint8_t len)
{
	uint32_t err;
	I2C_WRMemParamType param;

#if BM_MODE == BM_EK79001
	param.slave = 0x38;
	param.mem_width = 8;
#elif BM_MODE == BM_EK79007AD2
	param.slave = 0x14;
	param.mem_width = 16;
#endif
	param.mem_addr = reg_addr;
	param.data = data;
	param.length = len;
	
	xSemaphoreTake(s_usb_thread_param.i2c_sem, portMAX_DELAY);
	err = I2C_ReadMem(&s_i2c0, &param);
	if (err) {
		LOG_E(__FUNCTION__, "I2C_ReadMem() failed: %u", err);
		xSemaphoreGive(s_usb_thread_param.i2c_sem);
		return err;
	}
	err = I2C_WaitOperation(&s_i2c0, I2C_OP_RX_DONE, 100);
	if (err) {
		LOG_E(__FUNCTION__, "I2C_WaitOperation() failed: %u", err);
	}
	xSemaphoreGive(s_usb_thread_param.i2c_sem);

	return err;
}

static uint32_t touch_i2c_write(uint16_t reg_addr, uint8_t *data, uint8_t len)
{
	uint32_t err;
	I2C_WRMemParamType param;

#if BM_MODE == BM_EK79001
	param.slave = 0x38;
	param.mem_width = 8;
#elif BM_MODE == BM_EK79007AD2
	param.slave = 0x14;
	param.mem_width = 16;
#endif
	param.mem_addr = reg_addr;
	param.data = data;
	param.length = len;
	
	xSemaphoreTake(s_usb_thread_param.i2c_sem, portMAX_DELAY);
	err = I2C_WriteMem(&s_i2c0, &param);
	if (err) {
		LOG_E(__FUNCTION__, "I2C_WriteMem() failed: %u", err);
		xSemaphoreGive(s_usb_thread_param.i2c_sem);
		return err;
	}
	err = I2C_WaitOperation(&s_i2c0, I2C_OP_TX_DONE, 100);
	if (err) {
		LOG_E(__FUNCTION__, "I2C_WaitOperation() failed: %u", err);
	}
	xSemaphoreGive(s_usb_thread_param.i2c_sem);

	return err;
}

#if LV_USE_DEMO_BENCHMARK
static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary)
{
	lv_demo_benchmark_summary_display(summary);

	puts("GLCDC underflow count:");
	if ((s_lcd_ek79007ad2.layer_1.graphic_mem_1 != NULL) || (s_lcd_ek79007ad2.layer_1.graphic_mem_2 != NULL)) {
		printf("GR1 underflow: %u\r\n", s_lcd_ek79007ad2.gr1_underflow_cnt);
	}
	if ((s_lcd_ek79007ad2.layer_2.graphic_mem_1 != NULL) || (s_lcd_ek79007ad2.layer_2.graphic_mem_2 != NULL)) {
		printf("GR2 underflow: %u\r\n", s_lcd_ek79007ad2.gr2_underflow_cnt);
	}
}
#endif
