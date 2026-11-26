#include <inttypes.h>
#include <stdbool.h>

#include "hal_data.h"
#include "i2c.h"
#include "usb_hid_code.h"
#include "usb_thread.h"

#include "fusb303b/fusb303b.h"
#include "utils/log.h"

#ifndef __USB_THREAD_DEBUG
#define __USB_THREAD_DEBUG	0
#endif

#if __USB_THREAD_DEBUG

#define LOGD(msg, ...)		LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGI(msg, ...)		LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGW(msg, ...)		LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGE(msg, ...)		LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)

#define CHECK_THREAD_PARAM_POINT(p, msg, ...)										\
	do {																			\
		if (p == NULL) {															\
			if (msg != NULL) {														\
				LOG_E(__FUNCTION__, msg, ##__VA_ARGS__);							\
			}																		\
			xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_ERROR_EXIT);		\
			vTaskDelete(g_usb_thread_handle);										\
		}																			\
	} while(0)

#define HANDLE_ERR(e, msg, ...)														\
	do { 																			\
		if (e != FSP_SUCCESS) { 													\
			if (msg != NULL) { 														\
				LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); 							\
			} 																		\
			xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_ERROR_EXIT);		\
			vTaskDelete(g_usb_thread_handle);										\
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

#define CHECK_THREAD_PARAM_POINT(p, msg, ...)										\
	do {																			\
		if (p == NULL) {															\
			xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_ERROR_EXIT);		\
			vTaskDelete(g_usb_thread_handle);										\
		}																			\
	} while(0)
	
#define HANDLE_ERR(e, msg, ...)														\
	do { 																			\
		if (e != FSP_SUCCESS) { 													\
			xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_ERROR_EXIT);		\
			vTaskDelete(g_usb_thread_handle);										\
		} 																			\
	} while(0)

#define HANDLE_USB_ERR(e, msg, ...)													\
	if (e != FSP_SUCCESS) { 														\
		R_USB_Close(g_usbfs_hhid_basic.p_ctrl); 									\
		break;																		\
	}

#endif /* #if __USB_THREAD_DEBUG */

extern uint8_t *g_p_usb_hhid_device_table[USB_NUM_USBIP];
extern uint8_t *g_p_usb_hhid_config_table[USB_NUM_USBIP];
extern uint8_t *g_p_usb_hhid_interface_table[USB_NUM_USBIP];

EventGroupHandle_t g_usb_thread_event;
QueueHandle_t g_usb_data_queue;
TaskHandle_t g_usb_thread_handle;

static uint8_t s_setup_buf[64];
static uint16_t s_max_packet_size;
static USB_THREAD_ParamType *s_thread_param = NULL;
static QueueHandle_t s_usb_event_queue;
static USB_HID_QueueContentType s_hid_queue_content;

static void sendDeattachEvent(uint8_t device_type);
static void usbCheckDeviceType(uint8_t *device_type);
static bool usbUpdateLEDStatus(uint8_t *data, bool *num_lock, bool *caps_lock);
static void usbWaitAttach(void);

#if __USB_THREAD_DEBUG
static void debugOutputDeviceDescriptor(const char *tag);
static void debugOutputConfigurationDescriptor(const char *tag);
static void debugOutputInterfaceDescriptor(const char *tag);
static void debugDecodeInterfaceDescriptor(uint8_t *data);
#else
#define debugOutputDeviceDescriptor(tag)
#define debugOutputConfigurationDescriptor(tag)
#define debugOutputInterfaceDescriptor(tag)
#endif

