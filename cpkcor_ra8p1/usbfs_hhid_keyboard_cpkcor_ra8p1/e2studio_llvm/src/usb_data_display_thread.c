#include "common.h"
#include "hal_data.h"
#include "usb_hid_code.h"

#include "utils/log.h"

#ifndef __USB_DATA_DISPLAY_THREAD_DEBUG
#define __USB_DATA_DISPLAY_THREAD_DEBUG	0
#endif

#if __USB_DATA_DISPLAY_THREAD_DEBUG
#define LOGD(msg, ...)		LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGI(msg, ...)		LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGW(msg, ...)		LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define LOGE(msg, ...)		LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define LOGD(msg, ...)
#define LOGI(msg, ...)
#define LOGW(msg, ...)
#define LOGE(msg, ...)
#endif

void USBDataDisplayThread(void *param)
{
	bool left_shift_press;
	bool right_shift_press;
	bool all_zero;
	bool concat;
	uint8_t index_keyboard;
	uint8_t index_keypad;
	uint8_t rec_data[USB_DATA_QUEUE_SIZE];
	uint32_t i;

	(void)param;

	while (1) {
		all_zero = true;
		xQueueReceive(g_usb_data_queue, rec_data, portMAX_DELAY);
	#if __USB_DATA_DISPLAY_THREAD_DEBUG
		LOG_I(__FUNCTION__, "Get data:");
		for (i = 0; i < USB_DATA_QUEUE_SIZE; i++) {
			printf("%u ", rec_data[i]);
		}
		printf("\r\n");
	#endif
		for (i = 0; i < USB_DATA_QUEUE_SIZE; i++) {
			/* main_thread using this byte to mark CapLock and NumLock, don't check this byte */
			if (i == 1) {
				continue;
			}
			if (rec_data[i] != 0) {
				all_zero = false;
				break;
			}
		}
		if (all_zero) {
			continue;
		}

		concat = false;
		left_shift_press = false;
		right_shift_press = false;
		index_keyboard = 0;
		index_keypad = 0;
		printf("These key press: ");
		if (rec_data[0] & HID_BP_BIT_LEFT_CTRL) {
			printf("Left Ctrl");
			concat = true;
		}
		if (rec_data[0] & HID_BP_BIT_LEFT_SHIFT) {
			left_shift_press = true;
			index_keyboard = 1;
		}
		if (rec_data[0] & HID_BP_BIT_LEFT_ALT) {
			printf("Left Alt");
			concat = true;
		}
		if (rec_data[0] & HID_BP_BIT_LEFT_GUI) {
			printf("Left Windows");
			concat = true;
		}
		if (rec_data[0] & HID_BP_BIT_RIGHT_CTRL) {
			printf("Right Ctrl");
			concat = true;
		}
		if (rec_data[0] & HID_BP_BIT_RIGHT_SHIFT) {
			right_shift_press = true;
			index_keyboard = 1;
		}
		if (rec_data[0] & HID_BP_BIT_RIGHT_ALT) {
			printf("Right Alt");
			concat = true;
		}
		if (rec_data[0] & HID_BP_BIT_RIGHT_GUI) {
			printf("Right Windows");
			concat = true;
		}

		/* Caps Lock status */
		if (rec_data[1] & 0x02) {
			if (left_shift_press || right_shift_press) {
				index_keyboard = 0;
			}
			else {
				index_keyboard = 1;
			}
		}
		/* Num Lock status */
		if (rec_data[1] & 0x01) {
			index_keypad = 1;
		}

		for (i = 2; i < USB_DATA_QUEUE_SIZE; i++) {
			if (rec_data[i]) {
				if (concat) {
					printf(" + ");
				}
				if (HID_CodeIsKeypad(rec_data[i])) {
					printf("%s", g_usb_hid_bp_decode_string[index_keypad][rec_data[i]]);
				}
				else {
					printf("%s", g_usb_hid_bp_decode_string[index_keyboard][rec_data[i]]);
				}
				concat = true;
			}
		}

		/* No other key press */
		if (concat == false) {
			if (left_shift_press && (right_shift_press == false)) {
				printf("Left Shift");
			}
			else if ((left_shift_press == false) && right_shift_press) {
				printf("Right Shift");
			}
			else if (left_shift_press && right_shift_press) {
				printf("Left Shift + Right Shift");
			}
		}

		printf("\r\n");
	}
}
