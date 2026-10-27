#include "test.h"

#if TEST_EN_LCD

/*================================== INCLUDES =====================================================*/

#include <stdlib.h>
#include "hal_data.h"

#include "lcd/lcd.h"
#include "lcd/picture.h"
#include "perf_counter/perf_counter.h"
#include "utils/log.h"

/*================================== MACROS =======================================================*/

#ifndef TEST_LCD_EN_BACKLIGHT
#define TEST_LCD_EN_BACKLIGHT			0
#endif

#ifndef TEST_LCD_EN_BITMAP_BLOCK
#define TEST_LCD_EN_BITMAP_BLOCK		0
#endif

#ifndef TEST_LCD_EN_COLOR_BAND
#define TEST_LCD_EN_COLOR_BAND          1
#endif

#ifndef TEST_LCD_EN_COLOR_BLOCK
#define TEST_LCD_EN_COLOR_BLOCK			0
#endif

#ifndef TEST_LCD_EN_DRAW_POINT
#define TEST_LCD_EN_DRAW_POINT			0
#endif

#ifndef TEST_LCD_EN_FILL_SCREEN
#define TEST_LCD_EN_FILL_SCREEN			0
#endif

#ifndef TEST_LCD_EN_PICTURE
#define TEST_LCD_EN_PICTURE				0
#endif

#ifndef TEST_LCD_EN_PERFORMANCE
#define TEST_LCD_EN_PERFORMANCE			0
#endif

#ifndef TEST_LCD_EN_PERF_ONLY_FRESH
#define TEST_LCD_EN_PERF_ONLY_FRESH		0
#endif

#if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI

#ifndef TEST_LCD_EN_PRESURE
#define TEST_LCD_EN_PRESURE				0
#endif

#ifndef TEST_LCD_EN_PRESURE_REFINED
#define TEST_LCD_EN_PRESURE_REFINED		0
#endif

#endif /* #if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI */

#ifndef TEST_LCD_EN_USER_MALLOC
#define TEST_LCD_EN_USER_MALLOC			0
#endif

#if TEST_LCD_EN_PERFORMANCE || TEST_LCD_EN_PERF_ONLY_FRESH

#ifndef LCD_RUNNING_TIME_S
#define LCD_RUNNING_TIME_S				20
#endif

#endif

#if TEST_LCD_EN_PRESURE

#ifndef LCD_PRESURE_TIME_S
#define LCD_PRESURE_TIME_S				20
#endif

#endif

#if TEST_LCD_EN_PRESURE_REFINED

#ifndef LCD_PRESURE_REFINED_SIZE
#define LCD_PRESURE_REFINED_SIZE		256
#endif

/**
 * 1: At SDRAM
 * 2: At HyperRAM */
#ifndef LCD_FRAME_BUFFER_POS
#define LCD_FRAME_BUFFER_POS			2
#endif

#endif

#if TEST_LCD_EN_USER_MALLOC

#include TEST_LCD_USER_MALLOC_H

#ifndef TEST_LCD_MALLOC
#define TEST_LCD_MALLOC(x)				NULL
#endif

#ifndef TEST_LCD_FREE
#define TEST_LCD_FREE(x)
#endif

#endif /* #if TEST_LCD_EN_USER_MALLOC */

/*================================== TYPES ========================================================*/

