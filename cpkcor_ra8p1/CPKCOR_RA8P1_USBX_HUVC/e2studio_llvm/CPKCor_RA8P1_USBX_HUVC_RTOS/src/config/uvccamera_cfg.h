/***********************************************************************************************************************
 * File Name    : uvccamera_cfg.h
 * Description  : CPKCOR-RA8P1 USB camera (UVC) host demo - configuration
 *                The streaming path is chosen by the camera's VideoStreaming endpoint type (ISO / BULK).
 **********************************************************************************************************************/
#ifndef UVCCAMERA_CFG_H_
#define UVCCAMERA_CFG_H_

#include "ux_api.h"
#include "ux_system.h"
#include "ux_host_class_video.h"

#if defined(UVC_CONSOLE_RTT) || defined(UVC_CONSOLE_SCI9_B)
 #error "Remove UVC_CONSOLE_RTT / UVC_CONSOLE_SCI9_B from Defined symbols, use UVC_CONSOLE_TYPE=0 (RTT) or UVC_CONSOLE_TYPE=1 (UART)"
#endif

#define UX_ZERO                                 (0U)
#define BYTE_SIZE                               (4U)

/* ===================== Debug console =====================
 * Select one console with UVC_CONSOLE_TYPE:
 *   0 = J-Link RTT Viewer
 *   1 = UART SCI9_B
 *
 * Recommended: change UVC_CONSOLE_TYPE_DEFAULT in this file.
 *
 * Optional: for special build configurations, override it in:
 *   Project -> Properties -> C/C++ Build -> Settings
 *           -> Tool Settings -> Compiler -> Source
 *           -> User defined compiler options
 *
 *   -DUVC_CONSOLE_TYPE=0    // RTT
 *   -DUVC_CONSOLE_TYPE=1    // SCI9_B
 *
 * Project properties have higher priority. Make sure the value matches
 * the intended build configuration.
 */

#define UVC_CONSOLE_RTT                         (0)     /* SEGGER J-Link RTT Viewer */
#define UVC_CONSOLE_SCI9_B                      (1)     /* UART SCI9_B, TXD9 = P209, RXD9 = P208 -> J-Link OB VCOM, g_uart9 */

#define UVC_CONSOLE_TYPE_DEFAULT                UVC_CONSOLE_SCI9_B      /* <- edit here: 0 = RTT, 1 = UART */

#if defined(UVC_CONSOLE_TYPE)
 #define UVC_CONSOLE_FROM                       "project properties"
 #if (UVC_CONSOLE_TYPE != UVC_CONSOLE_TYPE_DEFAULT)

 #endif
#else
 #define UVC_CONSOLE_TYPE                       UVC_CONSOLE_TYPE_DEFAULT
 #define UVC_CONSOLE_FROM                       "uvccamera_cfg.h"
#endif

#if (UVC_CONSOLE_TYPE == UVC_CONSOLE_RTT)

 #define UVC_CONSOLE_NAME                       "J-Link RTT Viewer"
 #define UI_PRINT_PACE_TICKS                    (3U)    /* pacing between console lines (RTT up-buffer) */
#elif (UVC_CONSOLE_TYPE == UVC_CONSOLE_SCI9_B)

 #define UVC_CONSOLE_NAME                       "UART SCI9_B (J-Link VCOM, 115200 8N1)"
 #define UVC_CONSOLE_UART_INSTANCE              g_uart9 /* FSP: r_sci_b_uart instance name */
 #define UVC_CONSOLE_UART_ECHO                  (1)     /* 1: echo typed characters (terminal has no local echo) */
 #define UVC_CONSOLE_TX_BUF_SIZE                (256U)
 #define UVC_CONSOLE_RX_RING_SIZE               (128U)
 #define UVC_CONSOLE_TX_TIMEOUT_MS              (100U)
 #define UI_PRINT_PACE_TICKS                    (1U)    /* UART write is blocking, little pacing needed */
#else
 #error "UVC_CONSOLE_TYPE must be 0 (RTT) or 1 (UART SCI9_B). Check Defined symbols in the project properties."
#endif

#define UVC_CONSOLE_RX_CHUNK                    (64U)   /* max. input bytes handled per UI poll */
#define UI_NUMBER_IDLE_MS                       (800U)  /* number typed without Enter is accepted after this idle time */

/* ===================== Mode scan range ===================== */
#define UVC_AUTO_MAX_WIDTH                      (3840)
#define UVC_AUTO_MAX_HEIGHT                     (2160)
#define UVC_AUTO_MAX_FPS                        (60)
#define UVC_AUTO_MIN_FPS                        (10)

