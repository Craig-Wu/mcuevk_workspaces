#include "common.h"
#include "console.h"
#include "i2c.h"
#include "main_thread.h"
#include "usb_hid_code.h"

#include "fusb303b/fusb303b.h"
#include "utils/log.h"

#ifndef __MAIN_THREAD_DEBUG
#define __MAIN_THREAD_DEBUG	0
#endif

#if __MAIN_THREAD_DEBUG

#define LOGD(msg, ...)		LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGI(msg, ...)		LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGW(msg, ...)		LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGE(msg, ...)		LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)

#define HANDLE_ERR(e, msg, ...)														\
	do { 																			\
		if (e != FSP_SUCCESS) { 													\
			vTaskSuspendAll(); 														\
			if (msg != NULL) { 														\
				LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); 							\
			} 																		\
			while (1) { 															\
				R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW); 	\
				R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS); 			\
				R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);	\
				R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS); 			\
			} 																		\
		} 																			\
	} while(0)

#define HANDLE_USB_ERR(e, msg, ...)													\
	if (e != FSP_SUCCESS) { 														\
		if (msg != NULL) { 															\
			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); 								\
		} 																			\
		e = R_USB_Close(g_usbfs_hhid_basic.p_ctrl); 								\
		if (e != FSP_SUCCESS) { 													\
			LOG_E(__FUNCTION__, "R_USB_Close() failed: %u", e); 					\
		} 																			\
		break;																		\
	}

#else

#define LOGD(msg, ...)
#define LOGI(msg, ...)
#define LOGW(msg, ...)
#define LOGE(msg, ...)

#define HANDLE_ERR(e, msg, ...)														\
	do { 																			\
		if (e != FSP_SUCCESS) { 													\
			vTaskSuspendAll(); 														\
			while (1) { 															\
				R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW); 	\
				R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS); 			\
				R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);	\
				R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS); 			\
			} 																		\
		} 																			\
	} while(0)

#define HANDLE_USB_ERR(e, msg, ...)													\
	if (e != FSP_SUCCESS) { 														\
		R_USB_Close(g_usbfs_hhid_basic.p_ctrl); 									\
		break;																		\
	}

#endif /* #if __MAIN_THREAD_DEBUG */

extern uint8_t *g_p_usb_hhid_interface_table[USB_NUM_USBIP];
extern TaskHandle_t main_thread;

TaskHandle_t g_usb_data_display_handle;
QueueHandle_t g_usb_data_queue;

static uint8_t s_data_buf[USB_DATA_QUEUE_SIZE];
static uint8_t s_setup_buf[8];
static uint16_t s_max_packet_size;
static I2C_DeviceType s_i2c0;
static I2C_ConfigType s_i2c0_cfg;
static QueueHandle_t s_usb_event_queue;
static TaskHandle_t s_led_thread_handle;

static void ledThread(void *param);
static bool usbUpdateLEDStatus(uint8_t *data, bool *num_lock, bool *caps_lock);
static void usbWaitAttach(void);

