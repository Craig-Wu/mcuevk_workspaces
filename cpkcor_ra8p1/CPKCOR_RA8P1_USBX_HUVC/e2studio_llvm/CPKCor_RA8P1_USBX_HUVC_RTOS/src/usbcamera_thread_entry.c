/***********************************************************************************************************************
 * File Name    : usbcamera_thread_entry.c
 * Description  : CPKCOR-RA8P1 USB camera (UVC) host demo - MJPEG, Isochronous + Bulk streaming
 *
 *  The streaming path is chosen by the VideoStreaming endpoint type of the attached camera:
 *    - ISO IN endpoint  -> USBX video class
 *    - BULK IN endpoint -> own PROBE/COMMIT + r_usb_basic pipe + BULK IN read
 *
 *  Mode status in the list = software estimate only (OK / NG-P / NG-B):
 *    NG-P : ISO, PROBE payload > largest single-packet ISO endpoint (RA8P1: 1 packet <= 1024 B per micro-frame)
 *    NG-B : estimated MJPEG frame size > usable frame buffer (4K needs UVC_FB_IN_SDRAM = 1)
 *    OK   : none of the above
 *  The actual result is the hardware test output (Result PASS / FAIL after running the mode).
 *  Every mode, also NG, can be run.  Cameras can be hot-plugged (ISO <-> BULK).
 *
 *  Console : uvc_console.c  (UVC_CONSOLE_TYPE: 0 = RTT, 1 = UART SCI9_B)
 *
 *  RA8P1 / FSP notes
 *    - RA8P1 USBHS ISO: 1 packet / micro-frame, <= 1024 B (PIPEMAXP has no "additional transactions" field)
 *    - USBX frame_parameters_set() rejects payload > 3072 B (0x92) -> BULK cameras use own PROBE/COMMIT
 *    - rm_usbx_port.c creates no pipe for a video BULK endpoint -> the pipe is created / released here
 *    - A frame ends on the EOF bit, on a FID toggle, or (BULK) on a payload that ends with JPEG EOI
 *
 *  USBX / FSP sources are not modified.
 **********************************************************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "usbcamera_thread.h"
#include "uvccamera_cfg.h"
#include "usbx_host_uvc_common.h"
#include "uvc_console.h"
#include "common_utils.h"
#include "dht_table.h"

/* r_usb_basic internal API (same headers as rm_usbx_port.c) */
#include "r_usb_basic.h"
#include "r_usb_basic_api.h"
#include "../ra/fsp/src/r_usb_basic/src/driver/inc/r_usb_typedef.h"
#include "../ra/fsp/src/r_usb_basic/src/driver/inc/r_usb_extern.h"
#include "../ra/fsp/src/r_usb_basic/src/hw/inc/r_usb_bitdefine.h"
#include "../ra/fsp/src/r_usb_basic/src/hw/inc/r_usb_reg_access.h"

/* frame buffers in SDRAM need the SDRAM enabled in the BSP */
#if UVC_FB_IN_SDRAM && (!defined(BSP_CFG_SDRAM_ENABLED) || (0 == BSP_CFG_SDRAM_ENABLED))
 #error "UVC_FB_IN_SDRAM = 1 needs SDRAM enabled: FSP Configuration -> BSP -> RA8P1 Family -> SDRAM -> Enabled"
#endif

#define UVC_USB_IP                  (USB_IP1)       /* g_basic0: USB_IP1 (USBHS) */

#if (USB_CFG_DMA == USB_CFG_ENABLE)
extern const transfer_instance_t * g_p_usbx_transfer_tx[USB_NUM_USBIP];
extern const transfer_instance_t * g_p_usbx_transfer_rx[USB_NUM_USBIP];
#endif

#ifndef USB_TYPFIELD_BULK
 #define USB_TYPFIELD_BULK          (0x4000U)
#endif
#ifndef USB_DIR_H_IN
 #define USB_DIR_H_IN               (0x0000U)
#endif
#ifndef USB_SHTNAKON
 #define USB_SHTNAKON               (0x0080U)
#endif
#ifndef USB_DBLBON
 #define USB_DBLBON                 (0x0200U)
#endif
#ifndef USB_DEVADDRBIT
 #define USB_DEVADDRBIT             (12U)
#endif

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
 #define DCACHE_ON()                (0U != (SCB->CCR & SCB_CCR_DC_Msk))
#else
 #define DCACHE_ON()                (false)
#endif

/* ============================== constants ============================== */
#define PERF_WINDOW_PRINT           (1)     /* 1: one status line per second while streaming */
#define PERF_INVALID_PASS_X10       (50U)   /* invalid frames <= 5.0 % -> integrity PASS */
#define PERF_FPS_PASS_PCT           (95U)   /* average fps >= 95 % of target -> fps OK */
#define PERF_JUDGE_MIN_MS           (1000U) /* shorter tests are not judged */

#define ISO_MPS_SIZE_MASK           (0x07FFU)
#define USB_DT_INTERFACE_ID         (0x04U)
#define USB_DT_ENDPOINT_ID          (0x05U)
#define UVC_CS_INTERFACE            (0x24U)
#define UVC_VS_FORMAT_UNCOMPRESSED  (0x04U)
#define UVC_VS_FRAME_UNCOMPRESSED   (0x05U)
#define UVC_VS_FORMAT_MJPEG         (0x06U)
#define UVC_VS_FRAME_MJPEG          (0x07U)
#define UVC_VS_FORMAT_FRAME_BASED   (0x10U)
#define UVC_VS_FRAME_FRAME_BASED    (0x11U)
#define UVC_VS_FORMAT_STREAM_BASED  (0x12U)
#define EP_TYPE_ISO                 (1U)
#define EP_TYPE_BULK                (2U)
#define EP_TYPE_INT                 (3U)
#define UVC_PROBE_LEN_11            (34U)
#define UVC_PROBE_LEN_10            (26U)
#define UVC_MAX_CAND                (128U)
#define UVC_MAX_INTERVALS           (16U)
#define XFER_FIFO_SIZE              (16U)
#define UI_LINE_MAX                 (16U)
#define UI_ITEMS_PER_LINE           (5U)

typedef struct { USHORT w; USHORT h; } uvc_res_t;

static const uvc_res_t g_res_list[] =
{
    {1920,1080},{1600,896},{1280,960},{1280,720},{1024,768},{1024,576},{960,720},{960,540},
    {864,480},{800,600},{800,448},{640,480},{640,360},{352,288},{320,240},{176,144},{160,120},
};
static const ULONG g_interval_list[] = { 166666, 333333, 400000, 416666, 500000, 666666, 1000000 };

/* software estimate of one mode (no test run involved) */
typedef enum { EST_OK = 0, EST_NG_PACKET, EST_NG_BUFFER } est_t;

typedef struct
{
    USHORT w;
    USHORT h;
    ULONG  interval;
    ULONG  payload;
    ULONG  maxframe;
    ULONG  est_bytes;                                   /* estimated MJPEG frame size */
    UCHAR  fmt;
    UCHAR  frm;
    UCHAR  est;                                         /* est_t */
} uvc_cand_t;

/* ============================== variables ============================== */
static uint32_t             g_ux_uvc_pool_memory[MEMPOOL_SIZE / BYTE_SIZE];
static uvc_camera_cfg_t     uvc_cfg;
fb_data_t                   fb_data;                          /* e2 studio Memory view: fb_data.p_pointer */
static UINT                 g_video_status   = STOP;
static UCHAR                fb_buffer[JPEG_BUFFER_NUMBER][JPEG_BUFFER_SIZE] FB_BUFFER_ATTR;
static UCHAR                video_buffer[VIDEO_XFER_NUM][VIDEO_BUFFER_SIZE];
static uvc_camera_cfg_t     *thread_data     = &uvc_cfg;
static ULONG                actual_flags;

/* ISO completion FIFO */
typedef struct { UCHAR idx; ULONG len; } xfer_done_t;
static volatile xfer_done_t g_fifo[XFER_FIFO_SIZE];
static volatile UINT        g_fifo_w = 0, g_fifo_r = 0;

/* modes */
static uvc_cand_t           g_cand[UVC_MAX_CAND];
static UINT                 g_cand_num       = 0;
static UINT                 g_ep_limit       = 0;
static bool                 g_ready          = false;

/* BULK path */
static bool                 g_bulk_mode      = false;
static UX_ENDPOINT         *g_bulk_ep        = UX_NULL;
static UCHAR                g_bulk_buf[BULK_XFER_SIZE] __attribute__((aligned(32)));
static UCHAR                g_probe_buf[48]            __attribute__((aligned(32)));
static ULONG                g_probe_len      = UVC_PROBE_LEN_11;
static TX_SEMAPHORE         g_bulk_sem;
static bool                 g_bulk_sem_ok    = false;
static volatile ULONG       g_bulk_actual    = 0;
static volatile UINT        g_bulk_code      = 0;
static bool                 g_bk_in_payload  = false;
static ULONG                g_bk_got         = 0;
static UCHAR                g_bk_info        = 0;
static ULONG                g_bk_max_payload = 0;
static ULONG                g_bk_timeouts    = 0;
static ULONG                g_bk_dbg         = 0;
static ULONG                g_bk_wait_cycles = 0;
static uint8_t              g_bk_pipe        = 0;
static usb_utr_t            g_bk_utr;
static usb_utr_t            g_bk_abort_utr;
static volatile bool        g_bk_pending     = false;
static ULONG                g_bk_req_len     = 0;

/* camera survey */
typedef struct
{
    UINT n_iso, n_iso_hb, n_bulk;
    UINT n_fmt_mjpeg, n_fmt_uncomp, n_fmt_frame;
    UINT n_frm_mjpeg, n_frm_uncomp, n_frm_frame;
    UINT n_probe_fail, n_range;
    UINT last_probe_status;
} diag_t;
static diag_t               g_diag;

/* current test */
static UINT                 g_sel_idx        = 0;
static ULONG                g_capture_tick   = 0;
static UINT                 g_test_count     = 0;

/* frame assembly */
static UINT                 g_frame_index = 0, g_fb_index = 0;
static ULONG                g_frame_drop  = 0;              /* bytes that did not fit the buffer */
static bool                 g_frame_error = false, g_frame_ovf = false;
static int                  g_last_fid    = -1;

