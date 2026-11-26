#ifndef _USB_HID_CODE_H
#define _USB_HID_CODE_H

#include <stdbool.h>
#include <stdint.h>

#define HID_BP_CODE_STRING_LEN		0xDE /* USB keyboard Boot Protocol decode string length */

/* USB keyboard report format in Boot Protocol:
 * Byte 0: Modifier Keys, each bit meaning as follow macro
 * Byte 1: Reserved
 * Byte 2: Button code 1
 * Byte 3: Button code 2
 * Byte 4: Button code 3
 * Byte 5: Button code 4
 * Byte 6: Button code 5
 * Byte 7: Button code 6 */
#define HID_BP_BIT_LEFT_CTRL		0x01
#define HID_BP_BIT_LEFT_SHIFT		0x02
#define HID_BP_BIT_LEFT_ALT			0x04
#define HID_BP_BIT_LEFT_GUI			0x08 /* Windows button */
#define HID_BP_BIT_RIGHT_CTRL		0x10
#define HID_BP_BIT_RIGHT_SHIFT		0x20
#define HID_BP_BIT_RIGHT_ALT		0x40
#define HID_BP_BIT_RIGHT_GUI		0x80 /* Windows button */

#define HID_BP_KEYBOARD_A								0x04 /* a and A */
#define HID_BP_KEYBOARD_B								0x05 /* b and B */
#define HID_BP_KEYBOARD_C								0x06 /* c and C */
#define HID_BP_KEYBOARD_D								0x07 /* d and D */
#define HID_BP_KEYBOARD_E								0x08 /* e and E */
#define HID_BP_KEYBOARD_F								0x09 /* f and F */
#define HID_BP_KEYBOARD_G								0x0A /* g and G */
#define HID_BP_KEYBOARD_H								0x0B /* h and H */
#define HID_BP_KEYBOARD_I								0x0C /* i and I */
#define HID_BP_KEYBOARD_J								0x0D /* j and J */
#define HID_BP_KEYBOARD_K								0x0E /* k and K */
#define HID_BP_KEYBOARD_L								0x0F /* l and L */
#define HID_BP_KEYBOARD_M								0x10 /* m and M */
#define HID_BP_KEYBOARD_N								0x11 /* n and N */
#define HID_BP_KEYBOARD_O								0x12 /* o and O */
#define HID_BP_KEYBOARD_P								0x13 /* p and P */
#define HID_BP_KEYBOARD_Q								0x14 /* q and Q */
#define HID_BP_KEYBOARD_R								0x15 /* r and R */
#define HID_BP_KEYBOARD_S								0x16 /* s and S */
#define HID_BP_KEYBOARD_T								0x17 /* t and T */
#define HID_BP_KEYBOARD_U								0x18 /* u and U */
#define HID_BP_KEYBOARD_V								0x19 /* v and V */
#define HID_BP_KEYBOARD_W								0x1A /* w and W */
#define HID_BP_KEYBOARD_X								0x1B /* x and X */
#define HID_BP_KEYBOARD_Y								0x1C /* y and Y */
#define HID_BP_KEYBOARD_Z								0x1D /* z and Z */
#define HID_BP_KEYBOARD_1								0x1E /* 1 and ! */
#define HID_BP_KEYBOARD_2								0x1F /* 2 and @ */
#define HID_BP_KEYBOARD_3								0x20 /* 3 and # */
#define HID_BP_KEYBOARD_4								0x21 /* 4 and $ */
#define HID_BP_KEYBOARD_5								0x22 /* 5 and % */
#define HID_BP_KEYBOARD_6								0x23 /* 6 and ^ */
#define HID_BP_KEYBOARD_7								0x24 /* 7 and & */
#define HID_BP_KEYBOARD_8								0x25 /* 8 and * */
#define HID_BP_KEYBOARD_9								0x26 /* 9 and ( */
#define HID_BP_KEYBOARD_0								0x27 /* 0 and ) */
#define HID_BP_KEYBOARD_ENTER							0x28
#define HID_BP_KEYBOARD_ESCAPE							0x29
#define HID_BP_KEYBOARD_BACKSPACE						0x2A
#define HID_BP_KEYBOARD_TAB								0x2B
#define HID_BP_KEYBOARD_SPACE							0x2C

