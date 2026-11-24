#ifndef _COMMON_H
#define _COMMON_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define USB_DATA_QUEUE_LEN	16
#define USB_DATA_QUEUE_SIZE	8

#ifdef __cplusplus
extern "C" {
#endif

extern TaskHandle_t g_usb_data_display_handle;
extern QueueHandle_t g_usb_data_queue;

void USBDataDisplayThread(void *param);

#ifdef __cplusplus
}
#endif

#endif