/* console line input */
static char                 g_line[UI_LINE_MAX];
static UINT                 g_line_len    = 0;
static ULONG                g_line_tick   = 0;

/* ============================== statistics ============================== */
typedef struct
{
    ULONG    start_tick, frames_valid, frames_invalid, startup_drop, transfers, err_packets;
    uint64_t raw_bytes, jpeg_bytes;
    ULONG    frame_min, frame_max, ovf_max;
    ULONG    fps_min_x10, fps_max_x10, windows;
    ULONG    first_frame_tick, last_frame_tick, interval_min_ms, interval_max_ms;
    uint64_t proc_cycles, dht_cycles;
    ULONG    inv_no_soi, inv_no_eoi, inv_err, inv_ovf;
    ULONG    full_packets, max_packet;                 /* ISO: packets that hit the 1-packet limit */
    ULONG    w_start_tick, w_frames, w_invalid;
    uint64_t w_raw_bytes;
} perf_t;
static perf_t g_perf;
static USHORT g_jpeg_w = 0, g_jpeg_h = 0, g_ep_mps_raw = 0;

/* ============================== prototypes ============================== */
UINT ux_host_usr_event_notification(ULONG event, UX_HOST_CLASS * host_class, VOID * instance);
VOID uvc_transfer_request_done_callback(UX_TRANSFER * transfer_request);
static void  con_print(bool pace, const char *fmt, ...);
static void  test_stop(const char *reason);
static void  frame_end_payload(UCHAR info);
static void  ui_list(bool ok_only);
static void  ui_legend(void);
static void  ui_help(void);
static void  uvc_scan(void);
static void  bulk_pipe_close(void);

static inline ULONG fps_x10(ULONG iv) { return (iv > 0U) ? (ULONG) (100000000UL / iv) : 0U; }
static inline ULONG fps_round(ULONG iv) { return (fps_x10(iv) + 5U) / 10U; }
static inline ULONG le32(const UCHAR *p)
{
    return (ULONG) p[0] | ((ULONG) p[1] << 8) | ((ULONG) p[2] << 16) | ((ULONG) p[3] << 24);
}
static inline void put_le32(UCHAR *p, ULONG v)
{
    p[0] = (UCHAR) v; p[1] = (UCHAR) (v >> 8); p[2] = (UCHAR) (v >> 16); p[3] = (UCHAR) (v >> 24);
}
static ULONG ms_of(ULONG ticks) { return (ULONG) (((uint64_t) ticks * 1000U) / TX_TIMER_TICKS_PER_SECOND); }
static ULONG ticks_of(ULONG ms) { return (ULONG) (((uint64_t) ms * TX_TIMER_TICKS_PER_SECOND) / 1000U); }
static ULONG cyc_to_us(uint64_t c) { uint32_t k = SystemCoreClock / 1000000U; return k ? (ULONG) (c / k) : 0U; }

static const char *est_name(const uvc_cand_t *c)
{
    if (EST_NG_PACKET == c->est) return "NG-P";
    if (EST_NG_BUFFER == c->est) return "NG-B";
    return "OK";
}

/* estimated MJPEG frame size: w x h x bpp; a smaller dwMaxVideoFrameSize from the camera is used instead */
static ULONG est_frame_bytes(USHORT w, USHORT h, ULONG maxframe)
{
    ULONG est = (ULONG) (((uint64_t) w * h * UVC_MJPEG_EST_BPP_X100) / 800U);
    if ((0U != maxframe) && (maxframe < est))
    {
        est = maxframe;
    }
    return est;
}

static void dcache_clean(const void *p, ULONG size)
{
    if (DCACHE_ON() && (NULL != p) && (0U != size))
    {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        uintptr_t a = (uintptr_t) p & ~(uintptr_t) 31U;
        SCB_CleanDCache_by_Addr((void *) a, (int32_t) (size + ((uintptr_t) p - a) + 32U));
#endif
    }
}
static void dcache_invalidate(void *p, ULONG size)
{
    if (DCACHE_ON())
    {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        SCB_InvalidateDCache_by_Addr(p, (int32_t) size);
#endif
    }
}

/* =====================================================================================================
 *  USB callbacks
 * ===================================================================================================*/
VOID uvc_transfer_request_done_callback(UX_TRANSFER *transfer_request)
{
    UCHAR *p   = transfer_request->ux_transfer_request_data_pointer;
    UINT   idx = VIDEO_XFER_NUM;

    for (UINT i = 0; i < VIDEO_XFER_NUM; i++)
    {
        if ((p >= thread_data->video_data_ptrs[i]) && (p < (thread_data->video_data_ptrs[i] + VIDEO_BUFFER_SIZE)))
        {
            idx = i;
            break;
        }
    }
    if (VIDEO_XFER_NUM == idx)
    {
        return;
    }
    UINT w = g_fifo_w;
    g_fifo[w % XFER_FIFO_SIZE].idx = (UCHAR) idx;
    g_fifo[w % XFER_FIFO_SIZE].len = transfer_request->ux_transfer_request_actual_length;
    g_fifo_w = w + 1U;
    tx_semaphore_put(&g_payload_semaphore);
}

static bool xfer_pop(UINT *idx, ULONG *len)
{
    if (g_fifo_r == g_fifo_w) return false;
    *idx = g_fifo[g_fifo_r % XFER_FIFO_SIZE].idx;
    *len = g_fifo[g_fifo_r % XFER_FIFO_SIZE].len;
    g_fifo_r++;
    return true;
}

static void xfer_reset(void)
{
    while (TX_SUCCESS == tx_semaphore_get(&g_payload_semaphore, TX_NO_WAIT)) {}
    g_fifo_r = g_fifo_w;
}

UINT ux_host_usr_event_notification(ULONG event, UX_HOST_CLASS * host_class, VOID * instance)
{
    if (UX_SUCCESS == ux_utility_memory_compare ("ux_host_class_video", host_class,
                                                 ux_utility_string_length_get ((UCHAR *)"ux_host_class_video")))
    {
        if ((UX_DEVICE_INSERTION == event) && (uvc_cfg.video_host_class == UX_NULL))
        {
            uvc_cfg.video_host_class = instance;
            tx_event_flags_set (&g_device_insert_eventflag, EVENTFLAG_USB_DEVICE_INSERTED, TX_OR);
        }
        else if ((UX_DEVICE_REMOVAL == event) && (uvc_cfg.video_host_class == instance))
        {
            uvc_cfg.video_host_class = NULL;
            tx_event_flags_set (&g_device_insert_eventflag, ~EVENTFLAG_USB_DEVICE_INSERTED, TX_AND);
        }
    }
    return UX_SUCCESS;
}

/* =====================================================================================================
 *  JPEG helpers
 * ===================================================================================================*/
static UINT jpeg_dht_insert(UCHAR *p, UINT img_size)
{
    if ((img_size < 4U) || (p[0] != 0xFF) || (p[1] != 0xD8)) return 0U;
    for (UINT i = 2U; (i + 1U) < img_size; i++)
    {
        if (p[i] != 0xFF) continue;
        if (p[i + 1U] == 0xC4) return img_size;
        if (p[i + 1U] == 0xDA)
        {
            UINT new_size = img_size + DHT_TABLE_SIZE;
            UINT pad      = (JPEG_BUFFER_ALIGN - (new_size % JPEG_BUFFER_ALIGN)) % JPEG_BUFFER_ALIGN;
            if ((new_size + pad) > JPEG_BUFFER_SIZE) return 0U;
            memmove(&p[i + DHT_TABLE_SIZE], &p[i], img_size - i);
            memcpy(&p[i], dht_table, DHT_TABLE_SIZE);
            memset(&p[new_size], 0, pad);
            return new_size + pad;
        }
    }
    return 0U;
}

/* EOI (FF D9) within the last 'window' bytes */
static bool jpeg_tail_eoi(const UCHAR *p, UINT size, UINT window)
{
    UINT stop = (size > window) ? (size - window) : 2U;
    for (UINT i = size; (i >= 2U) && (i > stop); i--)
    {
        if ((p[i - 2U] == 0xFF) && (p[i - 1U] == 0xD9)) return true;
    }
    return false;
}

static void jpeg_get_size(const UCHAR *p, UINT size, USHORT *w, USHORT *h)
{
    for (UINT i = 2U; (i + 8U) < size; i++)
    {
        if ((p[i] == 0xFF) && ((p[i + 1U] == 0xC0) || (p[i + 1U] == 0xC2)))
        {
            *h = (USHORT) ((p[i + 5U] << 8) | p[i + 6U]);
            *w = (USHORT) ((p[i + 7U] << 8) | p[i + 8U]);
            return;
        }
    }
}

/* =====================================================================================================
 *  Endpoint helpers
 * ===================================================================================================*/
static UINT vs_if_num(UX_HOST_CLASS_VIDEO *v)
{
    return v->ux_host_class_video_streaming_interface->ux_interface_descriptor.bInterfaceNumber;
}

static UX_INTERFACE *vs_first_if(UX_HOST_CLASS_VIDEO *v)
{
    return v->ux_host_class_video_streaming_interface->ux_interface_configuration->ux_configuration_first_interface;
}

