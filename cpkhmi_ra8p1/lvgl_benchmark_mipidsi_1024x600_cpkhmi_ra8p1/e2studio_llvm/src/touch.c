/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <string.h>
#include "hal_data.h"
#include "touch.h"

/*================================== MACROS =======================================================*/

#ifndef __TOUCH_DEBUG
#define __TOUCH_DEBUG 0
#endif

#if __TOUCH_DEBUG
#include "utils/log.h"
#define TOUCH_LOGD(msg, ...)			LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define TOUCH_LOGI(msg, ...)			LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define TOUCH_LOGW(msg, ...)			LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define TOUCH_LOGE(msg, ...)			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#else
#define TOUCH_LOGD(msg, ...)
#define TOUCH_LOGI(msg, ...)
#define TOUCH_LOGW(msg, ...)
#define TOUCH_LOGE(msg, ...)
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { return v; }
#endif

#define IS_INVALID_POINT(p)				if (p == NULL) { return 1; }

#define I2C_READ_CHAECK(addr, data, len)	do { \
												err = touch->i2c_read(addr, data, len); \
												UNLIKE_RETURN(err, 0, "I2C Read failed: %" PRIu32, err); \
											} while(0)
#define I2C_WRITE_CHECK(addr, data, len)	do { \
												err = touch->i2c_write(addr, data, len); \
												UNLIKE_RETURN(err, 0, "I2C Read failed: %" PRIu32, err); \
											} while(0)

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/
/*================================== Private Functions Prototypes =================================*/

#if TOUCH_EN_LVGL
static void lvgl_touchpad_cb(lv_indev_t * indev_drv, lv_indev_data_t * data);
static void lvgl_touchpad_delete_cb(lv_timer_t *timer);
static void lvgl_touchpad_unregister(lv_indev_t *indev);
#endif

/*================================== Public Functions =============================================*/

uint32_t TOUCH_GetData(TOUCH_DeviceType *touch, TOUCH_DataType *data)
{
	uint8_t i;
	uint8_t count;
	uint8_t rcache[42];
	uint8_t wcache[1];

	uint32_t err = FSP_SUCCESS;
	uint8_t *p8 = NULL;

	if ((touch == NULL) || (data == NULL) || (touch->i2c_read == NULL)) {
		return FSP_ERR_ASSERTION;
	}

	data->ready = false;
	data->num = 0;
	memset(rcache, 0, sizeof(rcache));
	switch (touch->model) {
	case TOUCH_MODEL_FT5X16:
		/* 一次读取触点数和五个触点，避免分次读取跨越控制器的数据更新。 */
		err = touch->i2c_read(0x02, rcache, 29);
		UNLIKE_RETURN(err, 0, "i2c_read failed: %" PRIu32, err);
		count = rcache[0] & 0x0F;
		if (count == 0) {
			data->num = 0;
			data->ready = true;
			TOUCH_LOGD("No touch data");
			break;
		}
		if (count > 5) {
			TOUCH_LOGE("Points more than 5, may be some error occur");
			err = FSP_ERR_INVALID_DATA;
			break;
		}
		for (i = 0; i < count; i++) {
			p8 = &rcache[1 + i * 6];
			data->coor[i].id = p8[2] >> 4;
			data->coor[i].x = p8[0] & 0x0F;
			data->coor[i].x <<= 8;
			data->coor[i].x |= p8[1];
			data->coor[i].y = p8[2] & 0x0F;
			data->coor[i].y <<= 8;
			data->coor[i].y |= p8[3];
			if ((p8[0] >> 6) == 0) {
				data->coor[i].event = TOUCH_EVENT_PUT_DOWN;
			}
			else if ((p8[0] >> 6) == 1) {
				data->coor[i].event = TOUCH_EVENT_PUT_UP;
			}
			else if ((p8[0] >> 6) == 2) {
				data->coor[i].event = TOUCH_EVENT_CONTACT;
			}
			else {
				data->coor[i].event = TOUCH_EVENT_RESERVED;
			}
			data->coor[i].size_avaliable = false;
			data->coor[i].event_avaliable = true;
		}
		data->num = count;
		data->ready = true;
		break;
	case TOUCH_MODEL_GT911:
		data->num = 0;
		data->ready = false;
		if (touch->i2c_write == NULL) {
			return FSP_ERR_ASSERTION;
		}
		err = touch->i2c_read(0x814E, rcache, 41);
		UNLIKE_RETURN(err, 0, "i2c_read failed: %" PRIu32, err);
		/* 未就绪时不解析坐标，也不清除控制器状态。 */
		if ((rcache[0] & 0x80U) == 0U) {
			break;
		}

		count = rcache[0] & 0x0F;
		if (count > touch->max_touch_points) {
			TOUCH_LOGE("Points more than %" PRIu32 ", may be some error occur", touch->max_touch_points);
			err = FSP_ERR_INVALID_DATA;
			break;
		}

		for (i = 0; i < count; i++) {
			p8 = &rcache[1 + i * 8];
			data->coor[i].id = p8[0];
			data->coor[i].x = (uint16_t)((p8[2] << 8) | p8[1]);
			data->coor[i].y = (uint16_t)((p8[4] << 8) | p8[3]);
			data->coor[i].size = (uint16_t)((p8[6] << 8) | p8[5]);

			data->coor[i].size_avaliable = true;
			data->coor[i].event_avaliable = false;
		}

		/* 确认所有触摸数据，未确认则后续收不到触摸数据 */
		wcache[0] = 0;
		err = touch->i2c_write(0x814E, wcache, 1);
		I2C_WRITE_CHECK(0x814E, wcache, 1);
		data->num = count;
		data->ready = true;
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		break;
	}

	return err;
}