enum performance_mode_enum {
	PERFORMANCE_ONLY_L1,
	PERFORMANCE_ONLY_L2,
	PERFORMANCE_L1_AND_L2
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

#if TEST_LCD_EN_PRESURE_REFINED
static float s_speed_sum_r[LCD_PRESURE_REFINED_SIZE];
static float s_speed_sum_w[LCD_PRESURE_REFINED_SIZE];
#endif

/*================================== Private Functions Prototypes =================================*/

#if TEST_LCD_EN_BACKLIGHT
static uint32_t testLCDBacklight(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_BITMAP_BLOCK
static uint32_t testLCDBitmapBlock(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_COLOR_BAND
static uint32_t testLCDColorBand(LCD_DeviceType *lcd, uint8_t layer);
#endif

#if TEST_LCD_EN_COLOR_BLOCK
static uint32_t testLCDColorBlock(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_DRAW_POINT
static uint32_t testLCDDrawPoint(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_FILL_SCREEN
static uint32_t testLCDFillScreen(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PICTURE
static uint32_t testLCDPicture(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PERFORMANCE
static uint32_t testLCDPerformance(LCD_DeviceType *lcd, enum performance_mode_enum mode);
#endif

#if TEST_LCD_EN_PERF_ONLY_FRESH
static uint32_t testLCDPerformanceOnlyFresh(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PRESURE || TEST_LCD_EN_PRESURE_REFINED

#include "sdram.h"

#if TEST_LCD_EN_PRESURE
static uint32_t testLCDPresure(LCD_DeviceType *lcd);
#endif

#if TEST_LCD_EN_PRESURE_REFINED
static uint32_t testLCDPresureRefined(LCD_DeviceType *lcd);
#endif

#endif

/*================================== Public Functions =============================================*/

uint32_t TestLCD(void *lcd_device)
{
	LCD_DeviceType *lcd = (LCD_DeviceType *)lcd_device;

	(void)lcd;

#if TEST_LCD_EN_BACKLIGHT
	testLCDBacklight(lcd);
#endif

#if TEST_LCD_EN_BITMAP_BLOCK
	testLCDBitmapBlock(lcd);
#endif

#if TEST_LCD_EN_COLOR_BAND
	testLCDColorBand(lcd, 1);
	R_BSP_SoftwareDelay(4, BSP_DELAY_UNITS_SECONDS);
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
#endif

#if TEST_LCD_EN_COLOR_BLOCK
	testLCDColorBlock(lcd);
#endif

#if TEST_LCD_EN_DRAW_POINT
	testLCDDrawPoint(lcd);
#endif

#if TEST_LCD_EN_FILL_SCREEN
	testLCDFillScreen(lcd);
#endif

#if TEST_LCD_EN_PICTURE
	testLCDPicture(lcd);
#endif

#if TEST_LCD_EN_PERFORMANCE
	/* 只有一层时就只刷新 Layer1
	 * 双层时两层都刷新，只测试最坏的情况 */
	if ((lcd->layer_2.graphic_mem_1 == NULL) && (lcd->layer_2.graphic_mem_2 == NULL)) {
		LOG_I(__FUNCTION__, "LCD only configured Layer 1");
		testLCDPerformance(lcd, PERFORMANCE_ONLY_L1);
	}
	else {
		LOG_I(__FUNCTION__, "LCD configured Layer 1 and Layer 2");
		testLCDPerformance(lcd, PERFORMANCE_L1_AND_L2);
	}
#endif

#if TEST_LCD_EN_PERF_ONLY_FRESH
	testLCDPerformanceOnlyFresh(lcd);
#endif

#if TEST_LCD_EN_PRESURE
	testLCDPresure(lcd);
#endif

#if TEST_LCD_EN_PRESURE_REFINED
	testLCDPresureRefined(lcd);
#endif

	return 0;
}

/*================================== Private Functions ============================================*/

#if TEST_LCD_EN_BACKLIGHT
/**
 * @brief	测试 ST7796U 的背光调节接口
 * @param	lcd LCD 实例
 * @return	执行调节后返回最后一次 LCD_SetBackLight() 的结果，0 表示成功，非 0 表示失败
 * @details	仅在启用 LCD_EN_ST7796U 且实例型号为 ST7796U 时执行，依次传入 0 到 16 的背光值。
 * 			每次调节成功后延时 2 秒；失败时输出警告并停止调节。
 */
static uint32_t testLCDBacklight(LCD_DeviceType *lcd)
{
	uint8_t i;

	uint32_t err = FSP_SUCCESS;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_BACKLIGHT");
#if LCD_EN_ST7796U
	if (lcd->model == LCD_MODEL_ST7796U) {
		for (i = 0; i < 17; i++) {
			err = LCD_SetBackLight(lcd, i);
			if (err) {
				LOG_W(__FUNCTION__, "%s not support set backlight", lcd->name);
				break;
			}
			R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		}
	}
#endif

	return err;
}
#endif /* #if TEST_LCD_EN_BACKLIGHT */

#if TEST_LCD_EN_BITMAP_BLOCK
/**
 * @brief	通过 LCD_DrawBitmap() 在第 1 层依次绘制红、绿、蓝、橙四色块
 * @param	lcd LCD 实例
 * @return	0: 测试流程结束；1: 第 1 层颜色格式不支持或位图内存分配失败
 * @details	根据第 1 层颜色格式选择颜色，分配宽、高分别为该层一半的临时位图。
 * 			每次绘制前延时 2 秒，依次在左上、右上、左下、右下位置绘制并提交，最后调用配置对应的释放接口。
 * @note	使用 TEST_LCD_MALLOC() 或标准 malloc() 分配内存；当前 RTOS 分支未实现分配，将直接跳过测试。
 */
static uint32_t testLCDBitmapBlock(LCD_DeviceType *lcd)
{
	uint32_t i;
	uint32_t need_size;
	uint32_t colors[4];
	LCD_BitmapType bitmap;

	uint8_t *p8_bitmap = NULL;
	uint16_t *p16_bitmap = NULL;
	uint32_t *p32_bitmap = NULL;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_BITMAP_BLOCK");
	need_size = (lcd->layer_1.x_length / 2) * (lcd->layer_1.y_length / 2) * lcd->layer_1.bytes_per_pixel;

	switch (lcd->layer_1.color_format) {
	case LCD_COLOR_FORMAT_ARGB8888:
		colors[0] = LCD_COLOR_ARGB8888_RED;
		colors[1] = LCD_COLOR_ARGB8888_GREEN;
		colors[2] = LCD_COLOR_ARGB8888_BLUE;
		colors[3] = LCD_COLOR_ARGB8888_ORANGE;
		break;
	case LCD_COLOR_FORMAT_RGB565:
		colors[0] = LCD_COLOR_RGB565_RED;
		colors[1] = LCD_COLOR_RGB565_GREEN;
		colors[2] = LCD_COLOR_RGB565_BLUE;
		colors[3] = LCD_COLOR_RGB565_ORANGE;
		break;
	case LCD_COLOR_FORMAT_RGB888:
		colors[0] = LCD_COLOR_RGB888_RED;
		colors[1] = LCD_COLOR_RGB888_GREEN;
		colors[2] = LCD_COLOR_RGB888_BLUE;
		colors[3] = LCD_COLOR_RGB888_ORANGE;
		break;
	default:
		LOG_E(__FUNCTION__, "Unsupport color format");
		return 1;
	}

#if TEST_LCD_EN_USER_MALLOC
	p8_bitmap = (uint8_t *)TEST_LCD_MALLOC(need_size);
#elif BSP_CFG_RTOS == 1
#elif BSP_CFG_RTOS == 2
#else
	p8_bitmap = (uint8_t *)malloc(need_size);
#endif

	if (p8_bitmap == NULL) {
		LOG_W(__FUNCTION__, "Can't malloc %u bytes, ignore this case", need_size);
		return 1;
	}

	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (lcd->layer_1.bytes_per_pixel == 2) {
		p16_bitmap = (uint16_t *)p8_bitmap;
		for (i = 0; i < (need_size / 2); i++) {
			p16_bitmap[i] = (uint16_t)colors[0];
		}
	}
	else {
		p32_bitmap = (uint32_t *)p8_bitmap;
		for (i = 0; i < (need_size / 4); i++) {
			p32_bitmap[i] = colors[0];
		}
	}
	bitmap.x = 0;
	bitmap.y = 0;
	bitmap.length = lcd->layer_1.x_length / 2;
	bitmap.width = lcd->layer_1.y_length / 2;
	bitmap.bitmap = p8_bitmap;
	LCD_DrawBitmap(lcd, 1, &bitmap);
	LCD_Present(lcd, 1);

	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (lcd->layer_1.bytes_per_pixel == 2) {
		for (i = 0; i < (need_size / 2); i++) {
			p16_bitmap[i] = (uint16_t)colors[1];
		}
	}
	else {
		for (i = 0; i < (need_size / 4); i++) {
			p32_bitmap[i] = colors[1];
		}
	}
	bitmap.x = lcd->layer_1.x_length / 2;
	bitmap.y = 0;
	LCD_DrawBitmap(lcd, 1, &bitmap);
	LCD_Present(lcd, 1);

	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (lcd->layer_1.bytes_per_pixel == 2) {
		for (i = 0; i < (need_size / 2); i++) {
			p16_bitmap[i] = (uint16_t)colors[2];
		}
	}
	else {
		for (i = 0; i < (need_size / 4); i++) {
			p32_bitmap[i] = colors[2];
		}
	}
	bitmap.x = 0;
	bitmap.y = lcd->layer_1.y_length / 2;
	LCD_DrawBitmap(lcd, 1, &bitmap);
	LCD_Present(lcd, 1);

	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (lcd->layer_1.bytes_per_pixel == 2) {
		for (i = 0; i < (need_size / 2); i++) {
			p16_bitmap[i] = (uint16_t)colors[3];
		}
	}
	else {
		for (i = 0; i < (need_size / 4); i++) {
			p32_bitmap[i] = colors[3];
		}
	}
	bitmap.x = lcd->layer_1.x_length / 2;
	bitmap.y = lcd->layer_1.y_length / 2;
	LCD_DrawBitmap(lcd, 1, &bitmap);
	LCD_Present(lcd, 1);
#if TEST_LCD_EN_USER_MALLOC
	TEST_LCD_FREE(p_bitmap);
#elif BSP_CFG_RTOS == 1
#elif BSP_CFG_RTOS == 2
#else
	free(p8_bitmap);
#endif

	return 0;
}
#endif /* #if TEST_LCD_EN_BITMAP_BLOCK */

#if TEST_LCD_EN_COLOR_BAND
/**
 * @brief	在指定显示层绘制红、绿、蓝、黑、白、黄、紫、浅蓝八种颜色的横向彩条
 * @param	lcd   LCD 实例
 * @param	layer 要测试的显示层，调用时应传入 1 或 2
 * @return	总是返回 0，不反映绘制或提交结果
 * @details	延时 2 秒并等待指定层空闲后，从上到下填充彩条，每条高度为该层高度除以 8 向下取整。
 * 			绘制完成后统一提交，并以 1 毫秒间隔轮询，直到该层空闲。
 */
static uint32_t testLCDColorBand(LCD_DeviceType *lcd, uint8_t layer)
{
	bool idle;
	uint32_t color[8];
	LCD_RectangelType rect;

	LCD_LayerType *draw_layer = NULL;
	if (layer == 1) {
		draw_layer = &lcd->layer_1;
	}
	else {
		draw_layer = &lcd->layer_2;
	}

	LOG_I(__FUNCTION__, "TEST_LCD_EN_COLOR_BAND");
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	if (draw_layer->color_format == LCD_COLOR_FORMAT_RGB565) {
		color[0] = LCD_COLOR_RGB565_RED;
		color[1] = LCD_COLOR_RGB565_GREEN;
		color[2] = LCD_COLOR_RGB565_BLUE;
		color[3] = LCD_COLOR_RGB565_BLACK;
		color[4] = LCD_COLOR_RGB565_WHITE;
		color[5] = LCD_COLOR_RGB565_YELLOW;
		color[6] = LCD_COLOR_RGB565_PURPLE;
		color[7] = LCD_COLOR_RGB565_LIGHT_BLUE;
	}
	else if (draw_layer->color_format == LCD_COLOR_FORMAT_RGB888) {
		color[0] = LCD_COLOR_RGB888_RED;
		color[1] = LCD_COLOR_RGB888_GREEN;
		color[2] = LCD_COLOR_RGB888_BLUE;
		color[3] = LCD_COLOR_RGB888_BLACK;
		color[4] = LCD_COLOR_RGB888_WHITE;
		color[5] = LCD_COLOR_RGB888_YELLOW;
		color[6] = LCD_COLOR_RGB888_PURPLE;
		color[7] = LCD_COLOR_RGB888_LIGHT_BLUE;
	}
	else {
		color[0] = LCD_COLOR_ARGB8888_RED;
		color[1] = LCD_COLOR_ARGB8888_GREEN;
		color[2] = LCD_COLOR_ARGB8888_BLUE;
		color[3] = LCD_COLOR_ARGB8888_BLACK;
		color[4] = LCD_COLOR_ARGB8888_WHITE;
		color[5] = LCD_COLOR_ARGB8888_YELLOW;
		color[6] = LCD_COLOR_ARGB8888_PURPLE;
		color[7] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
	}

	LCD_IsIdle(lcd, layer, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, layer, &idle);
	}

    uint16_t step = (uint16_t)(draw_layer->y_length / (sizeof(color) / sizeof(color[0])));
    for (uint16_t i = 0; i < (sizeof(color) / sizeof(color[0])); i++) {
		rect.x = 0;
		rect.y = i * step;
		rect.length = draw_layer->x_length;
		rect.width = step;
		rect.color = color[i];
        LCD_FillRectangle(lcd, layer, &rect);
    }

    LCD_Present(lcd, layer);
    LCD_IsIdle(lcd, layer, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, layer, &idle);
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_COLOR_BAND */

#if TEST_LCD_EN_COLOR_BLOCK
/**
 * @brief	通过 LCD_FillRectangle() 在第 1 层依次绘制红、绿、蓝、橙四色块
 * @param	lcd LCD 实例
 * @return	0: 测试流程结束；1: 第 1 层颜色格式不支持
 * @details	根据第 1 层颜色格式选择颜色，使用宽、高分别为该层一半的矩形。
 * 			每次绘制前延时 2 秒，依次在左上、右上、左下、右下位置填充矩形并提交。
 * @note	绘制和提交接口的返回值未检查，返回 0 不表示显示操作均成功。
 */
static uint32_t testLCDColorBlock(LCD_DeviceType *lcd)
{
	uint32_t colors[4];
	LCD_RectangelType rect;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_COLOR_BLOCK");
	switch (lcd->layer_1.color_format) {
	case LCD_COLOR_FORMAT_ARGB8888:
		colors[0] = LCD_COLOR_ARGB8888_RED;
		colors[1] = LCD_COLOR_ARGB8888_GREEN;
		colors[2] = LCD_COLOR_ARGB8888_BLUE;
		colors[3] = LCD_COLOR_ARGB8888_ORANGE;
		break;
	case LCD_COLOR_FORMAT_RGB565:
		colors[0] = LCD_COLOR_RGB565_RED;
		colors[1] = LCD_COLOR_RGB565_GREEN;
		colors[2] = LCD_COLOR_RGB565_BLUE;
		colors[3] = LCD_COLOR_RGB565_ORANGE;
		break;
	case LCD_COLOR_FORMAT_RGB888:
		colors[0] = LCD_COLOR_RGB888_RED;
		colors[1] = LCD_COLOR_RGB888_GREEN;
		colors[2] = LCD_COLOR_RGB888_BLUE;
		colors[3] = LCD_COLOR_RGB888_ORANGE;
		break;
	default:
		LOG_E(__FUNCTION__, "Unsupport color format");
		return 1;
	}

	rect.x = 0;
	rect.y = 0;
	rect.length = lcd->layer_1.x_length / 2;
	rect.width = lcd->layer_1.y_length / 2;
	rect.color = colors[0];
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 1, &rect);
	LCD_Present(lcd, 1);

	rect.x = lcd->layer_1.x_length / 2;
	rect.color = colors[1];
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 1, &rect);
	LCD_Present(lcd, 1);

	rect.x = 0;
	rect.y = lcd->layer_1.y_length / 2;
	rect.color = colors[2];
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 1, &rect);
	LCD_Present(lcd, 1);

	rect.x = lcd->layer_1.x_length / 2;
	rect.color = colors[3];
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_FillRectangle(lcd, 1, &rect);
	LCD_Present(lcd, 1);

	return 0;
}
#endif /* #if TEST_LCD_EN_COLOR_BLOCK */

#if TEST_LCD_EN_DRAW_POINT
/**
 * @brief	逐点将第 1 层填充为灰色，测试 LCD_DrawPoint() 接口
 * @param	lcd LCD 实例
 * @return	0: 测试流程结束；1: 第 1 层颜色格式不支持
 * @details	等待第 1 层空闲后，按从左到右、从上到下的顺序遍历该层所有像素。
 * 			每绘制一个像素便提交，提交失败时以 1 毫秒间隔重试，再等待该层空闲后绘制下一个像素。
 * @note	绘点接口的返回值未检查，提交重试和空闲等待均无超时限制。且每帧只绘制一个点，全屏画完需要很长时间
 */
static uint32_t testLCDDrawPoint(LCD_DeviceType *lcd)
{
	bool idle;
	uint16_t x, y;
	uint32_t color;
	uint32_t err;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_DRAW_POINT");
	switch (lcd->layer_1.color_format) {
	case LCD_COLOR_FORMAT_ARGB8888:
		color = LCD_COLOR_ARGB8888_GREY;
		break;
	case LCD_COLOR_FORMAT_RGB565:
		color = LCD_COLOR_RGB565_GREY;
		break;
	case LCD_COLOR_FORMAT_RGB888:
		color = LCD_COLOR_RGB888_GREY;
		break;
	default:
		LOG_E(__FUNCTION__, "Unsupport color format");
		return 1;
	}

	LCD_IsIdle(lcd, 1, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		LCD_IsIdle(lcd, 1, &idle);
	}

	for (y = 0; y < lcd->layer_1.y_length; y++) {
		for (x = 0; x < lcd->layer_1.x_length; x++) {
			LCD_DrawPoint(lcd, 1, x, y, color);

			err = LCD_Present(lcd, 1);
			while (err) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				err = LCD_Present(lcd, 1);
			}

			LCD_IsIdle(lcd, 1, &idle);
			while (idle == false) {
				R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
				LCD_IsIdle(lcd, 1, &idle);
			}
		}
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_DRAW_POINT */

#if TEST_LCD_EN_FILL_SCREEN
/**
 * @brief	测试第 1 层的整层颜色填充
 * @param	lcd LCD 实例
 * @return	总是返回 0，不反映填充或提交结果
 * @details	根据第 1 层颜色格式选择颜色，依次填充黑、蓝、绿、灰、橙、紫、红、白、黄。
 * 			每次填充前延时 2 秒，填充后立即调用 LCD_Present() 提交。
 */
static uint32_t testLCDFillScreen(LCD_DeviceType *lcd)
{
	LOG_I(__FUNCTION__, "TEST_LCD_EN_FILL_SCREEN");
	if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB565) {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_BLACK);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_BLUE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_GREEN);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_GREY);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_ORANGE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_PURPLE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_RED);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_WHITE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB565_YELLOW);
		LCD_Present(lcd, 1);
	}
	else if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB888) {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_BLACK);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_BLUE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_GREEN);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_GREY);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_ORANGE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_PURPLE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_RED);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_WHITE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_RGB888_YELLOW);
		LCD_Present(lcd, 1);
	}
	else {
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_BLACK);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_BLUE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_GREEN);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_GREY);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_ORANGE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_PURPLE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_RED);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_WHITE);
		LCD_Present(lcd, 1);
		R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
		LCD_Fill(lcd, 1, LCD_COLOR_ARGB8888_YELLOW);
		LCD_Present(lcd, 1);
	}

	return 0;
}
#endif /* #if TEST_LCD_EN_FILL_SCREEN */