/* count endpoint types, largest single-packet ISO endpoint; print the list when 'verbose' */
static void uvc_ep_survey(UX_HOST_CLASS_VIDEO *v, bool verbose)
{
    UINT if_num = vs_if_num(v);

    g_ep_limit       = 0;
    g_diag.n_iso     = 0;
    g_diag.n_iso_hb  = 0;
    g_diag.n_bulk    = 0;

    for (UX_INTERFACE *intf = vs_first_if(v); intf != UX_NULL; intf = intf->ux_interface_next_interface)
    {
        if (intf->ux_interface_descriptor.bInterfaceNumber != if_num) continue;

        for (UX_ENDPOINT *ep = intf->ux_interface_first_endpoint; ep != UX_NULL; ep = ep->ux_endpoint_next_endpoint)
        {
            USHORT mps  = ep->ux_endpoint_descriptor.wMaxPacketSize;
            UINT   size = mps & ISO_MPS_SIZE_MASK;
            UINT   mult = ((mps >> 11) & 0x03U) + 1U;
            UINT   type = ep->ux_endpoint_descriptor.bmAttributes & 0x03U;

            if (EP_TYPE_ISO == type)
            {
                g_diag.n_iso++;
                if (mult > 1U) g_diag.n_iso_hb++;
                if ((1U == mult) && (size > g_ep_limit) && (size <= UVC_RA_ISO_MAX_PACKET) && (size <= VIDEO_BUFFER_SIZE))
                {
                    g_ep_limit = size;
                }
            }
            else if (EP_TYPE_BULK == type)
            {
                g_diag.n_bulk++;
            }

            if (verbose)
            {
                con_print(true, "    alt %2u : EP 0x%02X %-4s %4u B x %u per uframe   %s",
                          intf->ux_interface_descriptor.bAlternateSetting, ep->ux_endpoint_descriptor.bEndpointAddress,
                          (EP_TYPE_ISO == type) ? "ISO" : ((EP_TYPE_BULK == type) ? "BULK" : "INT"), size, mult,
                          (EP_TYPE_BULK == type) ? "-> Bulk path" :
                          ((EP_TYPE_ISO != type) ? "" : ((1U == mult) ? "-> usable by RA8P1" : "-> high-bandwidth")));
            }
        }
    }
}

/* =====================================================================================================
 *  BULK path: control requests, pipe, read
 * ===================================================================================================*/
static UINT uvc_ctrl(UINT req, UINT type, UINT value, UINT index, UCHAR *buf, ULONG len)
{
    UX_HOST_CLASS_VIDEO *vc = thread_data->video_host_class;
    if (UX_NULL == vc)
    {
        return UX_ERROR;                                         /* camera removed */
    }
    UX_TRANSFER *tr = &vc->ux_host_class_video_device->ux_device_control_endpoint.ux_endpoint_transfer_request;

    tr->ux_transfer_request_data_pointer     = buf;
    tr->ux_transfer_request_requested_length = len;
    tr->ux_transfer_request_function         = req;
    tr->ux_transfer_request_type             = type;
    tr->ux_transfer_request_value            = value;
    tr->ux_transfer_request_index            = index;
    return ux_host_stack_transfer_request(tr);
}

/* PROBE (+ COMMIT) without the USBX 3072 B ISO payload check */
static UINT uvc_bulk_probe(UCHAR fmt, UCHAR frm, ULONG iv, bool commit, ULONG *payload, ULONG *maxframe)
{
    UINT vs = vs_if_num(thread_data->video_host_class);
    UINT st = UX_ERROR;

    for (UINT retry = 0; retry < 2U; retry++)
    {
        memset(g_probe_buf, 0, sizeof(g_probe_buf));
        g_probe_buf[0] = 0x01;                                   /* bmHint: dwFrameInterval fixed */
        g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_FORMAT_INDEX] = fmt;
        g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_FRAME_INDEX]  = frm;
        put_le32(&g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_FRAME_INTERVAL], iv);

        st = uvc_ctrl(UX_HOST_CLASS_VIDEO_SET_CUR, UX_REQUEST_OUT | UX_REQUEST_TYPE_CLASS | UX_REQUEST_TARGET_INTERFACE,
                      UX_HOST_CLASS_VIDEO_VS_PROBE_CONTROL << 8, vs, g_probe_buf, g_probe_len);
        if ((UX_SUCCESS != st) && (UVC_PROBE_LEN_11 == g_probe_len))
        {
            g_probe_len = UVC_PROBE_LEN_10;                      /* UVC 1.0: 26 byte probe */
            continue;
        }
        break;
    }
    if (UX_SUCCESS != st) return st;

    st = uvc_ctrl(UX_HOST_CLASS_VIDEO_GET_CUR, UX_REQUEST_IN | UX_REQUEST_TYPE_CLASS | UX_REQUEST_TARGET_INTERFACE,
                  UX_HOST_CLASS_VIDEO_VS_PROBE_CONTROL << 8, vs, g_probe_buf, g_probe_len);
    if (UX_SUCCESS != st) return st;

    *payload  = le32(&g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_MAX_PAYLOAD_TRANSFER_SIZE]);
    *maxframe = le32(&g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_MAX_VIDEO_FRAME_SIZE]);

    if ((g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_FORMAT_INDEX] != fmt) ||
        (g_probe_buf[UX_HOST_CLASS_VIDEO_PROBE_COMMIT_FRAME_INDEX]  != frm))
    {
        return UX_HOST_CLASS_VIDEO_PARAMETER_ERROR;
    }
    if (commit)
    {
        st = uvc_ctrl(UX_HOST_CLASS_VIDEO_SET_CUR, UX_REQUEST_OUT | UX_REQUEST_TYPE_CLASS | UX_REQUEST_TARGET_INTERFACE,
                      UX_HOST_CLASS_VIDEO_VS_COMMIT_CONTROL << 8, vs, g_probe_buf, g_probe_len);
    }
    return st;
}

static UX_ENDPOINT *uvc_find_bulk_ep(UX_HOST_CLASS_VIDEO *v)
{
    UINT if_num = vs_if_num(v);

    for (UX_INTERFACE *intf = vs_first_if(v); intf != UX_NULL; intf = intf->ux_interface_next_interface)
    {
        if ((intf->ux_interface_descriptor.bInterfaceNumber != if_num) ||
            (intf->ux_interface_descriptor.bAlternateSetting != 0U)) continue;

        for (UX_ENDPOINT *ep = intf->ux_interface_first_endpoint; ep != UX_NULL; ep = ep->ux_endpoint_next_endpoint)
        {
            if (((ep->ux_endpoint_descriptor.bmAttributes & UX_MASK_ENDPOINT_TYPE) == UX_BULK_ENDPOINT) &&
                ((ep->ux_endpoint_descriptor.bEndpointAddress & UX_ENDPOINT_DIRECTION) == UX_ENDPOINT_IN))
            {
                return ep;
            }
        }
    }
    return UX_NULL;
}

static UINT uvc_bulk_select_alt0(void)
{
    UX_HOST_CLASS_VIDEO *v      = thread_data->video_host_class;
    UINT                 if_num = vs_if_num(v);

    for (UX_INTERFACE *intf = vs_first_if(v); intf != UX_NULL; intf = intf->ux_interface_next_interface)
    {
        if ((intf->ux_interface_descriptor.bInterfaceNumber == if_num) &&
            (intf->ux_interface_descriptor.bAlternateSetting == 0U))
        {
            return ux_host_stack_interface_setting_select(intf);
        }
    }
    return UX_ERROR;
}

/* create a host pipe for the BULK IN endpoint; rm_usbx_port.c creates none for video */
static UINT bulk_pipe_open(void)
{
    UX_HOST_CLASS_VIDEO *vc   = thread_data->video_host_class;
    UCHAR               *d    = vc->ux_host_class_video_configuration_descriptor;
    ULONG                len  = vc->ux_host_class_video_configuration_descriptor_length;
    UCHAR                addr = g_bulk_ep->ux_endpoint_descriptor.bEndpointAddress;
    uint8_t             *p_ep = NULL;
    usb_pipe_table_reg_t ep_tbl;
    uint16_t             dev_addr;
    uint8_t              pipe = USB_NULL;

    bulk_pipe_close();

    for (ULONG pos = 0; (pos + 2U) < len; pos += d[pos])
    {
        if (d[pos] < 2U) break;
        if ((USB_DT_ENDPOINT_ID == d[pos + 1U]) && (d[pos + 2U] == addr))
        {
            p_ep = (uint8_t *) &d[pos];
            break;
        }
    }
    if (NULL == p_ep)
    {
        con_print(true, " !! BULK: endpoint descriptor 0x%02X not found", addr);
        return UX_ERROR;
    }

    for (uint8_t i = BULK_PIPE_FIRST; i <= BULK_PIPE_LAST; i++)
    {
        if (USB_TRUE != g_usb_pipe_table[UVC_USB_IP][i].use_flag)
        {
            pipe = i;
            break;
        }
    }
    if (USB_NULL == pipe)
    {
        con_print(true, " !! BULK: no free BULK pipe (PIPE%u..%u in use)", BULK_PIPE_FIRST, BULK_PIPE_LAST);
        return UX_ERROR;
    }

    dev_addr = (uint16_t) vc->ux_host_class_video_device->ux_device_address;
    uint16_t mps = (uint16_t) (p_ep[4] | (p_ep[5] << 8)) & 0x07FFU;

    memset(&ep_tbl, 0, sizeof(ep_tbl));
    ep_tbl.pipe_cfg  = (uint16_t) (USB_TYPFIELD_BULK | USB_DBLBON | USB_SHTNAKON | USB_DIR_H_IN | (p_ep[2] & 0x0FU));
    ep_tbl.pipe_buf  = usb_hstd_get_pipe_buf_value(pipe);
    ep_tbl.pipe_maxp = (uint16_t) (mps | (uint16_t) (dev_addr << USB_DEVADDRBIT));
    ep_tbl.pipe_peri = 0U;
    usb_hstd_set_pipe_info(UVC_USB_IP, pipe, &ep_tbl);

    memset(&g_bk_utr, 0, sizeof(g_bk_utr));
    g_bk_utr.ip  = UVC_USB_IP;
    g_bk_utr.ipp = usb_hstd_get_usb_ip_adr(UVC_USB_IP);
    usb_hstd_set_pipe_reg(&g_bk_utr, pipe);
    hw_usb_set_sqclr(&g_bk_utr, pipe);

    g_bk_pipe    = pipe;
    g_bk_pending = false;
    return UX_SUCCESS;
}

/* release the BULK pipe so that it is free for the next camera (ISO or BULK) */
static void bulk_pipe_close(void)
{
    if (0U == g_bk_pipe)
    {
        return;
    }
    g_bk_utr.ip  = UVC_USB_IP;
    g_bk_utr.ipp = usb_hstd_get_usb_ip_adr(UVC_USB_IP);
    usb_cstd_clr_pipe_cnfg(&g_bk_utr, g_bk_pipe);               /* PIPECFG / PIPEMAXP / PIPEPERI = 0 */
    g_usb_pipe_table[UVC_USB_IP][g_bk_pipe].use_flag = USB_FALSE;
    g_bk_pipe    = 0;
    g_bk_pending = false;
}