void main_thread_entry(void *pvParameters)
{
	bool attached;
	bool num_lock;
	bool caps_lock;
	bool should_update;
	uint32_t err;
	BaseType_t os_err;
	usb_setup_t setup;
	FUSB303_Status status;

	usb_event_info_t *usb_event = NULL;

	FSP_PARAMETER_NOT_USED(pvParameters);

	CONSOLE_Init();
	puts("\x1b[2J\x1b[H");
    puts("\x1B[0m");
    printf("FSP Version: %s\r\n", FSP_VERSION_STRING);
    xTaskCreate(ledThread, "ledThread", 0x400, NULL, 2, &s_led_thread_handle);

    s_i2c0_cfg.inst = &g_i2c_master0;
    s_i2c0_cfg.model = I2C_MODEL_IIC_MASTER;
    I2C_Init(&s_i2c0, &s_i2c0_cfg);

    R_ICU_ExternalIrqOpen(g_external_irq24.p_ctrl, g_external_irq24.p_cfg);
    R_ICU_ExternalIrqEnable(g_external_irq24.p_ctrl);
    err = FUSB303B_Init();
    HANDLE_ERR(err, "FUSB303B_Init() failed: %u", err);
    usbWaitAttach();
    R_IOPORT_PinWrite(g_ioport.p_ctrl, BSP_IO_PORT_04_PIN_07, BSP_IO_LEVEL_HIGH);
    attached = true;

    s_usb_event_queue = xQueueCreate(USB_EVENT_MAX, 4);
    g_usb_data_queue = xQueueCreate(USB_DATA_QUEUE_LEN, USB_DATA_QUEUE_SIZE);
    xTaskCreate(USBDataDisplayThread, "usbDataDisplay", 0x400, NULL, 2, &g_usb_data_display_handle);
    err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
    HANDLE_ERR(err, "R_USB_Open() failed: %u", err);

	while (1) {
		if (attached) {
			os_err = xQueueReceive(s_usb_event_queue, &usb_event, portMAX_DELAY);
			if (os_err != pdTRUE) {
				LOGE("xQueueReceive() failed");
				continue;
			}
		}
		else {
			usbWaitAttach();
			R_IOPORT_PinWrite(g_ioport.p_ctrl, BSP_IO_PORT_04_PIN_07, BSP_IO_LEVEL_HIGH);
			attached = true;
			err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
			HANDLE_ERR(err, "R_USB_Open() failed: %u", err);
		}

		switch (usb_event->event) {
		case USB_STATUS_CONFIGURED:
			err = R_USB_HHID_MaxPacketSizeGet(g_usbfs_hhid_basic.p_ctrl, &s_max_packet_size, USB_HID_IN, usb_event->device_address);
			HANDLE_USB_ERR(err, "R_USB_HHID_MaxPacketSizeGet() failed: %u", err);
			LOGD("s_max_packet_size = %u", s_max_packet_size);

			setup.request_type = (USB_HID_SET_PROTOCOL | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE);
			setup.request_value = 0;
			setup.request_index = 0;
			setup.request_length = 0;
			err = R_USB_HostControlTransfer(g_usbfs_hhid_basic.p_ctrl, &setup, s_setup_buf, usb_event->device_address);
			HANDLE_USB_ERR(err, "R_USB_HostControlTransfer() failed int configured: %u", err);
			puts("Keyboard configuration done, press key and it will display");
			break;
		case USB_STATUS_DETACH:
			printf("USB Detach");
			R_IOPORT_PinWrite(g_ioport.p_ctrl, BSP_IO_PORT_04_PIN_07, BSP_IO_LEVEL_LOW);
			attached = false;
			break;
		case USB_STATUS_REQUEST_COMPLETE:
			LOGD("usb_event->status = %u", usb_event->status);
			if (usb_event->setup.request_type & USB_BREQUEST) {
				err = R_USB_Read(g_usbfs_hhid_basic.p_ctrl, s_data_buf, s_max_packet_size, usb_event->device_address);
				HANDLE_USB_ERR(err, "R_USB_Read() failed after REQUEST_COMPLETE: %u", err);
			}
			break;
		case USB_STATUS_READ_COMPLETE:
			s_setup_buf[0] = 0x00;
			should_update = usbUpdateLEDStatus(s_data_buf, &num_lock, &caps_lock);
			/* Byte 1 is reserved, using this deliver cap_lock and num_lock info to usb_data_display_thread */
			if (num_lock) {
				s_data_buf[1] |= 0x01;
				s_setup_buf[0] |= 0x01;
			}
			if (caps_lock) {
				s_data_buf[1] |= 0x02;
				s_setup_buf[0] |= 0x02;
			}
			os_err = xQueueSend(g_usb_data_queue, s_data_buf, 0);
			if (os_err != pdTRUE) {
				LOGE("xQueueSend() failed");
			}

			if (should_update) {
				LOGD("s_setup_buf[0] = 0x%X", s_setup_buf[0]);
				setup.request_type = (USB_HID_SET_REPORT | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE);
				setup.request_value = 0x0200;
				setup.request_index = 0;
				setup.request_length = 1;
				err = R_USB_HostControlTransfer(g_usbfs_hhid_basic.p_ctrl, &setup, s_setup_buf, usb_event->device_address);
				HANDLE_USB_ERR(err, "R_USB_HostControlTransfer() failed in update LED: %u", err);
			}
			else {
				err = R_USB_Read(g_usbfs_hhid_basic.p_ctrl, s_data_buf, s_max_packet_size, usb_event->device_address);
				if (err) {
					vTaskDelay(200);
					FUSB303B_Check(&status);
					if (status.attached && (status.type == DETECT_TYPE_SOURCE)) {
						puts("Maybe you detach keyboard but not detach OTG cable, you can reconnect keyboard");
						err = R_USB_Close(g_usbfs_hhid_basic.p_ctrl);
						HANDLE_USB_ERR(err, "R_USB_Close() failed after Read failed: %u", err);
						err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
						HANDLE_ERR(err, "R_USB_Open() failed: %u", err);
					}
					else if (status.attached == 0) {
						puts("OTG cable and keyboard deatch, you can reconnect keyboard");
						R_IOPORT_PinWrite(g_ioport.p_ctrl, BSP_IO_PORT_04_PIN_07, BSP_IO_LEVEL_LOW);
						attached = false;
						err = R_USB_Close(g_usbfs_hhid_basic.p_ctrl);
						HANDLE_USB_ERR(err, "R_USB_Close() failed after Read failed: %u", err);
					}
					else {
						HANDLE_USB_ERR(err, "R_USB_Read() failed: %u", err);
					}
				}
			}
			break;
		default:
			LOGD("Uncheck event: %u", usb_event->event);
			break;
		}
	}
}