#if TEST_LCD_EN_PICTURE
/**
 * @brief	在第 1 层左上角依次显示两幅内置测试图片
 * @param	lcd LCD 实例
 * @return	总是返回 0，不反映绘制或提交结果
 * @details	延时 2 秒后绘制并提交 g_picture_222x480_01，延时 5 秒后绘制并提交 g_picture_222x480_02，
 * 			再延时 5 秒后结束；每幅图片使用其自身记录的宽、高和像素数据。
 * @note	调用者需确保图片方向和像素格式与显示配置匹配，本函数不执行格式转换。
 * 			内置的两张图片为 RGB565 格式，若内部显存不使用这个格式，则最好关闭这个测试，或添加其它转换函数
 */
static uint32_t testLCDPicture(LCD_DeviceType *lcd)
{
	LCD_BitmapType bitmap;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PICTURE");
	LOG_I(__FUNCTION__, "You need to ensure that the orientation and format of the image are correct");

	bitmap.x = 0;
	bitmap.y = 0;
	bitmap.length = (uint16_t)g_picture_222x480_01.width;
	bitmap.width = (uint16_t)g_picture_222x480_01.height;
	bitmap.bitmap = (const uint8_t *)g_picture_222x480_01.pixel_data;
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_SECONDS);
	LCD_DrawBitmap(lcd, 1, &bitmap);
	LCD_Present(lcd, 1);

	bitmap.length = (uint16_t)g_picture_222x480_02.width;
	bitmap.width = (uint16_t)g_picture_222x480_02.height;
	bitmap.bitmap = (const uint8_t *)g_picture_222x480_02.pixel_data;
	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_SECONDS);
    LCD_DrawBitmap(lcd, 1, &bitmap);
    LCD_Present(lcd, 1);
	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_SECONDS);

	return 0;
}
#endif /* #if TEST_LCD_EN_PICTURE */