static void bulk_basic_cb(usb_utr_t *p_utr, uint16_t data1, uint16_t data2)
{
    (void) data1;
    (void) data2;
    g_bulk_actual = p_utr->read_req_len - p_utr->tranlen;
    g_bulk_code   = p_utr->status;
    g_bk_pending  = false;
    tx_semaphore_put(&g_bulk_sem);
}

/* BULK IN read. A pending transfer is kept across timeouts (camera may NAK for a while). */
static UINT bulk_read(ULONG len, ULONG *actual, ULONG *requested)
{
    if (!g_bk_pending)
    {
        while (TX_SUCCESS == tx_semaphore_get(&g_bulk_sem, TX_NO_WAIT)) {}
        dcache_invalidate(g_bulk_buf, sizeof(g_bulk_buf));

        g_bk_utr.keyword      = g_bk_pipe;
        g_bk_utr.p_tranadr    = g_bulk_buf;
        g_bk_utr.tranlen      = len;
        g_bk_utr.read_req_len = len;
        g_bk_utr.complete     = bulk_basic_cb;
        g_bk_utr.segment      = USB_TRAN_END;
        g_bk_utr.ip           = UVC_USB_IP;
        g_bk_utr.ipp          = usb_hstd_get_usb_ip_adr(UVC_USB_IP);
#if (USB_CFG_DMA == USB_CFG_ENABLE)
        g_bk_utr.p_transfer_tx = g_p_usbx_transfer_tx[UVC_USB_IP];
        g_bk_utr.p_transfer_rx = g_p_usbx_transfer_rx[UVC_USB_IP];
 #if defined(USB_CFG_HUVC_USE)
        g_bk_utr.p_tranadr_hold = NULL;
 #endif
#endif
        g_bk_req_len = len;
        g_bk_pending = true;
        if (USB_OK != usb_hstd_transfer_start(&g_bk_utr))
        {
            g_bk_pending = false;
            if (g_bk_dbg++ < 3U)
            {
                con_print(false, " !! BULK: transfer start error (pipe %u)", g_bk_pipe);
            }
            return UX_TRANSFER_ERROR;
        }
    }

    uint32_t tw = DWT->CYCCNT;
    UINT     ws = tx_semaphore_get(&g_bulk_sem, ticks_of(BULK_TIMEOUT_MS));
    g_bk_wait_cycles = (uint32_t) (DWT->CYCCNT - tw);
    if (TX_SUCCESS != ws)
    {
        return UX_TRANSFER_TIMEOUT;                              /* keep the transfer pending */
    }

    dcache_invalidate(g_bulk_buf, sizeof(g_bulk_buf));
    *actual    = g_bulk_actual;
    *requested = g_bk_req_len;

    if ((USB_DATA_OK == g_bulk_code) || (USB_DATA_SHT == g_bulk_code))
    {
        return UX_SUCCESS;
    }
    if (g_bk_dbg++ < 3U)
    {
        con_print(false, " !! BULK: transfer end status %d, %lu B", (int) g_bulk_code, g_bulk_actual);
    }
    return UX_TRANSFER_ERROR;
}

/* next read length: never cross the end of the current payload */
static ULONG bulk_next_len(void)
{
    ULONG remain;
    if (g_bk_in_payload && (g_bk_max_payload > g_bk_got)) remain = g_bk_max_payload - g_bk_got;
    else                                                  remain = g_bk_max_payload ? g_bk_max_payload : BULK_XFER_SIZE;
    return (remain < BULK_XFER_SIZE) ? remain : BULK_XFER_SIZE;
}

/* stop BULK streaming; 'camera_present' = false after hot-unplug (no control request possible) */
static void bulk_stop(bool camera_present)
{
    if ((0U != g_bk_pipe) && g_bk_pending)
    {
        g_bk_abort_utr.ip  = UVC_USB_IP;
        g_bk_abort_utr.ipp = usb_hstd_get_usb_ip_adr(UVC_USB_IP);
        usb_hstd_hcd_snd_mbx(&g_bk_abort_utr, USB_MSG_HCD_TRANSEND1, g_bk_pipe, (uint16_t *) 0, 0);
        (void) tx_semaphore_get(&g_bulk_sem, ticks_of(20U));
        g_bk_pending = false;
    }
#if BULK_STOP_CLEAR_HALT
    if (camera_present && (UX_NULL != g_bulk_ep))
    {
        (void) uvc_ctrl(UX_CLEAR_FEATURE, UX_REQUEST_OUT | UX_REQUEST_TYPE_STANDARD | UX_REQUEST_TARGET_ENDPOINT,
                        UX_ENDPOINT_HALT, g_bulk_ep->ux_endpoint_descriptor.bEndpointAddress, UX_NULL, 0);
    }
#else
    (void) camera_present;
#endif
    bulk_pipe_close();
    while (TX_SUCCESS == tx_semaphore_get(&g_bulk_sem, TX_NO_WAIT)) {}
    g_bk_in_payload = false;
}

/* =====================================================================================================
 *  Mode scan + software estimate
 * ===================================================================================================*/
static void uvc_add(UCHAR fmt, UCHAR frm, USHORT w, USHORT h, ULONG iv)
{
    UX_HOST_CLASS_VIDEO *vc = thread_data->video_host_class;
    UINT                 st;
    ULONG                payload  = 0;
    ULONG                maxframe = 0;

    if ((w > UVC_AUTO_MAX_WIDTH) || (h > UVC_AUTO_MAX_HEIGHT) || (0U == iv) ||
        (fps_x10(iv) > UVC_AUTO_MAX_FPS * 10U) || (fps_x10(iv) < UVC_AUTO_MIN_FPS * 10U))
    {
        g_diag.n_range++;
        return;
    }
    for (UINT i = 0; i < g_cand_num; i++)                        /* same size listed twice in the descriptor */
    {
        if ((g_cand[i].w == w) && (g_cand[i].h == h) && (g_cand[i].interval == iv)) return;
    }
    if (g_cand_num >= UVC_MAX_CAND) return;

    if (g_bulk_mode)
    {
        if (0U == fmt) return;
        st = uvc_bulk_probe(fmt, frm, iv, false, &payload, &maxframe);
    }
    else
    {
        st = ux_host_class_video_frame_parameters_set(vc, UX_HOST_CLASS_VIDEO_VS_FORMAT_MJPEG, w, h, iv);
        if (UX_SUCCESS == st) payload = ux_host_class_video_max_payload_get(vc);
    }
    if ((UX_SUCCESS != st) || (0U == payload))
    {
        g_diag.n_probe_fail++;
        g_diag.last_probe_status = st;
        return;
    }

    uvc_cand_t *c = &g_cand[g_cand_num++];
    memset(c, 0, sizeof(*c));
    c->w = w; c->h = h; c->interval = iv; c->payload = payload; c->maxframe = maxframe; c->fmt = fmt; c->frm = frm;
    c->est_bytes = est_frame_bytes(w, h, maxframe);

    /* software estimate: USB packet limit first, then frame buffer */
    if (!g_bulk_mode && (payload > g_ep_limit))
    {
        c->est = EST_NG_PACKET;
    }
    else if (c->est_bytes > FB_USABLE_BYTES)
    {
        c->est = EST_NG_BUFFER;
    }
    else
    {
        c->est = EST_OK;
    }
}

static UINT uvc_scan_descriptor(void)
{
    UX_HOST_CLASS_VIDEO *vc      = thread_data->video_host_class;
    UCHAR               *d       = vc->ux_host_class_video_configuration_descriptor;
    ULONG                len     = vc->ux_host_class_video_configuration_descriptor_length;
    UINT                 if_num  = vs_if_num(vc);
    bool                 in_vs   = false, mjpeg = false;
    UINT                 frames  = 0;
    UCHAR                fmt_idx = 0;

    if ((UX_NULL == d) || (0U == len)) return 0U;

    for (ULONG pos = 0; (pos + 2U) < len; )
    {
        UCHAR bl = d[pos], dt = d[pos + 1U];
        if ((bl < 2U) || ((pos + bl) > len)) break;

        if (USB_DT_INTERFACE_ID == dt)
        {
            in_vs = (d[pos + 2U] == if_num) && (d[pos + 3U] == 0U);
            mjpeg = false;
        }
        else if (in_vs && (UVC_CS_INTERFACE == dt) && (bl >= 3U))
        {
            UCHAR sub = d[pos + 2U];
            switch (sub)
            {
                case UVC_VS_FORMAT_MJPEG:        g_diag.n_fmt_mjpeg++;  mjpeg = true;  fmt_idx = d[pos + 3U]; break;
                case UVC_VS_FORMAT_UNCOMPRESSED: g_diag.n_fmt_uncomp++; mjpeg = false; break;
                case UVC_VS_FORMAT_FRAME_BASED:  g_diag.n_fmt_frame++;  mjpeg = false; break;
                case UVC_VS_FORMAT_STREAM_BASED: mjpeg = false; break;
                case UVC_VS_FRAME_UNCOMPRESSED:  g_diag.n_frm_uncomp++; break;
                case UVC_VS_FRAME_FRAME_BASED:   g_diag.n_frm_frame++;  break;
                case UVC_VS_FRAME_MJPEG:         g_diag.n_frm_mjpeg++;  break;
                default: break;
            }
            if (mjpeg && (UVC_VS_FRAME_MJPEG == sub) && (bl >= 26U))
            {
                const UCHAR *f = &d[pos];
                USHORT w = (USHORT) (f[5] | (f[6] << 8)), h = (USHORT) (f[7] | (f[8] << 8));
                UCHAR  ntype = f[25];
                frames++;
                if (0U == ntype)
                {
                    ULONG imin = le32(&f[26]), imax = le32(&f[30]);
                    for (UINT k = 0; k < array_count(g_interval_list); k++)
                        if ((g_interval_list[k] >= imin) && (g_interval_list[k] <= imax))
                            uvc_add(fmt_idx, f[3], w, h, g_interval_list[k]);
                }
                else
                {
                    for (UINT k = 0; (k < ntype) && (k < UVC_MAX_INTERVALS) && ((30U + 4U * k) <= bl); k++)
                        uvc_add(fmt_idx, f[3], w, h, le32(&f[26U + 4U * k]));
                }
            }
        }
        pos += bl;
    }
    return frames;
}

