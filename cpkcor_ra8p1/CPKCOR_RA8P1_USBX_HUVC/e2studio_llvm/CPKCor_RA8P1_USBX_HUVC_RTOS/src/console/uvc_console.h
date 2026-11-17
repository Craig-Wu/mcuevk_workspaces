/***********************************************************************************************************************
 * File Name    : uvc_console.h
 * Description  : CPKCOR-RA8P1 USB camera (UVC) host demo - debug console abstraction (RTT / UART SCI9_B)
 *                Backend: UVC_CONSOLE_TYPE 0 = RTT, 1 = UART SCI9_B (uvccamera_cfg.h or project properties)
 **********************************************************************************************************************/
#ifndef UVC_CONSOLE_H_
#define UVC_CONSOLE_H_

#include <stdint.h>
#include <stdbool.h>
#include "uvccamera_cfg.h"

void     uvc_console_init(void);                    /* open the console backend                 */
void     uvc_console_banner(void);                  /* print the start-up banner (EP_BANNER)    */
void     uvc_console_line(const char *msg);         /* one message = one line                   */
void     uvc_console_error_line(const char *msg);   /* error line                               */
void     uvc_console_error_trap(uint32_t err);      /* print error code and stop                */
uint32_t uvc_console_read(char *buf, uint32_t size);/* non-blocking input, returns bytes read    */
void     uvc_console_echo(char ch);                 /* local echo (UART only, no-op for RTT)    */

#endif /* UVC_CONSOLE_H_ */
