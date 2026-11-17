/***********************************************************************************************************************
 * File Name    : uvc_console.c
 * Description  : CPKCOR-RA8P1 USB camera (UVC) host demo - debug console abstraction
 *
 *   Console is selected by UVC_CONSOLE_TYPE (uvccamera_cfg.h or project properties):
 *   UVC_CONSOLE_RTT    : output via app_rtt_print_data(), input via g_rtt_ip_data_queue (RTT thread)
 *   UVC_CONSOLE_SCI9_B : output via r_sci_b_uart write (blocking until TX complete),
 *                        input via UART_EVENT_RX_CHAR -> ring buffer (no read() is started)
 *
 *   FSP (UART mode): r_sci_b_uart, name g_uart9, channel 9, callback uvc_console_uart_callback
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "usbcamera_thread.h"
#include "common_utils.h"
#include "usbx_host_uvc_common.h"
#include "uvc_console.h"

#if (UVC_CONSOLE_TYPE == UVC_CONSOLE_RTT)
/* =====================================================================================================
 *  RTT backend
 * ===================================================================================================*/
void uvc_console_init(void)
{
    rtt_thread_init_check();
}

void uvc_console_banner(void)
{
    app_rtt_print_data(RTT_OUTPUT_MESSAGE_BANNER, RESET_VALUE, NULL);
}

void uvc_console_line(const char *msg)
{
    app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_INFO_STR, (uint32_t) (strlen(msg) + 1U), (void *) msg);
}

void uvc_console_error_line(const char *msg)
{
    app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_ERR_STR, (uint32_t) (strlen(msg) + 1U), (void *) msg);
}

void uvc_console_error_trap(uint32_t err)
{
    static UINT e;
    e = (UINT) err;
    app_rtt_print_data(RTT_OUTPUT_MESSAGE_APP_ERR_TRAP, sizeof(UINT *), (void *) &e);
}

uint32_t uvc_console_read(char *buf, uint32_t size)
{
    rtt_msg_t *r = NULL;
    uint32_t   n = 0;

    if ((TX_SUCCESS == tx_queue_receive(&g_rtt_ip_data_queue, (VOID *) &r, TX_NO_WAIT)) && (NULL != r))
    {
        if (RTT_INPUT_MESSAGE_TYPE_STR_QUEUED == r->msg_id)
        {
            const char *p = (const char *) r->p_msg;
            while ((n < size) && ('\0' != p[n]))
            {
                buf[n] = p[n];
                n++;
            }
        }
        (void) tx_byte_release(r);
    }
    return n;
}

void uvc_console_echo(char ch)
{
    (void) ch;                                                   /* RTT Viewer echoes locally */
}

/* the g_uart9 stack may stay in the FSP configuration while RTT is selected */
 #if defined(R_UART_API_H)
void uvc_console_uart_callback(uart_callback_args_t *p_args)
{
    (void) p_args;
}
 #endif

#else
/* =====================================================================================================
 *  UART (SCI9_B) backend
 * ===================================================================================================*/
#define CON_UART        (UVC_CONSOLE_UART_INSTANCE)

static TX_SEMAPHORE      g_con_tx_sem;
static TX_MUTEX          g_con_mutex;
static bool              g_con_ready = false;
static char              g_con_tx_buf[UVC_CONSOLE_TX_BUF_SIZE];
static char              g_con_banner[1600];

static volatile uint8_t  g_con_rx_ring[UVC_CONSOLE_RX_RING_SIZE];
static volatile uint32_t g_con_rx_w   = 0;
static volatile uint32_t g_con_rx_r   = 0;
static volatile uint32_t g_con_rx_ovf = 0;

static ULONG con_ticks(ULONG ms)
{
    ULONG t = (ULONG) (((uint64_t) ms * TX_TIMER_TICKS_PER_SECOND) / 1000U);
    return (0U == t) ? 1U : t;
}

/* FSP callback (set "Callback" = uvc_console_uart_callback in the g_uart9 properties) */
void uvc_console_uart_callback(uart_callback_args_t *p_args)
{
    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:                                 /* no read() pending -> one event per byte */
        {
            uint32_t w    = g_con_rx_w;
            uint32_t next = (w + 1U) % UVC_CONSOLE_RX_RING_SIZE;
            if (next != g_con_rx_r)
            {
                g_con_rx_ring[w] = (uint8_t) p_args->data;
                g_con_rx_w       = next;
            }
            else
            {
                g_con_rx_ovf++;
            }
            break;
        }
        case UART_EVENT_TX_COMPLETE:
            (void) tx_semaphore_put(&g_con_tx_sem);
            break;
        default:                                                 /* framing / parity / overrun: ignored */
            break;
    }
}

