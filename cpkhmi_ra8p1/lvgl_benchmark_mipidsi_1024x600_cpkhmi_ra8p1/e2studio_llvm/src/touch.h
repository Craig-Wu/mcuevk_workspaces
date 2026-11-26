#ifndef __TOUCH_H
#define __TOUCH_H

#ifndef TOUCH_EN_FREERTOS
#define TOUCH_EN_FREERTOS		1
#endif

#ifndef TOUCH_EN_LVGL
#define TOUCH_EN_LVGL			1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if TOUCH_EN_FREERTOS
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "semphr.h"
#endif

#if TOUCH_EN_LVGL
#include "lvgl.h"
#endif

typedef enum {
	TOUCH_EVENT_PUT_DOWN,
	TOUCH_EVENT_PUT_UP,
	TOUCH_EVENT_CONTACT,
	TOUCH_EVENT_RESERVED
} TOUCH_EventEnum;

typedef enum {
	TOUCH_MODEL_UNKNOW,
	TOUCH_MODEL_FT5X16,
	TOUCH_MODEL_GT911,
} TOUCH_ModelEnum;

typedef struct {
	TOUCH_ModelEnum model;
	uint32_t (*i2c_read)(uint16_t reg_addr, uint8_t *data, uint8_t len);
	uint32_t (*i2c_write)(uint16_t reg_addr, uint8_t *data, uint8_t len);
} TOUCH_ConfigType;

typedef struct {
	bool size_avaliable;
	bool event_avaliable;
	uint8_t id;
	uint16_t x;
	uint16_t y;
	uint16_t size;
	TOUCH_EventEnum event;
} TOUCH_CoordinateType;

typedef struct {
	bool ready; /* 有效新帧，包括触点数为 0 的松手帧。 */
	uint8_t num;
	TOUCH_CoordinateType coor[5];
} TOUCH_DataType;

typedef struct {
	TOUCH_ModelEnum model;
	uint8_t max_touch_points;
	uint16_t sensor_width;
	uint16_t sensor_height;

#if TOUCH_EN_FREERTOS
	SemaphoreHandle_t sem;
#else
	bool sem;
#endif

#if TOUCH_EN_LVGL
	lv_indev_t *indev;
	uint32_t last_update_tick;
#endif

	uint32_t (*i2c_read)(uint16_t reg_addr, uint8_t *data, uint8_t len);
	uint32_t (*i2c_write)(uint16_t reg_addr, uint8_t *data, uint8_t len);
} TOUCH_DeviceType;

uint32_t TOUCH_GetData(TOUCH_DeviceType *touch, TOUCH_DataType *data);
uint32_t TOUCH_Init(TOUCH_DeviceType *touch, TOUCH_ConfigType *cfg);
uint32_t TOUCH_ExternalIRQCb(TOUCH_DeviceType *touch);

#if TOUCH_EN_LVGL
uint32_t TOUCH_RegisterLVGLTouchpad(TOUCH_DeviceType *touch, uint32_t period);
#endif

#ifdef __cplusplus
}
#endif

#endif
