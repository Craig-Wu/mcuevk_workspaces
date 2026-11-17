/***********************************************************************************************************************
 * File Name    : usbx_host_uvc_common.h
 * Description  : Macros and texts used by the CPKCOR-RA8P1 USB camera (UVC) host demo
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2023 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#ifndef USBX_HOST_UVC_COMMON_H_
#define USBX_HOST_UVC_COMMON_H_

#include "uvccamera_cfg.h"

/* Macros to print info, error and trap the error (console selected by UVC_CONSOLE_TYPE) */
#if (UVC_CONSOLE_TYPE == UVC_CONSOLE_RTT)
 #define PRINT_INFO_STR(str) (app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_INFO_STR, sizeof(str), (str)))
 #define PRINT_ERR_STR(str)  (app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_ERR_STR, sizeof(str), (str)))
 #define ERROR_TRAP(err)     (app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_ERR_TRAP, sizeof(UINT *), (void *)&(err)))
#else
 #include "uvc_console.h"
 #define PRINT_INFO_STR(str) (uvc_console_line(str))
 #define PRINT_ERR_STR(str)  (uvc_console_error_line(str))
 #define ERROR_TRAP(err)     (uvc_console_error_trap((uint32_t) (err)))
#endif

/* Start-up banner (%d.%d.%d = FSP version) */
#define EP_BANNER \
"\r\n=====================================================================================\r\n"\
"  CPKCOR-RA8P1  USB Camera (UVC) Host Demo                FSP %d.%d.%d / Azure RTOS USBX\r\n"\
"=====================================================================================\r\n"\
"  Connect a USB UVC camera (MJPEG) to the USB High-Speed Type-C port of CPKCOR-RA8P1.\r\n"\
"  Isochronous and Bulk cameras are supported, cameras can be hot-plugged.\r\n"\
"  This demo will:\r\n"\
"    1. detect the camera and its streaming type (Isochronous or Bulk)\r\n"\
"    2. list every MJPEG resolution / frame rate the camera offers\r\n"\
"    3. estimate by software whether RA8P1 can receive each mode (OK / NG)\r\n"\
"    4. stream the selected mode and report FPS, data rate and frame integrity (hardware result)\r\n"\
"    5. capture one JPEG frame -> e2 studio Memory view: fb_data.p_pointer -> Image\r\n"\
"  Console: " UVC_CONSOLE_NAME " (selected in " UVC_CONSOLE_FROM ").\r\n"\
"  Type a command and press Enter ('h' = help).\r\n"\
"-------------------------------------------------------------------------------------\r\n"

/* Kept for compatibility (RTT_OUTPUT_MESSAGE_APP_PRINT_MENU) */
#define USBX_HUVC_MENU  "  Commands: <No.> run mode | c capture + stop | x stop\r\n"\
                        "            l list modes | p OK modes | d device details | s rescan | h help\r\n"
#define EP_INFO         ""

/* Macros constant value */
#define ZERO                            (0)
#define STOP                            (1)
#define START                           (2)
#define CAPTURE_IMAGE                   (3)
#define WAIT_10_TICKS                   (10)
#define WAIT_100_TICKS                  (100)

#endif /* USBX_HOST_UVC_COMMON_H_ */