static int cand_cmp(const uvc_cand_t *a, const uvc_cand_t *b)
{
    uint64_t pa = (uint64_t) a->w * a->h, pb = (uint64_t) b->w * b->h;
    if (pa != pb) return (pa > pb) ? -1 : 1;
    if (a->interval != b->interval) return (a->interval < b->interval) ? -1 : 1;
    return 0;
}

static void ui_camera_summary(void)
{
    UX_DEVICE *dev = thread_data->video_host_class->ux_host_class_video_device;
    UINT       n_ok = 0;

    for (UINT i = 0; i < g_cand_num; i++)
    {
        if (EST_OK == g_cand[i].est) n_ok++;
    }

    con_print(true, "");
    con_print(true, " ============================== USB camera attached ==============================");
    con_print(true, "  Device   : VID %04X  PID %04X   USB %s",
              dev->ux_device_descriptor.idVendor, dev->ux_device_descriptor.idProduct,
              (UX_HIGH_SPEED_DEVICE == dev->ux_device_speed) ? "High-Speed (480 Mbps)" : "Full/Low-Speed");
    if (g_bulk_mode)
    {
        con_print(true, "  Transfer : BULK -> Bulk streaming path (no per-micro-frame packet limit)");
    }
    else if (0U != g_diag.n_iso)
    {
        con_print(true, "  Transfer : ISOCHRONOUS -> RA8P1 receives 1 packet per micro-frame, max %u B%s",
                  g_ep_limit, (0U != g_diag.n_iso_hb) ? " (camera also offers 2-3 packet modes)" : "");
    }
    else
    {
        con_print(true, "  Transfer : no Isochronous / Bulk IN video endpoint -> not supported");
    }
    con_print(true, "  Formats  : MJPEG %u sizes,  YUY2 %u,  H.264/frame-based %u   (this demo streams MJPEG)",
              g_diag.n_frm_mjpeg, g_diag.n_frm_uncomp, g_diag.n_frm_frame);
    con_print(true, "  Buffer   : %u KB x %u in %s, usable %lu KB per frame",
              JPEG_BUFFER_SIZE / 1024U, JPEG_BUFFER_NUMBER, FB_BUFFER_MEM_NAME, (ULONG) FB_USABLE_BYTES / 1024U);
    con_print(true, "  Modes    : %u MJPEG modes in scan range (<= %ux%u, %u..%u fps), software estimate: OK %u, NG %u",
              g_cand_num, UVC_AUTO_MAX_WIDTH, UVC_AUTO_MAX_HEIGHT, UVC_AUTO_MIN_FPS, UVC_AUTO_MAX_FPS,
              n_ok, g_cand_num - n_ok);
    if (0U == g_cand_num)
    {
        if ((0U == g_diag.n_iso) && (0U == g_diag.n_bulk))
            con_print(true, "  Reason   : the camera has no video IN endpoint");
        else if (0U == g_diag.n_fmt_mjpeg)
            con_print(true, "  Reason   : the camera offers no MJPEG format");
        else if (0U != g_diag.n_probe_fail)
            con_print(true, "  Reason   : MJPEG negotiation (PROBE) failed, code 0x%X", g_diag.last_probe_status);
        else
            con_print(true, "  Reason   : all MJPEG modes are outside the scan range (UVC_AUTO_MAX_* / MIN_FPS)");
    }
}

static void uvc_scan(void)
{
    UX_HOST_CLASS_VIDEO *vc = thread_data->video_host_class;

    g_ready = false;
    memset(&g_diag, 0, sizeof(g_diag));
    uvc_ep_survey(vc, false);

    /* transfer type decides the path */
    g_probe_len = UVC_PROBE_LEN_11;
    g_bulk_ep   = UX_NULL;
    g_bulk_mode = false;
    if ((0U == g_diag.n_iso) && (0U != g_diag.n_bulk))
    {
        g_bulk_ep   = uvc_find_bulk_ep(vc);
        g_bulk_mode = (UX_NULL != g_bulk_ep) && g_bulk_sem_ok;
    }

    g_cand_num = 0;
    if ((0U == uvc_scan_descriptor()) && (0U == g_diag.n_fmt_mjpeg + g_diag.n_fmt_uncomp + g_diag.n_fmt_frame) &&
        !g_bulk_mode && (0U != g_diag.n_iso))
    {
        for (UINT r = 0; r < array_count(g_res_list); r++)                 /* descriptor not readable */
            for (UINT i = 0; i < array_count(g_interval_list); i++)
                uvc_add(0U, 0U, g_res_list[r].w, g_res_list[r].h, g_interval_list[i]);
    }

    for (UINT i = 1; i < g_cand_num; i++)
    {
        uvc_cand_t key = g_cand[i];
        int j = (int) i - 1;
        while ((j >= 0) && (cand_cmp(&g_cand[j], &key) > 0)) { g_cand[j + 1] = g_cand[j]; j--; }
        g_cand[j + 1] = key;
    }
    g_ready = (g_cand_num > 0U);

    ui_camera_summary();
    if (g_ready)
    {
        ui_list(false);
        ui_legend();
    }
    ui_help();
}

/* =====================================================================================================
 *  Console UI
 * ===================================================================================================*/
static void ui_help(void)
{
    con_print(true, "  Commands : <No.> run mode | c capture + stop | x stop");
    con_print(true, "             l list modes | p OK modes only | d device details | s rescan | h help");
}

/* modes grouped by resolution: "No.) fps status" */
static void ui_list(bool ok_only)
{
    char line[128];
    UINT i = 0;

    con_print(true, " ---------------------------------------------------------------------------------");
    con_print(true, "  Resolution |  No.) fps status   (status = software estimate)");
    con_print(true, " ---------------------------------------------------------------------------------");
    while (i < g_cand_num)
    {
        USHORT w = g_cand[i].w, h = g_cand[i].h;
        UINT   j = i, n = 0;
        int    pos = snprintf(line, sizeof(line), "  %4ux%-5u |", w, h);

        for (; (j < g_cand_num) && (g_cand[j].w == w) && (g_cand[j].h == h); j++)
        {
            uvc_cand_t *c = &g_cand[j];
            if (ok_only && (EST_OK != c->est)) continue;
            if (UI_ITEMS_PER_LINE == n)
            {
                con_print(true, "%s", line);
                pos = snprintf(line, sizeof(line), "             |");
                n   = 0;
            }
            if ((pos > 0) && ((size_t) pos < sizeof(line)))
            {
                pos += snprintf(&line[pos], sizeof(line) - (size_t) pos, " %3u) %2lu %-4s ",
                                j + 1U, fps_round(c->interval), est_name(c));
            }
            n++;
        }
        if (n > 0U)
        {
            con_print(true, "%s", line);
        }
        i = j;
    }
}

static void ui_legend(void)
{
    con_print(true, " ---------------------------------------------------------------------------------");
    con_print(true, "  Status is a software estimate only. The actual result is the hardware test output:");
    con_print(true, "  run the mode (<No.> + Enter) and check 'Result' / 'Frames' in the test report.");
    if (g_bulk_mode)
    {
        con_print(true, "  OK   = Bulk streaming and estimated JPEG frame <= %lu KB buffer",
                  (ULONG) FB_USABLE_BYTES / 1024U);
    }
    else
    {
        con_print(true, "  OK   = PROBE payload <= %u B per micro-frame and estimated JPEG frame <= %lu KB buffer",
                  g_ep_limit, (ULONG) FB_USABLE_BYTES / 1024U);
        con_print(true, "  NG-P = PROBE payload > %u B: RA8P1 receives 1 packet (<= 1024 B) per micro-frame",
                  g_ep_limit);
    }
    con_print(true, "  NG-B = estimated JPEG frame (%u.%02u bit/pixel) > %lu KB %s buffer%s",
              UVC_MJPEG_EST_BPP_X100 / 100U, UVC_MJPEG_EST_BPP_X100 % 100U, (ULONG) FB_USABLE_BYTES / 1024U,
              FB_BUFFER_MEM_NAME, UVC_FB_IN_SDRAM ? "" : " -> UVC_FB_IN_SDRAM = 1 for larger frames");
    con_print(true, "  NG modes can still be run.");
}

static void ui_details(void)
{
    con_print(true, " ---------------------------------------------------------------------------------");
    con_print(true, "  VideoStreaming interface %u, endpoints:", vs_if_num(thread_data->video_host_class));
    uvc_ep_survey(thread_data->video_host_class, true);
    con_print(true, "  MJPEG formats %u (%u sizes), uncompressed %u (%u sizes), frame-based %u (%u sizes)",
              g_diag.n_fmt_mjpeg, g_diag.n_frm_mjpeg, g_diag.n_fmt_uncomp, g_diag.n_frm_uncomp,
              g_diag.n_fmt_frame, g_diag.n_frm_frame);
    con_print(true, "  Modes: %u listed, %u PROBE failed (last 0x%X), %u outside scan range",
              g_cand_num, g_diag.n_probe_fail, g_diag.last_probe_status, g_diag.n_range);
    if (g_bulk_mode)
    {
        con_print(true, "  BULK : EP 0x%02X, PIPE%u..%u, read %u B, PROBE length %lu",
                  g_bulk_ep ? g_bulk_ep->ux_endpoint_descriptor.bEndpointAddress : 0U,
                  BULK_PIPE_FIRST, BULK_PIPE_LAST, BULK_XFER_SIZE, g_probe_len);
    }
}

/* =====================================================================================================
 *  Test start / stop
 * ===================================================================================================*/
static void frame_state_reset(void)
{
    g_frame_index = 0;
    g_frame_drop  = 0;
    g_frame_error = g_frame_ovf = false;
    g_last_fid    = -1;
}

