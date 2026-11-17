/***********************************************************************************************************************
 * File Name    : rtt_thread_entry.c
 * Description  : RTT console thread (input queue / output queue)
 *                Output format: one line per message, no extra blank lines.
 *                Waits for free space in the RTT up-buffer so that long lists are not cut off.
 ***********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2023 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/

#include "rtt_thread.h"
#include "common_utils.h"
#include "uvccamera_cfg.h"
#include "usbx_host_uvc_common.h"

#define RTT_SPACE_WAIT_TICKS    (50U)   /* max wait for RTT up-buffer space (viewer not connected -> continue) */

/* Local functions */
static void process_rtt_op_msg(VOID);
static UINT memory_allocate_rtt(TX_BYTE_POOL *pool, rtt_msg_t **p_buf, uint32_t size);
static void rtt_wait_space(uint32_t len);

extern TX_THREAD rtt_thread;

/* RTT_Thread entry function */
void rtt_thread_entry(void)
{
    UINT       err = TX_SUCCESS;
    CHAR       rtt_buffer[IP_MSG_MAX_SIZE] = {'\0'};
    rtt_msg_t *p_data = NULL;

    err = rtt_framework_init();
    if (TX_SUCCESS != err)
    {
        APP_PRINT("\r\nERROR: rtt_framework_init failed\r\n");
        APP_ERR_TRAP(err);
    }

    while (1)
    {
        /* User input -> camera thread */
        if (APP_CHECK_DATA)
        {
            memset(rtt_buffer, '\0', sizeof(rtt_buffer));
            (void) APP_READ(rtt_buffer);
            rtt_buffer[sizeof(rtt_buffer) - 1] = '\0';

            uint32_t len = (uint32_t) strlen(rtt_buffer) + 1U;
            err = memory_allocate_rtt(&g_byte_pool, &p_data, sizeof(rtt_msg_t) + len);
            if (TX_SUCCESS == err)
            {
                p_data->msg_id        = RTT_INPUT_MESSAGE_TYPE_STR_QUEUED;
                p_data->msg_data_size = len;
                memcpy(p_data->p_msg, rtt_buffer, len);
                tx_queue_send(&g_rtt_ip_data_queue, (rtt_msg_t *) &p_data, TX_WAIT_FOREVER);
            }
        }

        /* Camera thread messages -> RTT Viewer */
        process_rtt_op_msg();

        tx_thread_sleep(1);
    }
}

/* Wait until the RTT up-buffer can take 'len' bytes (bounded wait, never blocks forever) */
static void rtt_wait_space(uint32_t len)
{
    for (UINT i = 0; i < RTT_SPACE_WAIT_TICKS; i++)
    {
        if (SEGGER_RTT_GetAvailWriteSpace(SEGGER_INDEX) >= (len + 4U))
        {
            return;
        }
        tx_thread_sleep(1);
    }
}

/* Print queued messages: one line each, no extra blank line */
static void process_rtt_op_msg(VOID)
{
    UINT       err;
    rtt_msg_t *rtt_op_data = NULL;

    do
    {
        err = tx_queue_receive(&g_rtt_op_data_queue, (VOID *) &rtt_op_data, 1);
        if (TX_SUCCESS == err)
        {
            switch (rtt_op_data->msg_id)
            {
                case RTT_OUTPUT_MESSAGE_BANNER:
                {
                    fsp_pack_version_t version = {RESET_VALUE};
                    R_FSP_VersionGet(&version);
                    rtt_wait_space(sizeof(EP_BANNER));
                    APP_PRINT(EP_BANNER, version.version_id_b.major, version.version_id_b.minor,
                              version.version_id_b.patch);
                    break;
                }
                case RTT_OUTPUT_MESSAGE_APP_INFO_STR:
                    rtt_wait_space(rtt_op_data->msg_data_size + 2U);
                    APP_PRINT("%s\r\n", rtt_op_data->p_msg);
                    break;

                case RTT_OUTPUT_MESSAGE_APP_ERR_STR:
                    rtt_wait_space(rtt_op_data->msg_data_size + 9U);
                    APP_PRINT("ERROR: %s\r\n", rtt_op_data->p_msg);
                    break;

                case RTT_OUTPUT_MESSAGE_APP_ERR_TRAP:
                {
                    UINT err_val = *(UINT *) rtt_op_data->p_msg;
                    APP_ERR_TRAP(err_val);
                    break;
                }
                case RTT_OUTPUT_MESSAGE_APP_NEXT_LINE:
                    APP_PRINT("\r\n");
                    break;

                case RTT_OUTPUT_MESSAGE_APP_PRINT_DATA:
                    APP_PRINT("%c", rtt_op_data->p_msg[0]);
                    break;

                case RTT_OUTPUT_MESSAGE_APP_PRINT_MENU:
                    rtt_wait_space(sizeof(USBX_HUVC_MENU));
                    APP_PRINT(USBX_HUVC_MENU);
                    break;

                default:
                    APP_PRINT("RTT: unknown message ID\r\n");
                    break;
            }
            (void) tx_byte_release((void *) rtt_op_data);
        }
    } while (TX_QUEUE_EMPTY != err);
}

/* Called by user threads to print a message */
UINT app_rtt_print_data(event_id_t id, uint32_t size, void * const p_data)
{
    rtt_msg_t *p_display_data = NULL;
    UINT       err;

    err = memory_allocate_rtt(&g_byte_pool, &p_display_data, sizeof(rtt_msg_t) + size);
    if (TX_SUCCESS != err)
    {
        APP_PRINT("\r\nError: memory_allocate_rtt failed\r\n");
        rtt_thread_service_cleanup();
    }

    p_display_data->msg_id        = id;
    p_display_data->msg_time      = tx_time_get();
    p_display_data->msg_data_size = size;
    memcpy(p_display_data->p_msg, p_data, size);

    return tx_queue_send(&g_rtt_op_data_queue, (rtt_msg_t *) &p_display_data, TX_WAIT_FOREVER);
}

static UINT memory_allocate_rtt(TX_BYTE_POOL *pool, rtt_msg_t **p_buf, uint32_t size)
{
    rtt_msg_t *buf = NULL;
    UINT       err = tx_byte_allocate(pool, (VOID **) &buf, size, TX_WAIT_FOREVER);

    if (TX_SUCCESS == err)
    {
        *p_buf = buf;
    }
    return err;
}

/* Wait until the RTT thread has started */
VOID rtt_thread_init_check(VOID)
{
    UINT          err;
    thread_info_t rtt_thread_info = { .thread_ptr = &rtt_thread };

    do
    {
        err = tx_thread_info_get(rtt_thread_info.thread_ptr, TX_NULL, &rtt_thread_info.state,
                                 &rtt_thread_info.run_count, TX_NULL, TX_NULL, TX_NULL, TX_NULL, TX_NULL);
        if (rtt_thread_info.run_count >= 1)
        {
            break;
        }
        tx_thread_sleep(1);
    } while (TX_SUCCESS == err);
}