#if TEST_LCD_EN_PERFORMANCE
/**
 * @brief	测试第 1 层持续填充提交和静态显示时的性能
 * @param	lcd LCD 实例
 * @return	0: 测试流程结束；1: 已配置图层的颜色格式不支持
 * @details	等待第 1 层空闲后，循环使用 8 种颜色填充、提交并等待空闲，持续至少 LCD_RUNNING_TIME_S 秒。
 * 			按完成的循环数计算帧率，统计平均填充时间和平均提交等待时间。
 * 			随后保持当前画面，延时 LCD_RUNNING_TIME_S 秒，根据 VSYNC 计数增量计算静态显示帧率。
 * 			两个阶段分别输出 GR1、GR2 underflow 计数增量；启用 MIPI 时还输出对应错误计数增量。
 * @note	填充时间仅包含 LCD_Fill()；等待时间包含 LCD_Present() 的失败重试及 LCD_IsIdle() 轮询。
 * 			提交重试和空闲等待均无超时限制，返回 0 不反映填充结果或显示错误计数。
 * 			FSP 配置中确保开启对应层的 underflow 中断，否则读不到 underflow 计数
 */
static uint32_t testLCDPerformance(LCD_DeviceType *lcd, enum performance_mode_enum mode)
{
	bool idle1, idle2;
	uint32_t i, err1, err2;
	int64_t time_start, time_end;
	int64_t time_fill_s, time_fill_e;
	int64_t time_wait_s, time_wait_e;
	uint32_t colors_layer1[8];
	uint32_t colors_layer2[8];
	float time_s;

	uint32_t frame_cnt = 0;
	int64_t time_fill = 0;
	int64_t time_wait = 0;
	int64_t time_stop = LCD_RUNNING_TIME_S * 1000;
	float fps = 0.0f;

	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PERFORMANCE");

	switch (lcd->layer_1.color_format) {
	case LCD_COLOR_FORMAT_ARGB1555:
		colors_layer1[0] = LCD_COLOR_ARGB1555_WHITE;
		colors_layer1[1] = LCD_COLOR_ARGB1555_BLUE;
		colors_layer1[2] = LCD_COLOR_ARGB1555_GREEN;
		colors_layer1[3] = LCD_COLOR_ARGB1555_GREY;
		colors_layer1[4] = LCD_COLOR_ARGB1555_LIGHT_BLUE;
		colors_layer1[5] = LCD_COLOR_ARGB1555_ORANGE;
		colors_layer1[6] = LCD_COLOR_ARGB1555_PURPLE;
		colors_layer1[7] = LCD_COLOR_ARGB1555_RED;
		break;
	case LCD_COLOR_FORMAT_ARGB4444:
		colors_layer1[0] = LCD_COLOR_ARGB4444_WHITE;
		colors_layer1[1] = LCD_COLOR_ARGB4444_BLUE;
		colors_layer1[2] = LCD_COLOR_ARGB4444_GREEN;
		colors_layer1[3] = LCD_COLOR_ARGB4444_GREY;
		colors_layer1[4] = LCD_COLOR_ARGB4444_LIGHT_BLUE;
		colors_layer1[5] = LCD_COLOR_ARGB4444_ORANGE;
		colors_layer1[6] = LCD_COLOR_ARGB4444_PURPLE;
		colors_layer1[7] = LCD_COLOR_ARGB4444_RED;
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
		colors_layer1[0] = LCD_COLOR_ARGB8888_WHITE;
		colors_layer1[1] = LCD_COLOR_ARGB8888_BLUE;
		colors_layer1[2] = LCD_COLOR_ARGB8888_GREEN;
		colors_layer1[3] = LCD_COLOR_ARGB8888_GREY;
		colors_layer1[4] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
		colors_layer1[5] = LCD_COLOR_ARGB8888_ORANGE;
		colors_layer1[6] = LCD_COLOR_ARGB8888_PURPLE;
		colors_layer1[7] = LCD_COLOR_ARGB8888_RED;
		break;
	case LCD_COLOR_FORMAT_RGB565:
		colors_layer1[0] = LCD_COLOR_RGB565_WHITE;
		colors_layer1[1] = LCD_COLOR_RGB565_BLUE;
		colors_layer1[2] = LCD_COLOR_RGB565_GREEN;
		colors_layer1[3] = LCD_COLOR_RGB565_GREY;
		colors_layer1[4] = LCD_COLOR_RGB565_LIGHT_BLUE;
		colors_layer1[5] = LCD_COLOR_RGB565_ORANGE;
		colors_layer1[6] = LCD_COLOR_RGB565_PURPLE;
		colors_layer1[7] = LCD_COLOR_RGB565_RED;
		break;
	case LCD_COLOR_FORMAT_RGB888:
		colors_layer1[0] = LCD_COLOR_RGB888_WHITE;
		colors_layer1[1] = LCD_COLOR_RGB888_BLUE;
		colors_layer1[2] = LCD_COLOR_RGB888_GREEN;
		colors_layer1[3] = LCD_COLOR_RGB888_GREY;
		colors_layer1[4] = LCD_COLOR_RGB888_LIGHT_BLUE;
		colors_layer1[5] = LCD_COLOR_RGB888_ORANGE;
		colors_layer1[6] = LCD_COLOR_RGB888_PURPLE;
		colors_layer1[7] = LCD_COLOR_RGB888_RED;
		break;
	default:
		LOG_E(__FUNCTION__, "Unsupported Layer 1 color format");
		return 1;
	}

	if ((lcd->layer_2.graphic_mem_1 == NULL) && (lcd->layer_2.graphic_mem_2 == NULL)) {
		if (mode != PERFORMANCE_ONLY_L1) {
			LOG_W(__FUNCTION__, "Only has Layer 1 but require to test Layer 2, ignore Layer 2 test");
			mode = PERFORMANCE_ONLY_L1;
		}
	}
	else {
		switch (lcd->layer_2.color_format) {
		case LCD_COLOR_FORMAT_ARGB1555:
			colors_layer2[0] = LCD_COLOR_ARGB1555_RED;
			colors_layer2[1] = LCD_COLOR_ARGB1555_PURPLE;
			colors_layer2[2] = LCD_COLOR_ARGB1555_ORANGE;
			colors_layer2[3] = LCD_COLOR_ARGB1555_LIGHT_BLUE;
			colors_layer2[4] = LCD_COLOR_ARGB1555_GREY;
			colors_layer2[5] = LCD_COLOR_ARGB1555_GREEN;
			colors_layer2[6] = LCD_COLOR_ARGB1555_BLUE;
			colors_layer2[7] = LCD_COLOR_ARGB1555_WHITE;
			break;
		case LCD_COLOR_FORMAT_ARGB4444:
			colors_layer2[0] = LCD_COLOR_ARGB4444_RED;
			colors_layer2[1] = LCD_COLOR_ARGB4444_PURPLE;
			colors_layer2[2] = LCD_COLOR_ARGB4444_ORANGE;
			colors_layer2[3] = LCD_COLOR_ARGB4444_LIGHT_BLUE;
			colors_layer2[4] = LCD_COLOR_ARGB4444_GREY;
			colors_layer2[5] = LCD_COLOR_ARGB4444_GREEN;
			colors_layer2[6] = LCD_COLOR_ARGB4444_BLUE;
			colors_layer2[7] = LCD_COLOR_ARGB4444_WHITE;
			break;
		case LCD_COLOR_FORMAT_ARGB8888:
			colors_layer2[0] = LCD_COLOR_ARGB8888_RED;
			colors_layer2[1] = LCD_COLOR_ARGB8888_PURPLE;
			colors_layer2[2] = LCD_COLOR_ARGB8888_ORANGE;
			colors_layer2[3] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
			colors_layer2[4] = LCD_COLOR_ARGB8888_GREY;
			colors_layer2[5] = LCD_COLOR_ARGB8888_GREEN;
			colors_layer2[6] = LCD_COLOR_ARGB8888_BLUE;
			colors_layer2[7] = LCD_COLOR_ARGB8888_WHITE;
			break;
		case LCD_COLOR_FORMAT_RGB565:
			colors_layer2[0] = LCD_COLOR_RGB565_RED;
			colors_layer2[1] = LCD_COLOR_RGB565_PURPLE;
			colors_layer2[2] = LCD_COLOR_RGB565_ORANGE;
			colors_layer2[3] = LCD_COLOR_RGB565_LIGHT_BLUE;
			colors_layer2[4] = LCD_COLOR_RGB565_GREY;
			colors_layer2[5] = LCD_COLOR_RGB565_GREEN;
			colors_layer2[6] = LCD_COLOR_RGB565_BLUE;
			colors_layer2[7] = LCD_COLOR_RGB565_WHITE;
			break;
		case LCD_COLOR_FORMAT_RGB888:
			colors_layer2[0] = LCD_COLOR_RGB888_RED;
			colors_layer2[1] = LCD_COLOR_RGB888_PURPLE;
			colors_layer2[2] = LCD_COLOR_RGB888_ORANGE;
			colors_layer2[3] = LCD_COLOR_RGB888_LIGHT_BLUE;
			colors_layer2[4] = LCD_COLOR_RGB888_GREY;
			colors_layer2[5] = LCD_COLOR_RGB888_GREEN;
			colors_layer2[6] = LCD_COLOR_RGB888_BLUE;
			colors_layer2[7] = LCD_COLOR_RGB888_WHITE;
			break;
		default:
			LOG_E(__FUNCTION__, "Unsupported Layer 2 color format");
			return 1;
		}
	}

	LCD_IsIdle(lcd, 1, &idle1);
	while (idle1 == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, 1, &idle1);
	}

	time_start = get_system_ms();
	while (1) {
		time_fill_s = get_system_us();
		switch (mode) {
		case PERFORMANCE_ONLY_L1:
			LCD_Fill(lcd, 1, colors_layer1[frame_cnt % 8]);
			break;
		case PERFORMANCE_ONLY_L2:
			LCD_Fill(lcd, 2, colors_layer2[frame_cnt % 8]);
			break;
		case PERFORMANCE_L1_AND_L2:
			LCD_Fill(lcd, 1, colors_layer1[frame_cnt % 8]);
			LCD_Fill(lcd, 2, colors_layer2[frame_cnt % 8]);
			break;
		default:
			break;
		}
		time_fill_e = get_system_us();

		time_wait_s = time_fill_e;
		switch (mode) {
		case PERFORMANCE_ONLY_L1:
			err1 = LCD_Present(lcd, 1);
			while (err1) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				err1 = LCD_Present(lcd, 1);
			}
			LCD_IsIdle(lcd, 1, &idle1);
			while (idle1 == false) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				LCD_IsIdle(lcd, 1, &idle1);
			}
			break;
		case PERFORMANCE_ONLY_L2:
			err1 = LCD_Present(lcd, 2);
			while (err1) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				err1 = LCD_Present(lcd, 2);
			}
			LCD_IsIdle(lcd, 2, &idle1);
			while (idle1 == false) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				LCD_IsIdle(lcd, 2, &idle1);
			}
			break;
		case PERFORMANCE_L1_AND_L2:
			err1 = LCD_Present(lcd, 1);
			err2 = LCD_Present(lcd, 2);
			while (err1 || err2) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				if (err1) {
					err1 = LCD_Present(lcd, 1);
				}
				if (err2) {
					err2 = LCD_Present(lcd, 2);
				}
			}
			LCD_IsIdle(lcd, 1, &idle1);
			LCD_IsIdle(lcd, 2, &idle2);
			while ((idle1 == false) || (idle2 == false)) {
				R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
				if (idle1 == false) {
					LCD_IsIdle(lcd, 1, &idle1);
				}
				if (idle2 == false) {
					LCD_IsIdle(lcd, 2, &idle2);
				}
			}
			break;
		default:
			break;
		}
		time_wait_e = get_system_us();

		time_end = get_system_ms();

		frame_cnt++;
		time_fill += time_fill_e - time_fill_s;
		time_wait += time_wait_e - time_wait_s;
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)frame_cnt / time_s;
	printf("============== Write Each Frame ===============\r\n");
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_cnt);
	printf("Calculate FPS      : %.3f\r\n", fps);

	time_s = (float)time_fill / (float)frame_cnt / 1000.0f;
	printf("Average fill time  : %.3fms\r\n", time_s);
	time_s = (float)time_wait / (float)frame_cnt / 1000.0f;
	printf("Average wait time  : %.3fms\r\n", time_s);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	frame_cnt = lcd->vsync_cnt;
	gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif
	time_start = get_system_ms();
	for (i = 0; i < LCD_RUNNING_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	time_s = (float)(time_end - time_start) / 1000.0f;
	frame_cnt = lcd->vsync_cnt - frame_cnt;
	fps = (float)frame_cnt / time_s;
	printf("=============== Static Display ================\r\n");
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_cnt);
	printf("Calculate FPS      : %.3f\r\n", fps);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	return 0;
}
#endif /* #if TEST_LCD_EN_PERFORMANCE */

