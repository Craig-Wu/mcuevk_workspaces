/***********************************************************************************************************************
 * File Name    : agt_ep.c
 * Description  : Contains data structures and functions used in agt_ep.c.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2020 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/

#include "common_utils.h"
#include "agt_ep.h"

/*******************************************************************************************************************//**
 * @addtogroup agt_ep
 * @{
 **********************************************************************************************************************/

extern bsp_leds_t g_bsp_leds;
extern volatile uint8_t g_periodic_timer_flag ;
extern volatile uint32_t g_error_flag ;
/*******************************************************************************************************************//**
 * @brief       This functions initializes AGT module.
 * @param[IN]   None
 * @retval      FSP_SUCCESS                  Upon successful open of AGT module
 * @retval      Any Other Error code apart from FSP_SUCCESS  Unsuccessful open
 **********************************************************************************************************************/
fsp_err_t agt_init(void)
{
    fsp_err_t err = FSP_SUCCESS;     // Error status

    /* Open Timer0 in One Shot Mode */
    err = R_AGT_Open(&g_timer_one_shot_ctrl, &g_timer_one_shot_cfg);
    /* Handle error */
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT ("\r\nAGT0 timer open failed.\r\nRestart the Application\r\n");
        return err;
    }

    /* Open Timer1 in Periodic mode */
    err = R_AGT_Open(&g_timer_periodic_ctrl, &g_timer_periodic_cfg);
    /* Handle error */
    if (FSP_SUCCESS != err)
    {
        /* Close Timer0 in One Shot Mode */
        if ( (FSP_SUCCESS != R_AGT_Close(&g_timer_one_shot_ctrl)))
        {
            APP_ERR_PRINT ("\r\nOne shot timer close failed.\r\nRestart the Application\r\n");
        }
        APP_ERR_PRINT ("\r\nAGT1 timer open failed.\r\nRestart the Application\r\n");
    }
    return err;
}

/*******************************************************************************************************************//**
 * @brief       This functions starts AGT0 in one-shot mode.
 * @param[IN]   None
 * @retval      FSP_SUCCESS                  Timer started successfully
 * @retval      Any Other Error code apart from FSP_SUCCESS  
 **********************************************************************************************************************/
fsp_err_t agt_start_oneshot_timer(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Start Timer0 in one-shot mode */
    err = R_AGT_Start(&g_timer_one_shot_ctrl);
    /* Handle error */
    if (FSP_SUCCESS != err)
    {
        APP_ERR_PRINT("\r\n AGT0 timer start failed.\r\n");
    }
    return err;
}

/*******************************************************************************************************************//**
 * @brief This function is callback for one-shot mode timer and starts AGT1 timer in Periodic mode.
 *
 * @param[in] (timer_callback_args_t *)
 **********************************************************************************************************************/
void one_shot_timer_callback(timer_callback_args_t *p_args)
{
    if(TIMER_EVENT_CYCLE_END == p_args->event)
    {
        fsp_err_t err = FSP_SUCCESS;
        timer_status_t periodic_timer_status;

        /* Retrieve the status of timer running in periodic mode */
        err = R_AGT_StatusGet(&g_timer_periodic_ctrl, &periodic_timer_status);
        if (FSP_SUCCESS != err)
        {
            g_error_flag = SET_FLAG;
        }
        else
        {
            if (TIMER_STATE_COUNTING != periodic_timer_status.state)
            {
                /* Start the timer in periodic mode only if the timer is in stooped state */
                err = R_AGT_Start(&g_timer_periodic_ctrl);
                /* Handle error */
                if (FSP_SUCCESS != err)
                {
                    g_error_flag = SET_FLAG;
                    APP_ERR_PRINT("\r\nAGT1 Cannot be started in Periodic mode");
                }
                else
                {
                    g_periodic_timer_flag = SET_FLAG;   //Set the flag since timer1 is started in periodic mode
                }
            }
            else
            {
                g_periodic_timer_flag = ALREADY_RUNNING;
            }
        }
    }
}

/*******************************************************************************************************************//**
 * @brief This function is callback for periodic timer and blinks LED on every 1 Second.
 *
 * @param[in] (timer_callback_args_t *)
 **********************************************************************************************************************/
void periodic_timer_callback(timer_callback_args_t *p_args)
{
#if defined (BOARD_RA4W1_EK) || defined (BOARD_RA6T1_RSSK)
    static volatile bsp_io_level_t led_level = BSP_IO_LEVEL_LOW;
#else
    static volatile bsp_io_level_t led_level = BSP_IO_LEVEL_HIGH;
#endif
    /* If this board has no LEDs then trap here */
    if (LED_COUNT_ZERO == g_bsp_leds.led_count)
    {
        g_error_flag = SET_FLAG;
        APP_ERR_PRINT ("\r\nError: No Led's Found on the board");
        return;
    }

    /* Change the state of the LED write value */
    led_level ^= BSP_IO_LEVEL_HIGH;

    if(TIMER_EVENT_CYCLE_END == p_args->event)
    {
        /* Change LED state */
        fsp_err_t err = R_IOPORT_PinWrite(&g_ioport_ctrl, (bsp_io_port_pin_t) g_bsp_leds.p_leds[RESET_FLAG], led_level);
        /* Handle error */
        if (FSP_SUCCESS != err)
        {
            g_error_flag = SET_FLAG;
            APP_ERR_PRINT("\r\nLED_PIN state cannot be toggled");
            return;
        }
    }
}

/*******************************************************************************************************************//**
 * @brief This function closes opened AGT module before the project ends up in an Error Trap.
 * @param[IN]   None
 * @retval      None
 **********************************************************************************************************************/
void agt_deinit(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Close AGT0 module */
    err = R_AGT_Close(&g_timer_one_shot_ctrl);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* AGT0 Close failure message */
        APP_ERR_PRINT("** R_AGT_Close API for channel 0 failed **  \r\n");
    }

    /* Close AGT1 module */
    err = R_AGT_Close(&g_timer_periodic_ctrl);
    /* handle error */
    if (FSP_SUCCESS != err)
    {
        /* AGT1 Close failure message */
        APP_ERR_PRINT("** R_AGT_Close API for channel 1 failed **  \r\n");
    }

}

/*******************************************************************************************************************//**
 * @} (end addtogroup agt_ep)
 **********************************************************************************************************************/





