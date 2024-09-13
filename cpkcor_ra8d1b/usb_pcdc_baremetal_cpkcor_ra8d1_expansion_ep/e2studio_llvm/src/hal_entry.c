/***********************************************************************************************************************
 * File Name    : hal_entry.c
 * Description  : Entry function.
 **********************************************************************************************************************/
/*
* Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <string.h>
#include "hal_entry.h"
#include "common_init.h"

/* Function declaration */
void R_BSP_WarmStart(bsp_warm_start_event_t event);

/* Global variables */
extern uint8_t g_apl_device[];
extern uint8_t g_apl_configuration[];
extern uint8_t g_apl_hs_configuration[];
extern uint8_t g_apl_qualifier_descriptor[];
extern uint8_t *g_apl_string_table[];
extern int g_curr_led_freq;

const usb_descriptor_t usb_descriptor =
{
 g_apl_device,                   /* Pointer to the device descriptor */
 g_apl_configuration,            /* Pointer to the configuration descriptor for Full-speed */
 g_apl_hs_configuration,         /* Pointer to the configuration descriptor for Hi-speed */
 g_apl_qualifier_descriptor,     /* Pointer to the qualifier descriptor */
 g_apl_string_table,             /* Pointer to the string descriptor table */
 NUM_STRING_DESCRIPTOR
};

usb_status_t            usb_event;

/* Banner Info */
char p_welcome[200] = {
                       "\r\n Welcome to Quick Start BLINKY example project for "
                       KIT_NAME_MACRO
                       "!"
                       "\r\n Press 1 for Kit Information or 2 for Next Steps.\r\n"
};

/* Next steps */
char nextsteps[USB_EP_PACKET_SIZE] = {
                                      "\r\n 2. NEXT STEPS \r\n"
                                      "\r\nVisit the following URLs to learn about the kit "
                                      "and the RA family of MCUs, download tools "
                                      "and documentation, and get support:\r\n"
                                      "\r\n a) "
                                      KIT_NAME_MACRO
                                      " resources: \t"
                                      KIT_LANDING_URL
                                      "\r\n b) RA product information:  \t"
                                      PRODUCT_INFO_URL
                                      "\r\n c) RA product support forum: \t"
                                      PRODUCT_SUPPORT_URL
                                      "\r\n d) Renesas support: \t\t"
                                      RENESAS_SUPPORT_URL
                                      "\r\n\r\n Press 1 for Kit Information or 2 for Next Steps.\r\n"
};

char kitinfo[USB_EP_PACKET_SIZE] = {'\0'};

const char *p_mcu_temp = "\r\n d) MCU Die temperature (F/C):  ";
const char *p_led_freq = "\r\n c) Current blinking frequency (Hz): ";
const char *p_kit_menu_ret = "\r\n Press 1 for Kit Information or 2 for Next Steps.\r\n";

static bool  b_usb_attach = false;

/* Private functions */
static fsp_err_t check_for_write_complete(void);
static fsp_err_t print_to_console(char *p_data);
static void process_kit_info(void);

fsp_err_t g_err = FSP_SUCCESS;