static void perf_reset(void)
{
    memset(&g_perf, 0, sizeof(g_perf));
    g_perf.start_tick = g_perf.w_start_tick = tx_time_get();
    g_perf.frame_min  = g_perf.fps_min_x10 = g_perf.interval_min_ms = 0xFFFFFFFFU;
    g_jpeg_w = g_jpeg_h = 0;
    fb_data.p_pointer = NULL;
    fb_data.size      = 0;
}

static bool test_start_bulk(uvc_cand_t *c)
{
    ULONG payload = 0, maxframe = 0;
    UINT  status  = uvc_bulk_probe(c->fmt, c->frm, c->interval, true, &payload, &maxframe);

    if ((UX_SUCCESS != status) || (0U == payload))
    {
        con_print(true, " !! BULK negotiation (PROBE/COMMIT) failed, code 0x%X", status);
        return false;
    }
    (void) uvc_bulk_select_alt0();
    g_bulk_ep = uvc_find_bulk_ep(thread_data->video_host_class);
    if (UX_NULL == g_bulk_ep)
    {
        con_print(true, " !! BULK IN endpoint not found");
        return false;
    }
    g_bk_dbg = 0;
    if (UX_SUCCESS != bulk_pipe_open())
    {
        return false;
    }
    g_bk_max_payload = payload;
    g_bk_in_payload  = false;
    g_bk_got         = 0;
    g_bk_timeouts    = 0;
    g_ep_mps_raw     = (USHORT) g_bulk_ep->ux_endpoint_descriptor.wMaxPacketSize;
    return true;
}

static bool test_start_iso(uvc_cand_t *c)
{
    UX_HOST_CLASS_VIDEO *vc = thread_data->video_host_class;
    UINT                 status;

    if (c->payload > VIDEO_BUFFER_SIZE)
    {
        con_print(true, " !! requested payload %lu B > VIDEO_BUFFER_SIZE (%u B)", c->payload, VIDEO_BUFFER_SIZE);
        return false;
    }
    status = ux_host_class_video_frame_parameters_set(vc, UX_HOST_CLASS_VIDEO_VS_FORMAT_MJPEG, c->w, c->h, c->interval);
    if (UX_SUCCESS != status)
    {
        con_print(true, " !! MJPEG negotiation failed, code 0x%X", status);
        return false;
    }
    status = ux_host_class_video_start(vc);
    if (UX_SUCCESS != status)
    {
        con_print(true, " !! video start failed, code 0x%X", status);
        (void) ux_host_class_video_stop(vc);
        return false;
    }
    UX_ENDPOINT *ep = vc->ux_host_class_video_isochronous_endpoint;
    g_ep_mps_raw = (UX_NULL != ep) ? (USHORT) ep->ux_endpoint_descriptor.wMaxPacketSize : 0U;

    xfer_reset();
    for (UINT i = 0; (i < VIDEO_XFER_PREQUEUE) && (i < VIDEO_XFER_NUM); i++)
    {
        if (UX_SUCCESS != ux_host_class_video_transfer_buffer_add(vc, thread_data->video_data_ptrs[i])) break;
    }
    return true;
}

static void test_start(UINT no)
{
    if (!g_ready || (no < 1U) || (no > g_cand_num))
    {
        con_print(true, " !! no mode No.%u (valid: 1..%u)", no, g_cand_num);
        return;
    }
    uvc_cand_t *c  = &g_cand[no - 1U];
    bool        ok;

    g_sel_idx = no - 1U;
    frame_state_reset();
    perf_reset();

    ok = g_bulk_mode ? test_start_bulk(c) : test_start_iso(c);
    if (!ok)
    {
        return;
    }
    g_test_count++;
    g_video_status = START;
    g_perf.start_tick = g_perf.w_start_tick = tx_time_get();

    con_print(true, "");
    con_print(true, " >> Test %u : No.%u  %ux%u @ %lu fps  [%s]    c = capture + stop,  x = stop",
              g_test_count, no, c->w, c->h, fps_round(c->interval), g_bulk_mode ? "BULK" : "ISO");
    con_print(true, "    Software estimate: %s (payload %lu B, est. JPEG %lu KB / buffer %lu KB) - hardware result follows",
              est_name(c), c->payload, c->est_bytes / 1024U, (ULONG) FB_USABLE_BYTES / 1024U);
    if (EST_NG_PACKET == c->est)
    {
        con_print(true, "    NG-P: data beyond 1 packet per micro-frame is lost; 'invalid' rising = not usable");
    }
    else if (EST_NG_BUFFER == c->est)
    {
        con_print(true, "    NG-B: frames larger than the buffer are dropped ('too large' in the report)");
    }
}

/* =====================================================================================================
 *  Report (hardware result)
 * ===================================================================================================*/
static void perf_final_report(const char *end_reason)
{
    uvc_cand_t *c     = &g_cand[g_sel_idx];
    ULONG dur         = ms_of(tx_time_get() - g_perf.start_tick);
    ULONG span        = ms_of(g_perf.last_frame_tick - g_perf.first_frame_tick);
    ULONG startup     = (g_perf.frames_valid > 0U) ? g_perf.startup_drop : 0U;
    ULONG invalid     = g_perf.frames_invalid - startup;
    ULONG tot         = g_perf.frames_valid + invalid;
    ULONG sel         = fps_x10(c->interval);
    ULONG f           = ((g_perf.frames_valid > 1U) && (span > 0U)) ?
                        (ULONG) (((uint64_t) (g_perf.frames_valid - 1U) * 10000U) / span) : 0U;
    ULONG inv         = tot ? (invalid * 1000U) / tot : 0U;
    ULONG avg         = g_perf.frames_valid ? (ULONG) (g_perf.jpeg_bytes / g_perf.frames_valid) : 0U;
    ULONG rate_x10    = dur ? (ULONG) ((g_perf.raw_bytes * 10000U) / dur / (1024U * 1024U)) : 0U;   /* MB/s x10 */
    ULONG tps         = dur ? (ULONG) (((uint64_t) g_perf.transfers * 1000U) / dur) : 0U;
    uint64_t tc       = (uint64_t) dur * (SystemCoreClock / 1000U);
    ULONG cpu         = tc ? (ULONG) ((g_perf.proc_cycles * 1000U) / tc) : 0U;
    bool  judged      = (dur >= PERF_JUDGE_MIN_MS) || (tot >= 10U);
    bool  fpsok       = (f * 100U) >= (sel * PERF_FPS_PASS_PCT);
    bool  intok       = (g_perf.frames_valid > 0U) && (inv <= PERF_INVALID_PASS_X10);
    bool  buf_limited = (g_perf.inv_ovf * 2U) >= invalid;          /* most invalid frames did not fit the buffer */
    char  causes[96]  = "";

    if (0xFFFFFFFFU == g_perf.frame_min)       g_perf.frame_min = 0;
    if (0xFFFFFFFFU == g_perf.fps_min_x10)     g_perf.fps_min_x10 = f;
    if (0xFFFFFFFFU == g_perf.interval_min_ms) g_perf.interval_min_ms = 0;
    if (invalid)
    {
        snprintf(causes, sizeof(causes), "  [too large %lu, no start %lu, no end %lu, packet error %lu]",
                 g_perf.inv_ovf, g_perf.inv_no_soi, g_perf.inv_no_eoi, g_perf.inv_err);
    }

    con_print(true, " ---------- Test %u result : No.%u  %ux%u @ %lu fps  [%s]  (%s) ----------",
              g_test_count, g_sel_idx + 1U, c->w, c->h, fps_round(c->interval), g_bulk_mode ? "BULK" : "ISO",
              end_reason);
    if (!judged)
        con_print(true, "  Result  : not judged (run the mode for at least 1 s)");
    else if (intok && fpsok)
        con_print(true, "  Result  : PASS - complete frames at target rate on this board");
    else if (intok)
        con_print(true, "  Result  : PASS - complete frames, rate below target (camera limited: light / exposure)");
    else if ((0U != g_perf.inv_ovf) && buf_limited)
        con_print(true, "  Result  : FAIL - frames up to %lu KB do not fit the %lu KB buffer (USB link is OK)",
                  (g_perf.ovf_max + 1023U) / 1024U, (ULONG) FB_USABLE_BYTES / 1024U);
    else
        con_print(true, "  Result  : FAIL - %lu.%lu %% invalid frames on this board", inv / 10U, inv % 10U);
    con_print(true, "  Estimate: %s (software)  -> the Result above is the hardware result", est_name(c));

    if (0U != g_jpeg_w)
    {
        con_print(true, "  Image   : %ux%u JPEG%s   size min / avg / max = %lu / %lu / %lu KB", g_jpeg_w, g_jpeg_h,
                  ((g_jpeg_w == c->w) && (g_jpeg_h == c->h)) ? "" : " (differs from request!)",
                  g_perf.frame_min / 1024U, avg / 1024U, g_perf.frame_max / 1024U);
    }
    else
    {
        con_print(true, "  Image   : no complete frame received");
    }
    if (NULL != fb_data.p_pointer)
    {
        con_print(true, "  Capture : fb_data.p_pointer = 0x%08lX (%lu B)  -> e2 studio Memory view, Image rendering",
                  (ULONG) (uintptr_t) fb_data.p_pointer, (ULONG) fb_data.size);
    }
    con_print(true, "  Rate    : %lu.%lu fps (1 s min %lu.%lu / max %lu.%lu, target %lu)   frame interval %lu..%lu ms",
              f / 10U, f % 10U, g_perf.fps_min_x10 / 10U, g_perf.fps_min_x10 % 10U,
              g_perf.fps_max_x10 / 10U, g_perf.fps_max_x10 % 10U, fps_round(c->interval),
              g_perf.interval_min_ms, g_perf.interval_max_ms);
    con_print(true, "  Frames  : %lu complete, %lu invalid (%lu.%lu %%)%s", g_perf.frames_valid, invalid,
              inv / 10U, inv % 10U, causes);
    if (g_bulk_mode)
    {
        con_print(true, "  USB     : BULK %lu.%lu MB/s, %lu reads/s, NAK timeouts %lu", rate_x10 / 10U, rate_x10 % 10U,
                  tps, g_bk_timeouts);
    }
    else
    {
        UINT  pk   = g_ep_mps_raw & ISO_MPS_SIZE_MASK;
        UINT  mult = ((g_ep_mps_raw >> 11) & 0x03U) + 1U;
        ULONG full = g_perf.transfers ? (g_perf.full_packets * 1000U) / g_perf.transfers : 0U;
        con_print(true, "  USB     : ISO %lu.%lu MB/s, endpoint %u B x %u per uframe, %lu packets/s, packet errors %lu",
                  rate_x10 / 10U, rate_x10 % 10U, pk, mult, tps, g_perf.err_packets);
        if (mult > 1U)
        {
            con_print(true, "  Load    : largest packet %lu B, %lu.%lu %% of micro-frames hit the RA8P1 1-packet limit",
                      g_perf.max_packet, full / 10U, full % 10U);
        }
    }
    con_print(true, "  CPU     : load %lu.%lu %%, DHT insert %lu us/frame", cpu / 10U, cpu % 10U,
              g_perf.frames_valid ? cyc_to_us(g_perf.dht_cycles / g_perf.frames_valid) : 0U);
    if (g_perf.inv_ovf)
    {
        con_print(true, "  Note    : %lu frame(s) > %lu KB buffer (largest %lu KB) -> enlarge JPEG_BUFFER_SIZE / use SDRAM",
                  g_perf.inv_ovf, (ULONG) FB_USABLE_BYTES / 1024U, (g_perf.ovf_max + 1023U) / 1024U);
    }
    con_print(true, " ---------------------------------------------------------------------------------");
    con_print(true, "  Next: <No.> + Enter, 'l' list, 'p' OK modes only");
}