void USB_Thread(void *param)
{
	bool attached;
	bool num_lock;
	bool caps_lock;
	bool should_update;
	uint8_t device_type;
	uint16_t request_type;
	uint32_t err;
	BaseType_t os_err;
	usb_setup_t setup;
	FUSB303_Status status;
	
	const char *device_name = "Unknow";
	uint8_t request_index = 0;
	usb_event_info_t *usb_event = NULL;
	
	if (g_usb_thread_event == NULL) {
		g_usb_thread_event = xEventGroupCreate();
	}
	if (g_usb_data_queue == NULL) {
		g_usb_data_queue = xQueueCreate(USB_THREAD_DATA_QUEUE_LEN, sizeof(USB_HID_QueueContentType));
	}
	if (s_usb_event_queue == NULL) {
		s_usb_event_queue = xQueueCreate(USB_EVENT_MAX, 4);
	}
	
	CHECK_THREAD_PARAM_POINT(param, "param is NULL");
	s_thread_param = (USB_THREAD_ParamType *)param;
	CHECK_THREAD_PARAM_POINT(s_thread_param->i2c_device, "i2c_device is NULL");
	CHECK_THREAD_PARAM_POINT(s_thread_param->i2c_sem, "i2c_sem is NULL");
	
	R_ICU_ExternalIrqOpen(g_external_irq24.p_ctrl, g_external_irq24.p_cfg);
    R_ICU_ExternalIrqEnable(g_external_irq24.p_ctrl);
	err = FUSB303B_Init();
	HANDLE_ERR(err, "FUSB303B_Init() failed: %" PRIu32, err);
	usbWaitAttach();
	R_USB_VbusSet(g_usbfs_hhid_basic.p_ctrl, USB_ON);
	attached = true;
	
	err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
    HANDLE_ERR(err, "R_USB_Open() failed: %" PRIu32, err);
	
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
			R_USB_VbusSet(g_usbfs_hhid_basic.p_ctrl, USB_ON);
			attached = true;
			err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
			HANDLE_ERR(err, "R_USB_Open() failed: %" PRIu32, err);
		}

		switch (usb_event->event) {
		case USB_STATUS_CONFIGURED:
			err = R_USB_HHID_MaxPacketSizeGet(g_usbfs_hhid_basic.p_ctrl, &s_max_packet_size, USB_HID_IN, usb_event->device_address);
			HANDLE_USB_ERR(err, "R_USB_HHID_MaxPacketSizeGet() failed: %" PRIu32, err);

			request_index = g_p_usb_hhid_interface_table[g_usbfs_hhid_basic_cfg.module_number][2];
			debugOutputDeviceDescriptor(__FUNCTION__);
			debugOutputConfigurationDescriptor(__FUNCTION__);
			debugOutputInterfaceDescriptor(__FUNCTION__);

			usbCheckDeviceType(&device_type);
			switch (device_type) {
			case 0x01:
				printf("Connected to a Keyboard\r\n");
				device_name = "Keyboard";
				xEventGroupClearBits(g_usb_thread_event, USB_THREAD_MSG_KEYBOARD_DEATTACH);
				xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_KEYBOARD_ATTACH);
				break;
			case 0x02:
				printf("Connected to a Mouse\r\n");
				device_name = "Mouse";
				xEventGroupClearBits(g_usb_thread_event, USB_THREAD_MSG_MOUSE_DEATTACH);
				xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_MOUSE_ATTACH);
				break;
			default:
				printf("Connected to a Unknow device\r\n");
				break;
			}
			setup.request_type = (USB_HID_GET_PROTOCOL | USB_DEV_TO_HOST | USB_CLASS | USB_INTERFACE);
			setup.request_value = 0;
			setup.request_index = request_index;
			setup.request_length = 1;
			err = R_USB_HostControlTransfer(g_usbfs_hhid_basic.p_ctrl, &setup, s_setup_buf, usb_event->device_address);
			HANDLE_USB_ERR(err, "R_USB_HostControlTransfer() failed when GET_PROTOCOL: %" PRIu32, err);
			break;
		case USB_STATUS_DETACH:
			printf("USB Detach");
			R_USB_VbusSet(g_usbfs_hhid_basic.p_ctrl, USB_OFF);
			attached = false;
			sendDeattachEvent(device_type);
			break;
		case USB_STATUS_REQUEST_COMPLETE:
			LOGD("Request status: %" PRIu32, usb_event->status);
			request_type = usb_event->setup.request_type & USB_BREQUEST;
			switch (request_type) {
			case USB_HID_GET_PROTOCOL:
				if (usb_event->status == USB_SETUP_STATUS_STALL) {
					err = FSP_ERR_USB_FAILED;
					HANDLE_USB_ERR(err, "STALL when request GET_REPORT");
				}
				LOGD("Receive data size: %" PRIu32, usb_event->data_size);
				if (s_setup_buf[0] == 0x00) {
					LOGD("Current already in boot protocol");
					err = R_USB_Read(g_usbfs_hhid_basic.p_ctrl, s_hid_queue_content.data, s_max_packet_size, usb_event->device_address);
					HANDLE_USB_ERR(err, "R_USB_Read() failed after GET_REPORT: %" PRIu32, err);
					printf("%s configuration done\r\n", device_name);
				}
				else if (s_setup_buf[0] == 0x01) {
					LOGD("Current in report protocol, try into boot protocol");
					setup.request_type = (USB_HID_SET_PROTOCOL | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE);
					setup.request_value = 0;
					setup.request_index = request_index;
					setup.request_length = 0;
					err = R_USB_HostControlTransfer(g_usbfs_hhid_basic.p_ctrl, &setup, s_setup_buf, usb_event->device_address);
					HANDLE_USB_ERR(err, "R_USB_HostControlTransfer() failed in configured: %" PRIu32, err);
				}
				else {
					err = FSP_ERR_USB_FAILED;
					HANDLE_USB_ERR(err, "Unknow protocol: 0x%02X", s_setup_buf[0]);
				}
				break;
			case USB_HID_SET_REPORT:
			case USB_HID_SET_PROTOCOL:
				err = R_USB_Read(g_usbfs_hhid_basic.p_ctrl, s_hid_queue_content.data, s_max_packet_size, usb_event->device_address);
				HANDLE_USB_ERR(err, "R_USB_Read() failed after SET_PROTOCOL: %" PRIu32, err);
				printf("%s configuration done\r\n", device_name);
				break;
			default:
				LOGW("Uncheck request complete: 0x%04X", request_type);
				break;
			}
			break;
		case USB_STATUS_READ_COMPLETE:
			s_setup_buf[0] = 0x00;
			/* This is keyboard */
			if (device_type == 0x01) {
				should_update = usbUpdateLEDStatus(s_hid_queue_content.data, &num_lock, &caps_lock);
				s_hid_queue_content.device_type = USB_HID_TYPE_KEYBOARD;
				/* When using keyboard
				 * flag[0]: Num Lock
				 * flag[1]: Caps Lock */
				if (num_lock) {
					s_hid_queue_content.flag |= 0x01;
					s_setup_buf[0] |= 0x01;
				}
				else {
					s_hid_queue_content.flag &= (uint32_t)(~0x01);
				}
				if (caps_lock) {
					s_hid_queue_content.flag |= 0x02;
					s_setup_buf[0] |= 0x02;
				}
				else {
					s_hid_queue_content.flag &= (uint32_t)(~0x02);
				}
			}
			else if (device_type == 0x02) {
				s_hid_queue_content.device_type = USB_HID_TYPE_MOUSE;
			}

			os_err = xQueueSend(g_usb_data_queue, &s_hid_queue_content, 0);
			if (os_err != pdTRUE) {
				LOGE("g_usb_data_queue full");
			}

			if (should_update) {
				LOGD("s_setup_buf[0] = 0x%X", s_setup_buf[0]);
				setup.request_type = (USB_HID_SET_REPORT | USB_HOST_TO_DEV | USB_CLASS | USB_INTERFACE);
				setup.request_value = 0x0200;
				setup.request_index = request_index;
				setup.request_length = 1;
				err = R_USB_HostControlTransfer(g_usbfs_hhid_basic.p_ctrl, &setup, s_setup_buf, usb_event->device_address);
				HANDLE_USB_ERR(err, "R_USB_HostControlTransfer() failed in update LED: %" PRIu32, err);
			}
			else {
				err = R_USB_Read(g_usbfs_hhid_basic.p_ctrl, s_hid_queue_content.data, s_max_packet_size, usb_event->device_address);
				if (err) {
					vTaskDelay(200);
					FUSB303B_Check(&status);
					if (status.attached && (status.type == DETECT_TYPE_SOURCE)) {
						printf("Maybe you detach %s but not detach OTG cable, you can reconnect\r\n", device_name);
						sendDeattachEvent(device_type);
						err = R_USB_Close(g_usbfs_hhid_basic.p_ctrl);
						HANDLE_USB_ERR(err, "R_USB_Close() failed after Read failed: %" PRIu32, err);
						err = R_USB_Open(g_usbfs_hhid_basic.p_ctrl, g_usbfs_hhid_basic.p_cfg);
						HANDLE_ERR(err, "R_USB_Open() failed: %" PRIu32, err);
					}
					else if (status.attached == 0) {
						printf("OTG cable and %s deatch, you can reconnect\r\n", device_name);
						R_USB_VbusSet(g_usbfs_hhid_basic.p_ctrl, USB_OFF);
						attached = false;
						sendDeattachEvent(device_type);
						err = R_USB_Close(g_usbfs_hhid_basic.p_ctrl);
						HANDLE_USB_ERR(err, "R_USB_Close() failed after Read failed: %" PRIu32, err);
					}
					else {
						HANDLE_USB_ERR(err, "R_USB_Read() failed: %" PRIu32, err);
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

	xSemaphoreTake(s_thread_param->i2c_sem, portMAX_DELAY);
	err = I2C_ReadMem(s_thread_param->i2c_device, &param);
	if (err == FSP_SUCCESS) {
		err = I2C_WaitOperation(s_thread_param->i2c_device, I2C_OP_ALL, 100);
	}
	xSemaphoreGive(s_thread_param->i2c_sem);

	return err;
}

uint32_t FUSB303B_Write(uint8_t *data, uint8_t length)
{
	uint32_t err;
	I2C_WRMemParamType param;

	param.slave = FUSB303B_ADDR;
	param.data = data;
	param.length = length;

	xSemaphoreTake(s_thread_param->i2c_sem, portMAX_DELAY);
	err = I2C_Write(s_thread_param->i2c_device, &param);
	if (err == FSP_SUCCESS) {
		err = I2C_WaitOperation(s_thread_param->i2c_device, I2C_OP_TX_DONE, 100);
	}
	xSemaphoreGive(s_thread_param->i2c_sem);

	return err;
}

void ExternalIRQ_Callback(external_irq_callback_args_t *p_args)
{
	BaseType_t xHigherPriorityTaskWoken;
	
	if (p_args->channel == 24) {
		xTaskNotifyFromISR(g_usb_thread_handle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
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

static void sendDeattachEvent(uint8_t device_type)
{
	switch (device_type) {
	case 0x01:
		xEventGroupClearBits(g_usb_thread_event, USB_THREAD_MSG_KEYBOARD_ATTACH);
		xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_KEYBOARD_DEATTACH);
		break;
	case 0x02:
		xEventGroupClearBits(g_usb_thread_event, USB_THREAD_MSG_MOUSE_ATTACH);
		xEventGroupSetBits(g_usb_thread_event, USB_THREAD_MSG_MOUSE_DEATTACH);
		break;
	default:
		LOGW("Unknow device: 0x%02X", device_type);
		break;
	}
}

static void usbCheckDeviceType(uint8_t *device_type)
{
	uint8_t *p8 = &g_p_usb_hhid_interface_table[g_usbfs_hhid_basic_cfg.module_number][0];

	if ((p8[5] == 0x03) && (p8[6] == 0x01)) {
		*device_type = p8[7];
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
	while ((status.attached == 0) || (status.type != DETECT_TYPE_SOURCE)) {
		xTaskNotifyWait(0x00, 0x01, NULL, portMAX_DELAY);
		FUSB303B_Check(&status);
	}
	LOGD("USB Attach");
}

#if __USB_THREAD_DEBUG
static void debugOutputDeviceDescriptor(const char *tag)
{
	uint8_t t8;
	uint16_t t16;

	LOG_D(tag, "Device Descriptor");
	for (t8 = 0; t8 < g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][0]; t8++) {
		LOG_Printf("%02X ", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][t8]);
	}
	LOG_Printf("\r\n");

	LOG_PrintfEndl("bLength           : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][0]);
	LOG_PrintfEndl("bDescriptorType   : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][1]);

	t16 = g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][3];
	t16 <<= 8;
	t16 |= g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][2];
	LOG_PrintfEndl("bcdUSB            : 0x%04X", t16);
	LOG_PrintfEndl("bDeviceClass      : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][4]);
	LOG_PrintfEndl("bDeviceSubClass   : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][5]);
	LOG_PrintfEndl("bDeviceProtocol   : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][6]);
	LOG_PrintfEndl("bMaxPacketSize0   : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][7]);

	t16 = g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][9];
	t16 <<= 8;
	t16 |= g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][8];
	LOG_PrintfEndl("idVendor          : 0x%04X", t16);

	t16 = g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][11];
	t16 <<= 8;
	t16 |= g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][10];
	LOG_PrintfEndl("idProduct         : 0x%04X", t16);

	t16 = g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][13];
	t16 <<= 8;
	t16 |= g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][12];
	LOG_PrintfEndl("bcdDevice         : 0x%04X", t16);

	LOG_PrintfEndl("iManufacturer     : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][14]);
	LOG_PrintfEndl("iProduct          : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][15]);
	LOG_PrintfEndl("iSerialNumber     : 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][16]);
	LOG_PrintfEndl("bNumConfigurations: 0x%02X", g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][17]);
}

static void debugOutputConfigurationDescriptor(const char *tag)
{
	uint8_t t8;
	uint16_t total_length;

	LOG_D(tag, "Configuration Descriptor");
	for (t8 = 0; t8 < 9; t8++) {
		LOG_Printf("%02X ", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][t8]);
	}
	LOG_Printf("\r\n");

	LOG_PrintfEndl("bLength            : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][0]);
	LOG_PrintfEndl("bDescriptorType    : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][1]);

	total_length = g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][3];
	total_length <<= 8;
	total_length |= g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][2];
	LOG_PrintfEndl("wTotalLength       : 0x%04X", total_length);

	LOG_PrintfEndl("bNumInterfaces     : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][4]);
	LOG_PrintfEndl("bConfigurationValue: 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][5]);
	LOG_PrintfEndl("iConfiguration     : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][6]);
	LOG_PrintfEndl("bmAttributes       : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][7]);
	LOG_PrintfEndl("bMaxPower          : 0x%02X", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][8]);
}

static void debugOutputInterfaceDescriptor(const char *tag)
{
	uint8_t i8;
	uint8_t endpoint_index;
	uint8_t interface_index;
	uint8_t hid_index;
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t i16, j16;
	uint16_t total_length;

	uint32_t is_new_interface = 0;
	uint8_t configuration_num = g_p_usb_hhid_device_table[g_usbfs_hhid_basic_cfg.module_number][17];

	LOG_D(tag, "Interface Descriptor:");
	for (i8 = 0; i8 < configuration_num; i8++) {
		LOG_PrintfEndl("Configuration Descriptor[%u] All:", i8);
		total_length = g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][3];
		total_length <<= 8;
		total_length |= g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][2];
		for (i16 = 0; i16 < total_length; i16++) {
			LOG_Printf("%02X ", g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][i16]);
		}
		LOG_Printf("\r\n");

		endpoint_index = 0;
		interface_index = 0;
		hid_index = 0;
		j16 = 0x09;
		while (j16 < total_length) {
			bLength = g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][j16];
			bDescriptorType = g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][j16 + 1];
			switch (bDescriptorType) {
			case 0x04:
				LOG_PrintfEndl("Interface [%" PRIu32 "]", interface_index);
				debugDecodeInterfaceDescriptor(&g_p_usb_hhid_config_table[g_usbfs_hhid_basic_cfg.module_number][j16]);
				interface_index++;
				is_new_interface = UINT32_MAX;
				break;
			case 0x05:
				if (is_new_interface & 0x01) {
					is_new_interface &= (uint32_t)(~0x01);
					endpoint_index = 0;
				}
				LOG_PrintfEndl("\tEndpoint [%" PRIu32 "]", endpoint_index);
				endpoint_index++;
				break;
			case 0x21:
				if (is_new_interface & 0x02) {
					is_new_interface &= (uint32_t)(~0x02);
					hid_index = 0;
				}
				LOG_PrintfEndl("\tHID [%" PRIu32 "]", hid_index);
				hid_index++;
				break;
			default:
				LOG_W(tag, "Unchecked bDescriptorType: 0x%02X", bDescriptorType);
				break;
			}
			j16 += bLength;
		}
	}
}