/* blocking write (waits for TX complete), called with g_con_mutex held */
static void con_uart_write(const char *s, uint32_t len)
{
    while (g_con_ready && (len > 0U))
    {
        uint32_t n = (len > sizeof(g_con_tx_buf)) ? (uint32_t) sizeof(g_con_tx_buf) : len;

        memcpy(g_con_tx_buf, s, n);
        while (TX_SUCCESS == tx_semaphore_get(&g_con_tx_sem, TX_NO_WAIT)) {}

        if (FSP_SUCCESS != CON_UART.p_api->write(CON_UART.p_ctrl, (uint8_t const *) g_con_tx_buf, n))
        {
            return;
        }
        (void) tx_semaphore_get(&g_con_tx_sem, con_ticks(UVC_CONSOLE_TX_TIMEOUT_MS));

        s   += n;
        len -= n;
    }
}

static void con_write_locked(const char *prefix, const char *msg, bool newline)
{
    if (!g_con_ready || (NULL == msg))
    {
        return;
    }
    (void) tx_mutex_get(&g_con_mutex, TX_WAIT_FOREVER);
    if (NULL != prefix)
    {
        con_uart_write(prefix, (uint32_t) strlen(prefix));
    }
    con_uart_write(msg, (uint32_t) strlen(msg));
    if (newline)
    {
        con_uart_write("\r\n", 2U);
    }
    (void) tx_mutex_put(&g_con_mutex);
}

void uvc_console_init(void)
{
    if (g_con_ready)
    {
        return;
    }
    if (TX_SUCCESS != tx_semaphore_create(&g_con_tx_sem, "con tx", 0))
    {
        return;
    }
    if (TX_SUCCESS != tx_mutex_create(&g_con_mutex, "con", TX_INHERIT))
    {
        return;
    }
    g_con_rx_w = g_con_rx_r = 0;
    g_con_ready = (FSP_SUCCESS == CON_UART.p_api->open(CON_UART.p_ctrl, CON_UART.p_cfg));
}

void uvc_console_banner(void)
{
    fsp_pack_version_t v;

    memset(&v, 0, sizeof(v));
    (void) R_FSP_VersionGet(&v);
    snprintf(g_con_banner, sizeof(g_con_banner), EP_BANNER,
             v.version_id_b.major, v.version_id_b.minor, v.version_id_b.patch);
    con_write_locked(NULL, g_con_banner, false);
}

void uvc_console_line(const char *msg)
{
    con_write_locked(NULL, msg, true);
}

void uvc_console_error_line(const char *msg)
{
    con_write_locked("ERROR: ", msg, true);
}

void uvc_console_error_trap(uint32_t err)
{
    char m[48];

    snprintf(m, sizeof(m), "ERROR TRAP: code 0x%lX", (unsigned long) err);
    con_write_locked(NULL, m, true);
    while (true)
    {
        tx_thread_sleep(100);                                    /* stop here, debugger can be attached */
    }
}

uint32_t uvc_console_read(char *buf, uint32_t size)
{
    uint32_t n = 0;

    while ((n < size) && (g_con_rx_r != g_con_rx_w))
    {
        buf[n++]   = (char) g_con_rx_ring[g_con_rx_r];
        g_con_rx_r = (g_con_rx_r + 1U) % UVC_CONSOLE_RX_RING_SIZE;
    }
    return n;
}

void uvc_console_echo(char ch)
{
#if UVC_CONSOLE_UART_ECHO
    if (!g_con_ready)
    {
        return;
    }
    (void) tx_mutex_get(&g_con_mutex, TX_WAIT_FOREVER);
    if (('\r' == ch) || ('\n' == ch))
    {
        con_uart_write("\r\n", 2U);
    }
    else if (('\b' == ch) || (0x7F == ch))
    {
        con_uart_write("\b \b", 3U);
    }
    else
    {
        con_uart_write(&ch, 1U);
    }
    (void) tx_mutex_put(&g_con_mutex);
#else
    (void) ch;
#endif
}

#endif /* UVC_CONSOLE_TYPE */