static void test_stop(const char *reason)
{
    if (g_bulk_mode)
    {
        bulk_stop(true);
    }
    else if (NULL != thread_data->video_host_class)
    {
        (void) ux_host_class_video_stop(thread_data->video_host_class);
    }
    g_video_status = STOP;
    xfer_reset();
    frame_state_reset();
    dcache_clean(fb_data.p_pointer, fb_data.size);              /* debugger reads memory, not the cache */
    perf_final_report(reason);
}

/* camera removed (hot-unplug): release resources without any USB request to the camera */
static void camera_removed(void)
{
    bool was_streaming = (STOP != g_video_status);

    if (g_bulk_mode)
    {
        bulk_stop(false);
    }
    g_video_status = STOP;
    xfer_reset();
    frame_state_reset();
    g_ready     = false;
    g_bulk_mode = false;
    g_bulk_ep   = UX_NULL;
    g_cand_num  = 0;

    con_print(true, "");
    con_print(true, " >> USB camera removed%s. Waiting for a USB camera ...",
              was_streaming ? " during the test" : "");
}

/* =====================================================================================================
 *  Console input (line based, RTT or UART)
 *    - a letter command runs at once (no Enter needed)
 *    - a number runs on Enter, or after UI_NUMBER_IDLE_MS without input
 * ===================================================================================================*/
static void ui_execute(char *s)
{
    while ((*s == ' ') || (*s == '\t')) s++;
    char c0   = (char) tolower((unsigned char) *s);
    bool idle = (STOP == g_video_status);

    if ('\0' == c0) return;
    if (NULL == thread_data->video_host_class)
    {
        con_print(true, "  no USB camera attached");
        return;
    }
    if (isdigit((unsigned char) c0))
    {
        if (idle) test_start((UINT) atoi(s));
        else      con_print(true, "  streaming: press 'c' or 'x' first");
        return;
    }
    switch (c0)
    {
        case 'c':
            if (START == g_video_status)
            {
                g_video_status = CAPTURE_IMAGE;                 /* stop at the next frame boundary */
                g_capture_tick = tx_time_get();
            }
            break;
        case 'x':
            if (!idle) test_stop("stopped");
            break;
        case 'l':
            if (idle && g_ready) { ui_list(false); ui_legend(); }
            break;
        case 'p':
            if (idle && g_ready) { ui_list(true); ui_legend(); }
            break;
        case 'd':
            if (idle) ui_details();
            break;
        case 's':
            if (idle) uvc_scan();
            break;
        default:
            ui_help();
            break;
    }
}

static void ui_line_submit(void)
{
    char cmd[UI_LINE_MAX];

    uvc_console_echo('\n');
    g_line[g_line_len] = '\0';
    memcpy(cmd, g_line, g_line_len + 1U);
    g_line_len = 0;
    ui_execute(cmd);
}

static void ui_command(void)
{
    char     rx[UVC_CONSOLE_RX_CHUNK];
    uint32_t n = uvc_console_read(rx, sizeof(rx));

    if (n > 0U)
    {
        UINT chunk = 0;

        for (uint32_t i = 0; i < n; i++)
        {
            char ch = rx[i];

            if ((ch == '\r') || (ch == '\n'))
            {
                if (g_line_len > 0U) ui_line_submit();
                chunk = 0;
            }
            else if ((ch == '\b') || (ch == 0x7F))
            {
                if (g_line_len > 0U)
                {
                    g_line_len--;
                    uvc_console_echo('\b');
                }
            }
            else if (isprint((unsigned char) ch) && (g_line_len < (UI_LINE_MAX - 1U)))
            {
                g_line[g_line_len++] = ch;
                uvc_console_echo(ch);
                chunk++;
            }
        }
        if (g_line_len > 0U)
        {
            /* letter: run at once.
             * RTT "Send on Enter": the whole line arrives in one message -> run at once.
             * UART: bytes arrive one by one -> a number runs on Enter or after UI_NUMBER_IDLE_MS. */
#if (UVC_CONSOLE_TYPE == UVC_CONSOLE_RTT)
            bool whole_line = (chunk >= 2U);
#else
            bool whole_line = false;
            (void) chunk;
#endif
            if (!isdigit((unsigned char) g_line[0]) || whole_line)
            {
                ui_line_submit();
            }
        }
        g_line_tick = tx_time_get();
    }

    /* number typed char by char without Enter */
    if ((g_line_len > 0U) && (ms_of(tx_time_get() - g_line_tick) >= UI_NUMBER_IDLE_MS))
    {
        ui_line_submit();
    }
}

/* =====================================================================================================
 *  Frame assembly (shared by ISO and BULK)
 * ===================================================================================================*/
static void frame_begin_payload(UCHAR info)
{
    int fid = (info & UVC_STREAM_HEADER_HEADERINFO_FID) ? 1 : 0;

    if ((g_last_fid >= 0) && (fid != g_last_fid) && ((g_frame_index > 0U) || (g_frame_drop > 0U)))
    {
        frame_end_payload(UVC_STREAM_HEADER_HEADERINFO_EOF);       /* FID toggle = previous frame ended */
        if (STOP == g_video_status)
        {
            return;
        }
    }
    g_last_fid = fid;
    if (info & UVC_STREAM_HEADER_HEADERINFO_ERR)
    {
        g_frame_error = true;
        g_perf.err_packets++;
    }
}

static void frame_append(const UCHAR *data, UINT dlen)
{
    if (0U == dlen) return;
    if (!g_frame_ovf && ((g_frame_index + dlen) <= FB_USABLE_BYTES))
    {
        memcpy(&fb_buffer[g_fb_index][g_frame_index], data, dlen);
        g_frame_index += dlen;
    }
    else
    {
        g_frame_ovf   = true;
        g_frame_drop += dlen;                                      /* keep counting for the size report */
    }
}

static void frame_end_payload(UCHAR info)
{
    if (0U == (info & UVC_STREAM_HEADER_HEADERINFO_EOF)) return;
    if ((0U == g_frame_index) && (0U == g_frame_drop))
    {
        g_frame_error = g_frame_ovf = false;
        return;
    }

    UCHAR *p   = &fb_buffer[g_fb_index][0];
    UINT   jpg = 0;
    bool   soi = (g_frame_index >= 4U) && (p[0] == 0xFF) && (p[1] == 0xD8);
    bool   eoi = (0 == JPEG_REQUIRE_EOI) || jpeg_tail_eoi(p, g_frame_index, JPEG_EOI_SEARCH_BYTES);

    if (g_frame_ovf)
    {
        ULONG total = g_frame_index + g_frame_drop;
        g_perf.inv_ovf++;
        if (total > g_perf.ovf_max) g_perf.ovf_max = total;
    }
    else if (g_frame_error) g_perf.inv_err++;
    else if (!soi)          g_perf.inv_no_soi++;
    else if (!eoi)          g_perf.inv_no_eoi++;
    else
    {
        uint32_t t1 = DWT->CYCCNT;
        jpg = jpeg_dht_insert(p, g_frame_index);
        g_perf.dht_cycles += (uint32_t) (DWT->CYCCNT - t1);
        if (0U == jpg) g_perf.inv_no_soi++;
    }

    if (jpg)
    {
        ULONG now = tx_time_get();
        if (0U == g_perf.frames_valid)
        {
            g_perf.first_frame_tick = now;
        }
        g_perf.frames_valid++; g_perf.w_frames++;
        g_perf.jpeg_bytes += g_frame_index;
        if (g_frame_index < g_perf.frame_min) g_perf.frame_min = g_frame_index;
        if (g_frame_index > g_perf.frame_max) g_perf.frame_max = g_frame_index;
        if (g_perf.last_frame_tick)
        {
            ULONG iv = ms_of(now - g_perf.last_frame_tick);
            if (iv < g_perf.interval_min_ms) g_perf.interval_min_ms = iv;
            if (iv > g_perf.interval_max_ms) g_perf.interval_max_ms = iv;
        }
        g_perf.last_frame_tick = now;
        if (0U == g_jpeg_w) jpeg_get_size(p, jpg, &g_jpeg_w, &g_jpeg_h);
        fb_data.p_pointer = p;                                      /* latest complete frame */
        fb_data.size      = jpg;
        g_fb_index        = (g_fb_index + 1U) % JPEG_BUFFER_NUMBER;
    }
    else
    {
        g_perf.frames_invalid++; g_perf.w_invalid++;                /* last complete frame stays in fb_data */
        if (0U == g_perf.frames_valid)
        {
            g_perf.startup_drop++;                                  /* partial frames before the first good one */
        }
    }
    g_frame_index = 0;
    g_frame_drop  = 0;
    g_frame_error = g_frame_ovf = false;

    if (CAPTURE_IMAGE == g_video_status)                            /* 'c': stop at this frame boundary */
    {
        test_stop(jpg ? "captured" : ((NULL != fb_data.p_pointer) ? "captured last complete frame" : "stopped"));
    }
}