#if TEST_LCD_EN_PERF_ONLY_FRESH
/**
 * @brief	根据 VSYNC 计数统计当前画面的静态显示帧率
 * @param	lcd LCD 实例
 * @return	总是返回 0，不反映显示错误计数
 * @details	延时 LCD_RUNNING_TIME_S 秒后，根据实际耗时和 VSYNC 计数增量计算帧率，
 * 			输出 GR1、GR2 underflow 计数增量；启用 MIPI 时还输出对应错误计数增量。
 * @note	本函数不绘制或提交新画面，也不等待已有显示提交完成。
 */
static uint32_t testLCDPerformanceOnlyFresh(LCD_DeviceType *lcd)
{
	uint32_t i;
	uint32_t frame_start;
	uint32_t frame_end;
	int64_t time_start, time_end;
	float fps;
	float time_s;

	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PERF_ONLY_FRESH");


	frame_start = lcd->vsync_cnt;
	time_start = get_system_ms();
	for (i = 0; i < LCD_RUNNING_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	frame_end = lcd->vsync_cnt;

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)(frame_end - frame_start) / time_s;
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total fresh frame  : %" PRIu32 "\r\n", frame_end - frame_start);
	printf("Calculate FPS      : %.3f\r\n", fps);

	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	return 0;
}
#endif /* #if TEST_LCD_EN_PERF_ONLY_FRESH */