uint32_t FUSB303B_Read(uint8_t reg, uint8_t *data, uint8_t length)
{
	uint32_t err;
	I2C_WRMemParamType param;

	param.slave = FUSB303B_ADDR;
	param.mem_addr = reg;
	param.mem_width = 8;
	param.data = data;
	param.length = length;

	err = I2C_ReadMem(&s_i2c0, &param);
	if (err == FSP_SUCCESS) {
		err = I2C_WaitOperation(&s_i2c0, I2C_OP_ALL, 100);
	}

	return err;
}

uint32_t FUSB303B_Write(uint8_t *data, uint8_t length)
{
	uint32_t err;
	I2C_WRMemParamType param;

	param.slave = FUSB303B_ADDR;
	param.data = data;
	param.length = length;

	err = I2C_Write(&s_i2c0, &param);
	if (err == FSP_SUCCESS) {
		err = I2C_WaitOperation(&s_i2c0, I2C_OP_TX_DONE, 100);
	}

	return err;
}

void ExternalIRQ_Callback(external_irq_callback_args_t *p_args)
{
	BaseType_t xHigherPriorityTaskWoken;

	if (p_args->channel == 24) {
		LOGD("Get IRQ");
		xTaskNotifyFromISR(main_thread, 0x01, eSetBits, &xHigherPriorityTaskWoken);
	}
}

void USBFS_HHID_Callback(usb_event_info_t *event, usb_hdl_t hdl, usb_onoff_t on_off)
{
	BaseType_t os_err;

	(void)hdl;
	(void)on_off;

	os_err = xQueueSend(s_usb_event_queue, &event, 0);
	if (os_err != pdPASS) {
		LOGE("USB event queue full");
	}
}

#if LOG_CFG_EN_TIMESTAMP
void LOG_GetTime(uint32_t *s, uint32_t *ms)
{
	TickType_t tick;

	if (__get_IPSR() == 0) {
		tick = xTaskGetTickCount();
	}
	else {
		tick = xTaskGetTickCountFromISR();
	}

	*s = tick / 1000;
	*ms = tick % 1000;
}
#endif

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

static bool usbUpdateLEDStatus(uint8_t *data, bool *num_lock, bool *caps_lock)
{
	static bool caps_lock_on = false;
	static bool num_lock_on = false;

	uint32_t i;

	bool caps_lock_press = false;
	bool num_lock_press = false;
	bool should_update = false;

	for (i = 2; i < 8; i++) {
		if (data[i] == HID_BP_KEYBOARD_CAPS_LOCK) {
			caps_lock_press = true;
		}
		else if (data[i] == HID_BP_KEYPAD_NUM_LOCK_AND_CLEAR) {
			num_lock_press = true;
		}
	}

	if (caps_lock_press || num_lock_press) {
		should_update = true;
		if (caps_lock_press) {
			caps_lock_on = caps_lock_on ? false : true;
		}
		if (num_lock_press) {
			num_lock_on = num_lock_on ? false : true;
		}
	}
	*num_lock = num_lock_on;
	*caps_lock = caps_lock_on;

	return should_update;
}

static void usbWaitAttach(void)
{
	FUSB303_Status status;

	xTaskNotifyWait(0x00, 0x01, NULL, portMAX_DELAY);
	FUSB303B_Check(&status);
	while ((status.attached == 0) && (status.type != DETECT_TYPE_SOURCE)) {
		xTaskNotifyWait(0x00, 0x01, NULL, portMAX_DELAY);
		FUSB303B_Check(&status);
	}
	puts("USB Attach");
}