#define HID_BP_KEYBOARD_CAPS_LOCK						0x39
#define HID_BP_KEYBOARD_F1								0x3A
#define HID_BP_KEYBOARD_F2								0x3B
#define HID_BP_KEYBOARD_F3								0x3C
#define HID_BP_KEYBOARD_F4								0x3D
#define HID_BP_KEYBOARD_F5								0x3E
#define HID_BP_KEYBOARD_F6								0x3F
#define HID_BP_KEYBOARD_F7								0x40
#define HID_BP_KEYBOARD_F8								0x41
#define HID_BP_KEYBOARD_F9								0x42
#define HID_BP_KEYBOARD_F10								0x43
#define HID_BP_KEYBOARD_F11								0x44
#define HID_BP_KEYBOARD_F12								0x45
#define HID_BP_KEYBOARD_PRINT_SCREEN					0x46
#define HID_BP_KEYBOARD_SCROLL_LOCK						0x47
#define HID_BP_KEYBOARD_PAUSE							0x48
#define HID_BP_KEYBOARD_INSERT							0x49
#define HID_BP_KEYBOARD_HOME							0x4A
#define HID_BP_KEYBOARD_PAGE_UP							0x4B
#define HID_BP_KEYBOARD_DELETE_FORWARD					0x4C
#define HID_BP_KEYBOARD_END								0x4D
#define HID_BP_KEYBOARD_PAGE_DOWN						0x4E
#define HID_BP_KEYBOARD_RIGHT_ARROW						0x4F
#define HID_BP_KEYBOARD_LEFT_ARROW						0x50
#define HID_BP_KEYBOARD_DOWN_ARROW						0x51
#define HID_BP_KEYBOARD_UP_ARROW						0x52
#define HID_BP_KEYPAD_NUM_LOCK_AND_CLEAR				0x53

#define HID_BP_KEYPAD_ENTER								0x58
#define HID_BP_KEYPAD_1_AND_END							0x59
#define HID_BP_KEYPAD_2_AND_DOWN_ARROW					0x5A
#define HID_BP_KEYPAD_3_AND_PAGE_DOWN					0x5B
#define HID_BP_KEYPAD_4_AND_LEFT_ARROW					0x5C
#define HID_BP_KEYPAD_5									0x5D
#define HID_BP_KEYPAD_6_AND_RIGHT_ARROW					0x5E
#define HID_BP_KEYPAD_7_AND_HOME						0x5F
#define HID_BP_KEYPAD_8_AND_UP_ARROW					0x60
#define HID_BP_KEYPAD_9_AND_PAGE_UP						0x61
#define HID_BP_KEYPAD_0_AND_INSERT						0x62
#define HID_BP_KEYPAD_PERIOD_AND_DELETE					0x63

#define HID_BP_KEYBOARD_APPLICATION						0x65
#define HID_BP_KEYBOARD_POWER							0x66

#define HID_BP_KEYBOARD_F13								0x68
#define HID_BP_KEYBOARD_F14								0x69
#define HID_BP_KEYBOARD_F15								0x6A
#define HID_BP_KEYBOARD_F16								0x6B
#define HID_BP_KEYBOARD_F17								0x6C
#define HID_BP_KEYBOARD_F18								0x6D
#define HID_BP_KEYBOARD_F19								0x6E
#define HID_BP_KEYBOARD_F20								0x6F
#define HID_BP_KEYBOARD_F21								0x70
#define HID_BP_KEYBOARD_F22								0x71
#define HID_BP_KEYBOARD_F23								0x72
#define HID_BP_KEYBOARD_F24								0x73
#define HID_BP_KEYBOARD_EXECUTE							0x74
#define HID_BP_KEYBOARD_HELP							0x75
#define HID_BP_KEYBOARD_MENU							0x76
#define HID_BP_KEYBOARD_SELECT							0x77
#define HID_BP_KEYBOARD_STOP							0x78
#define HID_BP_KEYBOARD_AGAIN							0x79
#define HID_BP_KEYBOARD_UNDO							0x7A
#define HID_BP_KEYBOARD_CUT								0x7B
#define HID_BP_KEYBOARD_COPY							0x7C
#define HID_BP_KEYBOARD_PASTE							0x7D
#define HID_BP_KEYBOARD_FIND							0x7E
#define HID_BP_KEYBOARD_MUTE							0x7F
#define HID_BP_KEYBOARD_VOLUME_UP						0x80
#define HID_BP_KEYBOARD_VOLUME_DOWN						0x81
#define HID_BP_KEYBOARD_LOCKING_CAPS_LOCK				0x82
#define HID_BP_KEYBOARD_LOCKING_NUM_LOCK				0x83
#define HID_BP_KEYBOARD_LOCKING_SCROLL_LOCK				0x84