/* ISO: one transfer = one payload */
static void process_iso(UINT idx, ULONG len)
{
    UCHAR               *buf = thread_data->video_data_ptrs[idx];
    uvc_stream_header_t *h   = (uvc_stream_header_t *) buf;
    UINT                 hl  = (len >= 2U) ? h->bHeaderLength : 0U;
    UINT                 pk  = g_ep_mps_raw & ISO_MPS_SIZE_MASK;

    g_perf.transfers++;
    g_perf.raw_bytes   += len;
    g_perf.w_raw_bytes += len;
    if (len > g_perf.max_packet) g_perf.max_packet = len;
    if ((0U != pk) && (len >= pk)) g_perf.full_packets++;

    if ((len >= 2U) && (hl >= 2U) && (hl <= len))
    {
        UCHAR info = h->bmHeaderInfo;
        frame_begin_payload(info);
        if (STOP == g_video_status) return;
        frame_append(buf + hl, len - hl);
        frame_end_payload(info);
    }
}

/* BULK: a payload may span several reads, header only at payload start */
static void process_bulk(ULONG len, ULONG requested)
{
    ULONG off = 0;

    g_perf.transfers++;
    g_perf.raw_bytes   += len;
    g_perf.w_raw_bytes += len;
    if (0U == len)
    {
        g_bk_in_payload = false;
        return;
    }
    if (!g_bk_in_payload)
    {
        UINT hl = g_bulk_buf[0];
        if ((len < 2U) || (hl < 2U) || (hl > len))
        {
            return;                                                  /* resync on next payload */
        }
        g_bk_info       = g_bulk_buf[1];
        g_bk_in_payload = true;
        g_bk_got        = 0;
        frame_begin_payload(g_bk_info);
        if (STOP == g_video_status) return;
        off = hl;
    }
    frame_append(&g_bulk_buf[off], (UINT) (len - off));
    g_bk_got += len;

    if ((len < requested) || ((0U != g_bk_max_payload) && (g_bk_got >= g_bk_max_payload)))
    {
        UCHAR info = g_bk_info;
        g_bk_in_payload = false;
        /* camera without EOF bit: a payload ending with JPEG EOI closes the frame now (no wait for next FID) */
        if (0U == (info & UVC_STREAM_HEADER_HEADERINFO_EOF))
        {
            bool eoi = g_frame_ovf ? ((len - off) >= 2U && jpeg_tail_eoi(&g_bulk_buf[off], (UINT) (len - off), 32U))
                                   : ((g_frame_index >= 4U) &&
                                      jpeg_tail_eoi(&fb_buffer[g_fb_index][0], g_frame_index, 32U));
            if (eoi)
            {
                info |= UVC_STREAM_HEADER_HEADERINFO_EOF;
            }
        }
        frame_end_payload(info);
    }
}

static void perf_window(void)
{
    ULONG now = tx_time_get(), ms = ms_of(now - g_perf.w_start_tick);
    if (ms < 1000U) return;

    ULONG f = (g_perf.w_frames * 10000U) / ms;
    if (g_perf.frames_valid > g_perf.w_frames)                      /* skip windows before the first frame */
    {
        if (f < g_perf.fps_min_x10) g_perf.fps_min_x10 = f;
        if (f > g_perf.fps_max_x10) g_perf.fps_max_x10 = f;
    }
    g_perf.windows++;
#if PERF_WINDOW_PRINT
    ULONG mb_x10 = (ULONG) ((g_perf.w_raw_bytes * 10000U) / ms / (1024U * 1024U));
    if (0U != g_jpeg_w)
    {
        con_print(false, "    %4ux%-4u  %2lu.%lu fps   complete %3lu   invalid %3lu   %lu.%lu MB/s",
                  g_jpeg_w, g_jpeg_h, f / 10U, f % 10U, g_perf.w_frames, g_perf.w_invalid, mb_x10 / 10U, mb_x10 % 10U);
    }
    else
    {
        con_print(false, "    (no complete frame yet)   invalid %3lu   %lu.%lu MB/s",
                  g_perf.w_invalid, mb_x10 / 10U, mb_x10 % 10U);
    }
#endif
    g_perf.w_start_tick = now;
    g_perf.w_frames = g_perf.w_invalid = 0;
    g_perf.w_raw_bytes = 0;
}

/* =====================================================================================================
 *  Thread entry
 * ===================================================================================================*/
static void wait_for_camera_insertion(void)
{
    tx_event_flags_get (&g_device_insert_eventflag, thread_data->device_inserted_eventflag,
                        TX_OR, (ULONG*) &actual_flags, TX_WAIT_FOREVER);
    tx_thread_sleep (WAIT_100_TICKS);
    if (NULL == thread_data->video_host_class)                      /* removed again during the delay */
    {
        return;
    }
    ux_host_class_video_transfer_callback_set (thread_data->video_host_class, uvc_transfer_request_done_callback);
    if (!g_bulk_sem_ok)
    {
        g_bulk_sem_ok = (TX_SUCCESS == tx_semaphore_create(&g_bulk_sem, "uvc bulk", 0));
    }
    bulk_pipe_close();                                              /* nothing left from a previous camera */
    for (UINT i = 0; i < VIDEO_XFER_NUM; i++)
    {
        thread_data->video_data_ptrs[i] = &video_buffer[i][0];
    }
    g_video_status = STOP;
    g_line_len     = 0;
    uvc_scan();
}

void usbcamera_thread_entry(void)
{
    UINT status;

    /* console (RTT or UART SCI9_B, selected by UVC_CONSOLE_TYPE) */
    uvc_console_init();
    uvc_console_banner();

    /* DWT cycle counter for CPU load / DHT time */
#if defined(DCB_DEMCR_TRCENA_Msk)
    DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
#else
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#endif
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    if (UX_SUCCESS != (status = ux_system_initialize(g_ux_uvc_pool_memory, MEMPOOL_SIZE, UX_NULL, UX_ZERO)))
    {
        PRINT_ERR_STR("ux_system_initialize failed");
        ERROR_TRAP(status);
    }
    if (UX_SUCCESS != (status = ux_host_stack_initialize(ux_host_usr_event_notification)))
    {
        PRINT_ERR_STR("ux_host_stack_initialize failed");
        ERROR_TRAP(status);
    }
    fsp_err_t err = R_USB_Open(&g_basic0_ctrl, &g_basic0_cfg);
    if (FSP_SUCCESS != err)
    {
        PRINT_ERR_STR("R_USB_Open failed");
        (void) ux_host_stack_uninitialize();
        ERROR_TRAP(err);
    }
    PRINT_INFO_STR(" >> USB host ready. Waiting for a USB camera ...");

    thread_data->device_inserted_eventflag = EVENTFLAG_USB_DEVICE_INSERTED;
    wait_for_camera_insertion();

    while (true)
    {
        tx_event_flags_get (&g_device_insert_eventflag, thread_data->device_inserted_eventflag, TX_OR,
                            (ULONG*) &actual_flags, TX_NO_WAIT);
        if (!(actual_flags & thread_data->device_inserted_eventflag) || (NULL == thread_data->video_host_class))
        {
            if (g_ready || (STOP != g_video_status))
            {
                camera_removed();
            }
            wait_for_camera_insertion();
            continue;
        }

        ui_command();

        if (STOP == g_video_status)
        {
            tx_thread_sleep(1);
            continue;
        }
        if ((CAPTURE_IMAGE == g_video_status) &&
            (ms_of(tx_time_get() - g_capture_tick) > UVC_CAPTURE_TIMEOUT_MS))
        {
            test_stop((NULL != fb_data.p_pointer) ? "captured last complete frame" : "stopped, no frame");
            continue;
        }

        /* ---------------- BULK path ---------------- */
        if (g_bulk_mode)
        {
            ULONG    req = bulk_next_len();
            ULONG    act = 0;
            uint32_t tb  = DWT->CYCCNT;

            status = bulk_read(req, &act, &req);
            if (UX_TRANSFER_TIMEOUT == status)
            {
                g_bk_timeouts++;
            }
            else if (UX_SUCCESS != status)
            {
                if (NULL != thread_data->video_host_class)
                {
                    test_stop("BULK transfer error");
                }
                continue;
            }
            else
            {
                process_bulk(act, req);
            }
            if (STOP != g_video_status)
            {
                perf_window();
            }
            uint32_t db = (uint32_t) (DWT->CYCCNT - tb);
            g_perf.proc_cycles += (db > g_bk_wait_cycles) ? (db - g_bk_wait_cycles) : 0U;
            g_bk_wait_cycles = 0;
            continue;
        }

        /* ---------------- ISO path ---------------- */
        if (TX_SUCCESS != tx_semaphore_get(&g_payload_semaphore, WAIT_10_TICKS))
        {
            continue;
        }
        UINT  idx;
        ULONG len;
        if (!xfer_pop(&idx, &len))
        {
            continue;
        }
        uint32_t t0 = DWT->CYCCNT;
        process_iso(idx, len);
        if ((STOP != g_video_status) && (NULL != thread_data->video_host_class))
        {
            status = ux_host_class_video_transfer_buffer_add(thread_data->video_host_class,
                                                             thread_data->video_data_ptrs[idx]);
            if (UX_SUCCESS != status)
            {
                test_stop("USB transfer error");
                continue;
            }
            perf_window();
        }
        g_perf.proc_cycles += (uint32_t) (DWT->CYCCNT - t0);
    }
}

/* =====================================================================================================
 *  Console output: one message = one line (RTT or UART)
 * ===================================================================================================*/
static void con_print(bool pace, const char *fmt, ...)
{
    char    msg[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    uvc_console_line(msg);

    if (pace && (0U != UI_PRINT_PACE_TICKS))
    {
        tx_thread_sleep(UI_PRINT_PACE_TICKS);
    }
}