static void debugDecodeInterfaceDescriptor(uint8_t *data)
{
	LOG_PrintfEndl("bLength           : 0x%02X", data[0]);
	LOG_PrintfEndl("bDescriptorType   : 0x%02X", data[1]);
	LOG_PrintfEndl("bInterfaceNumber  : 0x%02X", data[2]);
	LOG_PrintfEndl("bAlternateSetting : 0x%02X", data[3]);
	LOG_PrintfEndl("bNumEndpoints     : 0x%02X", data[4]);

	LOG_Printf("bInterfaceClass   : 0x%02X", data[5]);
	if (data[5] == 0x03) {
		LOG_Printf(" [HID]");
	}
	LOG_Printf("\r\n");

	LOG_Printf("bInterfaceSubClass: 0x%02X", data[6]);
	if (data[6] == 0x01) {
		LOG_Printf(" [Boot]");
	}
	LOG_Printf("\r\n");

	LOG_Printf("bInterfaceProtocol: 0x%02X", data[7]);
	if (data[7] == 0x01) {
		LOG_Printf(" [Keyboard]");
	}
	else if (data[7] == 0x02) {
		LOG_Printf(" [Mouse]");
	}
	LOG_Printf("\r\n");

	LOG_PrintfEndl("iInterface        : 0x%02X", data[8]);
}
#endif /* #if __USB_THREAD_DEBUG */