#define HID_BP_KEYBOARD_INTERNATIONAL1					0x87
#define HID_BP_KEYBOARD_INTERNATIONAL2					0x88
#define HID_BP_KEYBOARD_INTERNATIONAL3					0x89
#define HID_BP_KEYBOARD_INTERNATIONAL4					0x8A
#define HID_BP_KEYBOARD_INTERNATIONAL5					0x8B
#define HID_BP_KEYBOARD_INTERNATIONAL6					0x8C
#define HID_BP_KEYBOARD_INTERNATIONAL7					0x8D
#define HID_BP_KEYBOARD_INTERNATIONAL8					0x8E
#define HID_BP_KEYBOARD_INTERNATIONAL9					0x8F
#define HID_BP_KEYBOARD_LANG1							0x90
#define HID_BP_KEYBOARD_LANG2							0x91
#define HID_BP_KEYBOARD_LANG3							0x92
#define HID_BP_KEYBOARD_LANG4							0x93
#define HID_BP_KEYBOARD_LANG5							0x94
#define HID_BP_KEYBOARD_LANG6							0x95
#define HID_BP_KEYBOARD_LANG7							0x96
#define HID_BP_KEYBOARD_LANG8							0x97
#define HID_BP_KEYBOARD_LANG9							0x98
#define HID_BP_KEYBOARD_ALTERNATE_ERASE					0x99
#define HID_BP_KEYBOARD_SYSREQ_AND_ATTENTION			0x9A
#define HID_BP_KEYBOARD_CANCEL							0x9B
#define HID_BP_KEYBOARD_CLEAR							0x9C
#define HID_BP_KEYBOARD_PRIOR							0x9D
#define HID_BP_KEYBOARD_RETURN							0x9E
#define HID_BP_KEYBOARD_SEPARATOR						0x9F
#define HID_BP_KEYBOARD_OUT								0xA0
#define HID_BP_KEYBOARD_OPER							0xA1
#define HID_BP_KEYBOARD_CLEAR_AND_AGAIN					0xA2
#define HID_BP_KEYBOARD_CRSEL_AND_PROPS					0xA3
#define HID_BP_KEYBOARD_EXSEL							0xA4

#define HID_BP_KEYPAD_00								0xB0
#define HID_BP_KEYPAD_000								0xB1

#define HID_BP_KEYPAD_TAB								0xBA
#define HID_BP_KEYPAD_BACKSPACE							0xBB
#define HID_BP_KEYPAD_A									0xBC
#define HID_BP_KEYPAD_B									0xBD
#define HID_BP_KEYPAD_C									0xBE
#define HID_BP_KEYPAD_D									0xBF
#define HID_BP_KEYPAD_E									0xC0
#define HID_BP_KEYPAD_F									0xC1
#define HID_BP_KEYPAD_XOR								0xC2

#define HID_BP_KEYPAD_SPACE								0xCD

#define HID_BP_KEYPAD_MEMORY_STORE						0xD0
#define HID_BP_KEYPAD_MEMORY_RECALL						0xD1
#define HID_BP_KEYPAD_MEMORY_CLEAR						0xD2
#define HID_BP_KEYPAD_MEMORY_ADD						0xD3
#define HID_BP_KEYPAD_MEMORY_SUBTRACT					0xD4
#define HID_BP_KEYPAD_MEMORY_MULTIPLY					0xD5
#define HID_BP_KEYPAD_MEMORY_DIVIDE						0xD6

#define HID_BP_KEYPAD_CLEAR								0xD8
#define HID_BP_KEYPAD_CLEAR_ENTRY						0xD9
#define HID_BP_KEYPAD_BINARY							0xDA
#define HID_BP_KEYPAD_OCTAL								0xDB
#define HID_BP_KEYPAD_DECIMAL							0xDC
#define HID_BP_KEYPAD_HEXADECIMAL						0xDD

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	USB_HID_TYPE_UNKNOW,
	USB_HID_TYPE_KEYBOARD,
	USB_HID_TYPE_MOUSE,
} USB_HID_TypeEnum;

typedef struct {
	USB_HID_TypeEnum device_type;
	uint32_t flag;
	uint8_t data[64];
} USB_HID_QueueContentType;

extern const char * const g_usb_hid_bp_decode_string[2][HID_BP_CODE_STRING_LEN];

bool HID_CodeIsKeypad(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _USB_HID_CODE_H */