#if TEST_LCD_EN_PRESURE
/**
 * @brief	测试持续填充提交时叠加 SDRAM 读写负载的影响
 * @param	lcd LCD 实例
 * @return	正常执行结束后返回 0，不反映显示错误计数
 * @details	先循环使用 8 种颜色填充第 1 层、提交并等待空闲，作为不额外执行 SDRAM 读写测速的基准。
 * 			随后重复相同显示操作，每次提交成功后执行 SDRAM 写入和读取测速，再等待第 1 层空闲。
 * 			两个阶段分别持续至少 LCD_PRESURE_TIME_S 秒，并输出显示错误计数。
 * 			第二阶段按循环次数计算平均 SDRAM 读写速度，未启用 D-cache 时输出各项平均值。
 * @note	第二阶段输出的错误计数仍以函数入口为基准，包含两个阶段的累计增量。
 * 			提交重试和空闲等待均无超时限制。
 */
static uint32_t testLCDPresure(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t i;
	uint32_t err;
	uint32_t colors[8];
	int64_t time_start, time_end;
	float sdram_speed_r[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_w[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_r_sum[TEST_SDRAM_SPEED_COUNT];
	float sdram_speed_w_sum[TEST_SDRAM_SPEED_COUNT];

	uint32_t frame_cnt = 0;
	uint32_t gr1_underflow_cnt = lcd->gr1_underflow_cnt;
	uint32_t gr2_underflow_cnt = lcd->gr2_underflow_cnt;
	int64_t time_stop = LCD_PRESURE_TIME_S * 1000;
#if LCD_EN_PERI_MIPI
	uint32_t mipi_timing_err_cnt = lcd->mipi_timing_err_cnt;
	uint32_t mipi_underflow_cnt = lcd->mipi_underflow_cnt;
	uint32_t mipi_overflow_cnt = lcd->mipi_overflow_cnt;
#endif

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PRESURE");

	if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB565) {
		colors[0] = LCD_COLOR_RGB565_WHITE;
		colors[1] = LCD_COLOR_RGB565_BLUE;
		colors[2] = LCD_COLOR_RGB565_GREEN;
		colors[3] = LCD_COLOR_RGB565_GREY;
		colors[4] = LCD_COLOR_RGB565_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB565_ORANGE;
		colors[6] = LCD_COLOR_RGB565_PURPLE;
		colors[7] = LCD_COLOR_RGB565_RED;
	}
	else if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB888) {
		colors[0] = LCD_COLOR_RGB888_WHITE;
		colors[1] = LCD_COLOR_RGB888_BLUE;
		colors[2] = LCD_COLOR_RGB888_GREEN;
		colors[3] = LCD_COLOR_RGB888_GREY;
		colors[4] = LCD_COLOR_RGB888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_RGB888_ORANGE;
		colors[6] = LCD_COLOR_RGB888_PURPLE;
		colors[7] = LCD_COLOR_RGB888_RED;
	}
	else {
		colors[0] = LCD_COLOR_ARGB8888_WHITE;
		colors[1] = LCD_COLOR_ARGB8888_BLUE;
		colors[2] = LCD_COLOR_ARGB8888_GREEN;
		colors[3] = LCD_COLOR_ARGB8888_GREY;
		colors[4] = LCD_COLOR_ARGB8888_LIGHT_BLUE;
		colors[5] = LCD_COLOR_ARGB8888_ORANGE;
		colors[6] = LCD_COLOR_ARGB8888_PURPLE;
		colors[7] = LCD_COLOR_ARGB8888_RED;
	}

	LCD_IsIdle(lcd, 1, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, 1, &idle);
	}

	time_start = get_system_ms();
	while (1) {
		LCD_Fill(lcd, 1, colors[frame_cnt % 8]);

		err = LCD_Present(lcd, 1);
		while (err) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			err = LCD_Present(lcd, 1);
		}

		LCD_IsIdle(lcd, 1, &idle);
		while (idle == false) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			LCD_IsIdle(lcd, 1, &idle);
		}

		frame_cnt++;

		time_end = get_system_ms();
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	printf("Result when not issue SDRAM WR\r\n");
	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	memset(sdram_speed_r_sum, 0, sizeof(sdram_speed_r_sum));
	memset(sdram_speed_w_sum, 0, sizeof(sdram_speed_w_sum));
	LCD_IsIdle(lcd, 1, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, 1, &idle);
	}

	time_start = get_system_ms();
	frame_cnt = 0;
	while (1) {
		LCD_Fill(lcd, 1, colors[frame_cnt % 8]);

		err = LCD_Present(lcd, 1);
		while (err) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			err = LCD_Present(lcd, 1);
		}

		TestSDRAM_SpeedWrite(sdram_speed_w, TEST_SDRAM_WIDTH_32BIT);
		TestSDRAM_SpeedRead(sdram_speed_r, TEST_SDRAM_WIDTH_32BIT);
		for (i = 0; i < TEST_SDRAM_SPEED_COUNT; i++) {
			sdram_speed_r_sum[i] += sdram_speed_r[i];
			sdram_speed_w_sum[i] += sdram_speed_w[i];
		}

		LCD_IsIdle(lcd, 1, &idle);
		while (idle == false) {
			R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
			LCD_IsIdle(lcd, 1, &idle);
		}

		frame_cnt++;

		time_end = get_system_ms();
		if ((time_end - time_start) >= time_stop) {
			break;
		}
	}

	for (i = 0; i < TEST_SDRAM_SPEED_COUNT; i++) {
		sdram_speed_r_sum[i] = sdram_speed_r_sum[i] / (float)frame_cnt;
		sdram_speed_w_sum[i] = sdram_speed_w_sum[i] / (float)frame_cnt;
	}

	printf("Result when issue SDRAM WR\r\n");
	printf("GR1 underflow cnt  : %" PRIu32 "\r\n", lcd->gr1_underflow_cnt - gr1_underflow_cnt);
	printf("GR2 underflow cnt  : %" PRIu32 "\r\n", lcd->gr2_underflow_cnt - gr2_underflow_cnt);
