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

static const uint32_t sc_lcd_model_name_len = sizeof(sc_lcd_model_name) / sizeof(sc_lcd_model_name[0]);

/*================================== Private Functions Prototypes =================================*/

static void dirtyRectClear(LCD_DirtyRectType *rect);
static void dirtyRectMerge(LCD_DirtyRectType *rect, uint16_t x, uint16_t y, uint16_t length, uint16_t width);
static uint32_t prepareWriteBuffer(LCD_LayerType *draw_layer, bool overwrite_all);

/*================================== Public Functions =============================================*/

uint32_t LCD_DrawBitmap(LCD_DeviceType *lcd, uint8_t layer, LCD_BitmapType *bitmap)
{
	uint16_t i;
	uint32_t err;
	LCD_BitmapType real_bitmap;

	LCD_LayerType *draw_layer = NULL;
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
		if (p16_bg != NULL) {
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
	bool use_16_bit;
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
		use_16_bit = true;
		break;
	case LCD_COLOR_FORMAT_ARGB8888:
	case LCD_COLOR_FORMAT_RGB888:
		use_16_bit = false;
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

	if (use_16_bit) {
		p16_bg = (VOLATILE uint16_t *)draw_layer->graphic_writeable;
	}
	else {
		p32_bg = (VOLATILE uint32_t *)draw_layer->graphic_writeable;
	}
	stride = (uint32_t)draw_layer->x_length + draw_layer->x_dummy;
	for (row = rectangle->y; row < bottom; row++) {
		middle_row = (row >= inner_top) && (row < inner_bottom);
		left_end = middle_row && (inner_left < right) ? inner_left : right;
		start = row * stride;
		for (column = rectangle->x; column < left_end; column++) {
			if (use_16_bit) {
				p16_bg[start + column] = (uint16_t)rectangle->color;
			}
			else {
				p32_bg[start + column] = rectangle->color;
			}
		}
		if (middle_row) {
			for (column = inner_right; column < right; column++) {
				if (use_16_bit) {
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
		void *t = lcd->peri_1;
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
	err = lcd->present(lcd);

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