/*******************************************************************************************************************//**
 * The RA Configuration tool generates main() and uses it to generate threads if an RTOS is used.  This function is
 * called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    fsp_err_t err                           = FSP_SUCCESS;
    usb_event_info_t    event_info          = {0};
    uint8_t g_buf[READ_BUF_SIZE]            = {0};
    static usb_pcdc_linecoding_t g_line_coding;

    /* Open USB instance */
    err = R_USB_Open (&g_basic_ctrl, &g_basic_cfg);
    /* Handle error */
    if (FSP_SUCCESS != err)
    {
        APP_ERR_TRAP(err);
    }

    while (true)
    {
        /* Obtain USB related events */
        err = R_USB_EventGet (&event_info, &usb_event);

        /* Handle error */
        if (FSP_SUCCESS != err)
        {
            APP_ERR_TRAP(err);
        }

        /* USB event received by R_USB_EventGet */
        switch (usb_event)
        {
            case USB_STATUS_CONFIGURED:
            {
                err = R_USB_Read (&g_basic_ctrl, g_buf, READ_BUF_SIZE, USB_CLASS_PCDC);
                /* Handle error */
                if (FSP_SUCCESS != err)
                {
                    APP_ERR_TRAP(err);
                }
                break;
            }

            case USB_STATUS_READ_COMPLETE:
            {
                if(b_usb_attach)
                {
                    err = R_USB_Read (&g_basic_ctrl, g_buf, READ_BUF_SIZE, USB_CLASS_PCDC);
                }
                /* Handle error */
                if (FSP_SUCCESS != err)
                {
                    APP_ERR_TRAP(err);
                }

                /* Switch case evaluation of user input */
                switch (g_buf[0])
                {
                    case KIT_INFO:
                    {
                        process_kit_info();
                        break;
                    }
                    case NEXT_STEPS:
                    {
                        err = print_to_console(nextsteps);
                        if (FSP_SUCCESS != err)
                        {
                            APP_ERR_TRAP(err);
                        }
                        break;
                    }

                    case CARRIAGE_RETURN:
                    {
                        /* Print banner info to console */
                        err = print_to_console(p_welcome);
                        if (FSP_SUCCESS != err)
                        {
                            APP_ERR_TRAP(err);
                        }
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
                break;
            }

            case USB_STATUS_REQUEST : /* Receive Class Request */
            {
                /* Check for the specific CDC class request IDs */
                if (USB_PCDC_SET_LINE_CODING == (event_info.setup.request_type & USB_BREQUEST))
                {
                    err =  R_USB_PeriControlDataGet (&g_basic_ctrl, (uint8_t *) &g_line_coding, LINE_CODING_LENGTH );
                    /* Handle error */
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_TRAP(err);
                    }
                }
                else if (USB_PCDC_GET_LINE_CODING == (event_info.setup.request_type & USB_BREQUEST))
                {
                    err =  R_USB_PeriControlDataSet (&g_basic_ctrl, (uint8_t *) &g_line_coding, LINE_CODING_LENGTH );
                    /* Handle error */
                    if (FSP_SUCCESS != err)
                    {
                        APP_ERR_TRAP(err);
                    }
                }
                else if (USB_PCDC_SET_CONTROL_LINE_STATE == (event_info.setup.request_type & USB_BREQUEST))
                {
                    err = R_USB_PeriControlStatusSet (&g_basic_ctrl, USB_SETUP_STATUS_ACK);
                    /* Handle error */
                    if (FSP_SUCCESS != err)
                        //if (FSP_SUCCESS != g_err)
                    {
                        APP_ERR_TRAP(err);
                    }
                }
                else
                {
                    /* none */
                }

                break;
            }

            case USB_STATUS_DETACH:
            case USB_STATUS_SUSPEND:
            {
                b_usb_attach = false;
                memset (g_buf, 0, sizeof(g_buf));
                break;
            }
            case USB_STATUS_RESUME:
            {
                b_usb_attach = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */
        /* Configure pins. */
        R_IOPORT_Open (&g_ioport_ctrl, &g_bsp_pin_cfg);
    }
}

/*****************************************************************************************************************
 *  @brief      Prints the message to console
 *  @param[in]  p_msg contains address of buffer to be printed
 *  @retval     FSP_SUCCESS     Upon success
 *  @retval     any other error code apart from FSP_SUCCESS, Write is unsuccessful
 ****************************************************************************************************************/
static fsp_err_t print_to_console(char *p_data)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t len = ((uint32_t)strlen(p_data));

    err = R_USB_Write (&g_basic_ctrl, (uint8_t*)p_data, len, USB_CLASS_PCDC);
    /* Handle error */
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = check_for_write_complete();
    if (FSP_SUCCESS != err)
    {
        /* Did not get the event hence returning error */
        return FSP_ERR_USB_FAILED;
    }
    return err;
}

/*****************************************************************************************************************
 *  @brief      Check for write completion
 *  @param[in]  None
 *  @retval     FSP_SUCCESS     Upon success
 *  @retval     any other error code apart from FSP_SUCCESS
 ****************************************************************************************************************/
static fsp_err_t check_for_write_complete(void)
{
    usb_status_t usb_write_event = USB_STATUS_NONE;
    int32_t timeout_count = UINT16_MAX;
    fsp_err_t err = FSP_SUCCESS;
    usb_event_info_t    event_info = {0};

    do
    {
        err = R_USB_EventGet (&event_info, &usb_write_event);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        --timeout_count;

        if (0 > timeout_count)
        {
            timeout_count = 0;
            err = (fsp_err_t)USB_STATUS_NONE;
            break;
        }
    }while(USB_STATUS_WRITE_COMPLETE != usb_write_event);

    return err;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup hal_entry)
 **********************************************************************************************************************/

static void process_kit_info(void)
{
    uint16_t buffer_index_count = 0x0000;
    uint16_t adc_data                    = 0;
    float mcu_temp_f                     = 0;
    float mcu_temp_c                     = 0;
    fsp_err_t err = FSP_SUCCESS;

    /* clear kit info buffer before updating data */
    memset(kitinfo, '\0', 511);

    /* update  predefined text in the buffer */
    memcpy(kitinfo, (char *)KIT_INFO_PRIMARY_TEXT, strlen((char *)KIT_INFO_PRIMARY_TEXT) );

    /* Print kit menu to console */
    err = print_to_console(kitinfo);
    /* Handle error*/
    if (FSP_SUCCESS != err)
    {
        APP_ERR_TRAP(err);
    }
}
