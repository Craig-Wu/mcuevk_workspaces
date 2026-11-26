#ifndef __USB_THREAD_H
#define __USB_THREAD_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "i2c.h"

#define USB_THREAD_DATA_QUEUE_LEN				16

#define USB_THREAD_MSG_ERROR_EXIT				(0x01 << 0)
#define USB_THREAD_MSG_KEYBOARD_ATTACH			(0x01 << 1)
#define USB_THREAD_MSG_KEYBOARD_DEATTACH		(0x01 << 2)
#define USB_THREAD_MSG_MOUSE_ATTACH				(0x01 << 3)
#define USB_THREAD_MSG_MOUSE_DEATTACH			(0x01 << 4)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	I2C_DeviceType *i2c_device;
	SemaphoreHandle_t i2c_sem;
} USB_THREAD_ParamType;

extern EventGroupHandle_t g_usb_thread_event;
extern QueueHandle_t g_usb_data_queue;
extern TaskHandle_t g_usb_thread_handle;
	
void USB_Thread(void *param);
	
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __USB_THREAD_H */