uint32_t TOUCH_Init(TOUCH_DeviceType *touch, TOUCH_ConfigType *cfg)
{
	uint8_t rcache[8];
	uint8_t wcache[1];
	uint8_t refresh_period;
	uint16_t t16;
	uint32_t err;

	IS_INVALID_POINT(touch);
	IS_INVALID_POINT(cfg);
	IS_INVALID_POINT(cfg->i2c_read);
	IS_INVALID_POINT(cfg->i2c_write);

	(void)refresh_period;

	touch->sensor_width = 0;
	touch->sensor_height = 0;
	touch->model = cfg->model;
	touch->i2c_read = cfg->i2c_read;
	touch->i2c_write = cfg->i2c_write;

	memset(rcache, 0, sizeof(rcache));
	switch (touch->model) {
	case TOUCH_MODEL_FT5X16:
		I2C_READ_CHAECK(0x00, rcache, 1);
		TOUCH_LOGD("Device mode: 0x%" PRIX32, rcache[0]);
		if (rcache[0] != 0) {
			wcache[0] = 0x00;
			I2C_WRITE_CHECK(0x00, wcache, 1);
		}
		I2C_READ_CHAECK(0x01, rcache, 1);
		TOUCH_LOGD("GEST ID: 0x%" PRIX32, rcache[0]);
		I2C_READ_CHAECK(0xA3, rcache, 1);
		TOUCH_LOGD("Vendor ID: 0x%" PRIX32, rcache[0]);
		I2C_READ_CHAECK(0xA4, rcache, 1);
		TOUCH_LOGD("Interrupt status: 0x%" PRIX32, rcache[0]);
		I2C_READ_CHAECK(0xA6, rcache, 1);
		TOUCH_LOGD("Firmware ID: 0x%" PRIX32, rcache[0]);
		I2C_READ_CHAECK(0xA8, rcache, 1);
		TOUCH_LOGD("CTPM Vendor ID: 0x%" PRIX32, rcache[0]);
		I2C_READ_CHAECK(0xA9, rcache, 1);
		TOUCH_LOGD("Error code: 0x%" PRIX32, rcache[0]);
		break;
	case TOUCH_MODEL_GT911:
		I2C_READ_CHAECK(0x8140, rcache, 4);
		TOUCH_LOGD("Product ID: %s", rcache);
		I2C_READ_CHAECK(0x8144, rcache, 2);
		TOUCH_LOGD("Firmware ID: 0x%02X%02X", rcache[1], rcache[0]);

		I2C_READ_CHAECK(0x8047, rcache, 7);
		TOUCH_LOGD("Current Config Vertion [%c] Info", rcache[0]);
		t16 = rcache[2] << 8 | rcache[1];
		touch->sensor_width = t16;
		TOUCH_LOGD("Max X: %" PRIu32, t16);
		t16 = rcache[4] << 8 | rcache[3];
		touch->sensor_height = t16;
		TOUCH_LOGD("Max Y: %" PRIu32, t16);
		TOUCH_LOGD("Max Output: %" PRIu32, rcache[5] & 0x0F);
		TOUCH_LOGD("XY Wrap: %" PRIu32, (rcache[6] >> 3) & 0x01);
		TOUCH_LOGD("INT trigger: %" PRIu32, rcache[6] & 0x03);
		touch->max_touch_points = rcache[5] & 0x0F;
		if ((touch->sensor_width < 2U) || (touch->sensor_height < 2U) ||
			(touch->max_touch_points == 0U) || (touch->max_touch_points > 5U)) {
			return FSP_ERR_INVALID_DATA;
		}
		I2C_READ_CHAECK(0x8056, rcache, 1);
		refresh_period = (rcache[0] & 0x0F) + 5;
		TOUCH_LOGD("Refresh rate: %" PRIu32 "ms", refresh_period);
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

#if TOUCH_EN_FREERTOS
	touch->sem = xSemaphoreCreateBinary();
	if (touch->sem == NULL) {
		return FSP_ERR_OUT_OF_MEMORY;
	}
#else
	touch->sem = false;
#endif

	return 0;
}

/**
 * @brief	在外部中断函数中调用，以确定收到触摸数据
 * @param	touch TOUCH_DeviceType 实例
 * @note	GT911 和 FT5X16 发送脉冲的逻辑不一致：
 * 			- GT911: 按寄存器设置固定周期发送，即使没有触摸事件发生
 * 			- FT5X16: 按下、松手时发送
 * 			因此 GT911 可以不使用此函数，而使用轮询处理
 */
uint32_t TOUCH_ExternalIRQCb(TOUCH_DeviceType *touch)
{
#if TOUCH_EN_FREERTOS
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	xSemaphoreGiveFromISR(touch->sem, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#else
	touch->sem = true;
#endif

	return 0;
}

#if TOUCH_EN_LVGL
uint32_t TOUCH_RegisterLVGLTouchpad(TOUCH_DeviceType *touch, uint32_t period)
{
	if ((touch == NULL) || (lv_display_get_default() == NULL)) {
		return FSP_ERR_ASSERTION;
	}
#if TOUCH_EN_FREERTOS
	if (touch->sem == NULL) {
		return FSP_ERR_ASSERTION;
	}
#endif
	touch->last_update_tick = lv_tick_get();
	touch->indev = lv_indev_create();
	if (touch->indev == NULL) {
		TOUCH_LOGE("lv_indev_create() failed: can't malloc memory");
		return FSP_ERR_OUT_OF_MEMORY;
	}
	lv_indev_set_type(touch->indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(touch->indev, lvgl_touchpad_cb);
	lv_indev_set_user_data(touch->indev, touch);

	/* 单独设置输入采样周期 */
	if (period != 0) {
		lv_timer_set_period(lv_indev_get_read_timer(touch->indev), period);
	}

	return 0;
}
#endif

/*================================== Private Functions ============================================*/

#if TOUCH_EN_LVGL
static void lvgl_touchpad_cb(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
	uint32_t err;
	TOUCH_DataType touch_data = {0};
	TOUCH_DeviceType *touch = (TOUCH_DeviceType *)lv_indev_get_user_data(indev_drv);

	/* LVGL 已将 point 初始化为上次原始坐标，仅需保留设备状态。 */
	data->state = lv_indev_get_state(indev_drv);
	if (touch == NULL) {
		data->state = LV_INDEV_STATE_RELEASED;
		return;
	}

	switch (touch->model) {
	case TOUCH_MODEL_FT5X16:
	#if TOUCH_EN_FREERTOS
		if (touch->sem == NULL) {
			data->state = LV_INDEV_STATE_RELEASED;
			return;
		}
		/* FT5X16 在长按滑动时不会触发 IRQ，如果 LVGL 的上一次状态是按下，则要继续读取，以确定坐标是否变化，
		 * 即 FT5X16 的 TOUCH_EVENT_CONTACT */
		if ((xSemaphoreTake(touch->sem, 0) != pdTRUE) && (data->state != LV_INDEV_STATE_PRESSED)) {
			return;
		}
	#else
		if (!touch->sem && (data->state != LV_INDEV_STATE_PRESSED)) {
			return;
		}
		touch->sem = false;
	#endif
		err = TOUCH_GetData(touch, &touch_data);
		data->state = LV_INDEV_STATE_RELEASED;
		if (err != FSP_SUCCESS) {
			TOUCH_LOGE("TOUCH_GetData() failed: %" PRIu32 ", unregister input device", err);
			lvgl_touchpad_unregister(indev_drv);
			return;
		}
		if (touch_data.num == 0) {
			break;
		}
		switch (touch_data.coor[0].event) {
		case TOUCH_EVENT_PUT_DOWN:
		case TOUCH_EVENT_CONTACT:
			data->state = LV_INDEV_STATE_PRESSED;
			data->point.x = touch_data.coor[0].x;
			data->point.y = touch_data.coor[0].y;
			break;
		case TOUCH_EVENT_PUT_UP:
		case TOUCH_EVENT_RESERVED:
		default:
			break;
		}
		break;
	case TOUCH_MODEL_GT911:
	{
		uint32_t now;

		err = TOUCH_GetData(touch, &touch_data);
		if (err != FSP_SUCCESS) {
			TOUCH_LOGE("TOUCH_GetData() failed: %" PRIu32 ", unregister input device", err);
			data->state = LV_INDEV_STATE_RELEASED;
			lvgl_touchpad_unregister(indev_drv);
			return;
		}
		now = lv_tick_get();
		if (!touch_data.ready) {
			/* 暂无新帧时保持状态，超过参考工程的 150 ms 超时则释放。 */
			if ((now - touch->last_update_tick) >= 150) {
				data->state = LV_INDEV_STATE_RELEASED;
			}
			return;
		}
		touch->last_update_tick = now;
		data->state = LV_INDEV_STATE_RELEASED;
		if (touch_data.num == 0) {
			break;
		}

		/* 当前模组屏幕和触摸均为 1024x600，可以直接分别反转 X、Y
		 * 如果不相等，则要么软件进行缩放，或者改 GT911 的配置使其与屏幕分辨率相等 */
		data->point.x = 1023 - (int32_t)touch_data.coor[0].x;
		data->point.y = 599 - (int32_t)touch_data.coor[0].y;
		data->state = LV_INDEV_STATE_PRESSED;
		break;
	}
	default:
		data->state = LV_INDEV_STATE_RELEASED;
		break;
	}
}

static void lvgl_touchpad_unregister(lv_indev_t *indev)
{
	TOUCH_DeviceType *touch = (TOUCH_DeviceType *)lv_indev_get_user_data(indev);

	lv_indev_enable(indev, false);
	lv_indev_set_read_cb(indev, NULL);
	touch->indev = NULL;
	/* 读取回调返回后 LVGL 仍访问 indev，复用读取定时器完成删除。 */
	lv_timer_set_cb(lv_indev_get_read_timer(indev), lvgl_touchpad_delete_cb);
	lv_timer_ready(lv_indev_get_read_timer(indev));
}

static void lvgl_touchpad_delete_cb(lv_timer_t *timer)
{
	lv_indev_t *indev = (lv_indev_t *)lv_timer_get_user_data(timer);

	/* lv_indev_delete 同时删除该设备的读取定时器。 */
	lv_indev_delete(indev);
}
#endif