#if LCD_EN_PERI_MIPI
	printf("MIPI timing err cnt: %" PRIu32 "\r\n", lcd->mipi_timing_err_cnt - mipi_timing_err_cnt);
	printf("MIPI underflow cnt : %" PRIu32 "\r\n", lcd->mipi_underflow_cnt - mipi_underflow_cnt);
	printf("MIPI overflow cnt  : %" PRIu32 "\r\n", lcd->mipi_overflow_cnt - mipi_overflow_cnt);
#endif

	printf("Average SDRAM speed in 32bit width\r\n");
#if BSP_CFG_DCACHE_ENABLED
#else
	printf("Write from DTCM  to SDRAM with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_w_sum[0]);
	printf("Write from RAM   to SDRAM with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_w_sum[1]);
	printf("Read  from SDRAM to DTCM  with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_r_sum[0]);
	printf("Read  from SDRAM to RAM   with 32bit width, speed: %.2f MB/s\r\n", sdram_speed_r_sum[1]);
#endif

	return 0;
}
#endif

#if TEST_LCD_EN_PRESURE_REFINED
/**
 * @brief	在 VSYNC 计数变化之间采样 SRAM 与 SDRAM 的读写速度
 * @param	lcd LCD 实例
 * @return	0: 完成读写采样；1: 任一方向的采样次数达到 LCD_PRESURE_REFINED_SIZE
 * @details	等待第 1 层空闲后，分别等待一次 VSYNC 计数变化，再连续执行 SRAM 到 SDRAM 的写入测速
 * 			及 SDRAM 到 SRAM 的读取测速，直到再次观察到 VSYNC 计数变化。
 * 			RGB565 使用 16 位访问，其它颜色格式使用 32 位访问；分别输出耗时、次数和每次测速结果。
 * @note	当前绘制及提交代码被禁用，采样期间保持已有画面；单次测速完成后才检查 VSYNC，末次测速可能跨越其变化。
 * 			空闲等待和 VSYNC 等待均无超时限制，需确保显示刷新及 VSYNC 计数更新正常运行。
 */
static uint32_t testLCDPresureRefined(LCD_DeviceType *lcd)
{
	bool idle;
	uint32_t i, err;
	uint32_t color;
	int64_t time_start, time_end;
	float speed_r;
	float speed_w;
	volatile uint32_t frame_cnt;
#if LCD_FRAME_BUFFER_POS == 1
	TestSDRAM_WidthEnum width;
#else
	TestHR_WidthEnum width;
#endif

	const char *width_info = NULL;
	uint32_t test_cnt_w = 0;
	uint32_t test_cnt_r = 0;

	(void)err;

	LOG_I(__FUNCTION__, "TEST_LCD_EN_PRESURE_REFINED");

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	LCD_IsIdle(lcd, 1, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, 1, &idle);
	}

	if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB565) {
		color = LCD_COLOR_RGB565_GREY;
	#if LCD_FRAME_BUFFER_POS == 1
		width = TEST_SDRAM_WIDTH_16BIT;
	#else
		width = TEST_HYPER_RAM_WIDTH_16BIT;
	#endif
		width_info = "16bit";
	}
	else if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB888) {
		color = LCD_COLOR_RGB888_GREY;
	#if LCD_FRAME_BUFFER_POS == 1
		width = TEST_SDRAM_WIDTH_32BIT;
	#else
		width = TEST_HYPER_RAM_WIDTH_32BIT;
	#endif
		width_info = "32bit";
	}
	else {
		color = LCD_COLOR_ARGB8888_GREY;
	#if LCD_FRAME_BUFFER_POS == 1
		width = TEST_SDRAM_WIDTH_32BIT;
	#else
		width = TEST_HYPER_RAM_WIDTH_32BIT;
	#endif
		width_info = "32bit";
	}

	LCD_IsIdle(lcd, 1, &idle);
	while (idle == false) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		LCD_IsIdle(lcd, 1, &idle);
	}

	/* Write speed test */
	frame_cnt = lcd->vsync_cnt;
	while (frame_cnt == lcd->vsync_cnt) {
		__NOP();
	}