/* ===================== Mode status (software estimate) =====================
 * OK / NG in the mode list is calculated by software only. The actual result is the hardware test output.
 *   NG-P : ISO, PROBE payload > largest single-packet ISO endpoint (RA8P1: 1 packet <= 1024 B per micro-frame)
 *   NG-B : estimated MJPEG frame size > usable frame buffer
 * Estimated MJPEG frame size = width x height x UVC_MJPEG_EST_BPP_X100 / 100 / 8
 *   (80 = 0.8 bit/pixel; if the camera reports a smaller dwMaxVideoFrameSize, that value is used)
 *   example: 2560x1440 -> 360 KB (fits 384 KB SRAM buffer), 3840x2160 -> 810 KB (needs SDRAM) */
#define UVC_MJPEG_EST_BPP_X100                  (80U)

/* 'c' : stop at the next frame boundary; if no frame boundary arrives within this time, stop anyway */
#define UVC_CAPTURE_TIMEOUT_MS                  (500U)

/* RA8P1 USBHS isochronous: 1 packet per micro-frame, <= 1024 B (PIPEMAXP has no "additional transactions" field) */
#define UVC_RA_ISO_MAX_PACKET                   (1024U)

/* ===================== Frame buffers =====================
 * 1080p MJPEG ~100-250 KB, 1440p ~150-250 KB, 4K ~300-600 KB (depends on the scene).
 * 0 (default): frame buffers in SRAM, 384 KB each (up to 2560x1440 MJPEG, 4K modes are NG-B)
 * 1          : frame buffers in SDRAM, 1.5 MB each (needed for 3840x2160)
 *              -> enable SDRAM in the BSP and check the section name in script/fsp.ld */
#define JPEG_BUFFER_NUMBER                      (2)
#define UVC_FB_IN_SDRAM                         (0)

#if UVC_FB_IN_SDRAM
 #define JPEG_BUFFER_SIZE                       (1536U * 1024U)
 #define FB_BUFFER_ATTR                         BSP_PLACE_IN_SECTION(".sdram_noinit") BSP_ALIGN_VARIABLE(32)
 #define FB_BUFFER_MEM_NAME                     "SDRAM"
#else
 #define JPEG_BUFFER_SIZE                       (384U * 1024U)
 #define FB_BUFFER_ATTR                         BSP_ALIGN_VARIABLE(32)
 #define FB_BUFFER_MEM_NAME                     "SRAM"
#endif

#define VIDEO_XFER_NUM                          (2)     /* ISO transfer buffers */
#define VIDEO_XFER_PREQUEUE                     (1)     /* ISO buffers queued at the same time */
#define VIDEO_BUFFER_SIZE                       (3U * 1024U)

/* ===================== BULK path ===================== */
#define BULK_XFER_SIZE                          (16U * 1024U)   /* one BULK IN read (multiple of 512) */
#define BULK_TIMEOUT_MS                         (300U)          /* wait per read (camera may NAK) */
#define BULK_STOP_CLEAR_HALT                    (1)             /* CLEAR_FEATURE(ENDPOINT_HALT) on stop */

/* RA USBHS: PIPE1-2 = ISO/BULK, PIPE3-5 = BULK only. The first free pipe in this range is used. */
#define BULK_PIPE_FIRST                         (1U)
#define BULK_PIPE_LAST                          (5U)

/* ===================== JPEG ===================== */
#define JPEG_REQUIRE_EOI                        (1)
#define JPEG_EOI_SEARCH_BYTES                   (4096U)

#define EVENTFLAG_USB_DEVICE_INSERTED           (1U << 0)

#define UVC_STREAM_HEADER_HEADERINFO_ERR        (1U << 6)
#define UVC_STREAM_HEADER_HEADERINFO_EOF        (1U << 1)
#define UVC_STREAM_HEADER_HEADERINFO_FID        (1U << 0)

#define DHT_TABLE_SIZE                          (420)
#define JPEG_BUFFER_ALIGN                       (8u)
#define MEMPOOL_SIZE                            (1024U*60)

/* bytes of one frame buffer that can hold JPEG data (DHT insert + alignment reserved) */
#define FB_USABLE_BYTES                         (JPEG_BUFFER_SIZE - DHT_TABLE_SIZE - JPEG_BUFFER_ALIGN)

#define array_count(array) (sizeof(array)/sizeof(array[0]))

typedef struct uvc_camera_cfg
{
    UX_HOST_CLASS_VIDEO  * volatile video_host_class;
    UCHAR                *video_data_ptrs[VIDEO_XFER_NUM];
    ULONG                device_inserted_eventflag;
} uvc_camera_cfg_t;

typedef struct uvc_stream_header_struct
{
    uint8_t bHeaderLength;
    uint8_t bmHeaderInfo;
    uint8_t dwPresentationTime[4];
    uint8_t scrSourceClock_SourceTimeClock[4];
    uint8_t scrSourceClock_SOFCounter[2];
} uvc_stream_header_t;

typedef struct fb_data_struct
{
    UCHAR   *p_pointer;
    uint32_t size;
} fb_data_t;

#endif /* UVCCAMERA_CFG_H_ */