#if 0
	LCD_Fill(lcd, color);
	err = LCD_Present(lcd);
	while (err) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		err = LCD_Present(lcd);
	}
#endif

	frame_cnt = lcd->vsync_cnt;
	time_start = get_system_ms();
	while (frame_cnt == lcd->vsync_cnt) {
	#if LCD_FRAME_BUFFER_POS == 1
		TestSDRAM_Speed(&speed_w, width, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
	#else
		TestHyperRAM_Speed(&speed_w, width, TEST_HR_DIR_SRAM_TO_HR);
	#endif
		s_speed_sum_w[test_cnt_w] = speed_w;
		test_cnt_w++;

		if (test_cnt_w >= LCD_PRESURE_REFINED_SIZE) {
			LOG_E(__FUNCTION__, "LCD_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			return 1;
		}
	}
	time_end = get_system_ms();

	printf("============ Writing speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", test_cnt_w);
#if LCD_FRAME_BUFFER_POS == 1
	printf("SRAM ==> SDRAM, %s width\r\n", width_info);
#else
	printf("SRAM ==> HyperRAM, %s width\r\n", width_info);
#endif
	for (i = 0; i < test_cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
	}

	/* Read speed test */
	if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB565) {
		color = LCD_COLOR_RGB565_GREEN;
	}
	else if (lcd->layer_1.color_format == LCD_COLOR_FORMAT_RGB888) {
		color = LCD_COLOR_RGB888_GREEN;
	}
	else {
		color = LCD_COLOR_ARGB8888_GREEN;
	}

	frame_cnt = lcd->vsync_cnt;
	while (frame_cnt == lcd->vsync_cnt) {
		__NOP();
	}

#if 0
	LCD_Fill(lcd, color);
	err = LCD_Present(lcd);
	while (err) {
		R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
		err = LCD_Present(lcd);
	}
#endif

	frame_cnt = lcd->vsync_cnt;
	time_start = get_system_ms();
	while (frame_cnt == lcd->vsync_cnt) {
	#if LCD_FRAME_BUFFER_POS == 1
		TestSDRAM_Speed(&speed_r, width, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
	#else
		TestHyperRAM_Speed(&speed_r, width, TEST_HR_DIR_HR_TO_SRAM);
	#endif
		s_speed_sum_r[test_cnt_r] = speed_r;
		test_cnt_r++;

		if (test_cnt_r >= LCD_PRESURE_REFINED_SIZE) {
			LOG_E(__FUNCTION__, "LCD_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			return 1;
		}
	}
	time_end = get_system_ms();

	printf("============ Reading speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", test_cnt_r);
#if LCD_FRAME_BUFFER_POS == 1
	printf("SDRAM ==> SRAM, %s width\r\n", width_info);
#else
	printf("HyperRAM ==> SRAM, %s width\r\n", width_info);
#endif
	for (i = 0; i < test_cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
	}

	return 0;
}
#endif

#endif /* #if TEST_EN_LCD */
