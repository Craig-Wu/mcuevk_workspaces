/***********************************************************************************************************************
 * File Name    : mipi_dsi_ep.c
 * Description  : Contains data structures and functions setup LCD used in hal_entry.c.
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
 * Copyright (C) 2023 Renesas Electronics Corporation. All rights reserved.
 ***********************************************************************************************************************/
#include "gt911.h"
#include "mipi_dsi_ep.h"
#include "r_mipi_dsi.h"
#include "hal_data.h"
#include "common_utils.h"
#include "arm_mve.h"
#include "pic_show.h"
#include "graphics.h"


/*******************************************************************************************************************//**
 * @addtogroup mipi_dsi_ep
 * @{
 **********************************************************************************************************************/

uint32_t count;

#define ST7796U 1

/* User defined functions */
static void display_draw (uint32_t * framebuffer);
static uint8_t mipi_dsi_set_display_time (void);
static uint8_t process_input_data(void);
void handle_error (fsp_err_t err,  const char * err_str);
void touch_screen_reset(void);
static fsp_err_t wait_for_mipi_dsi_event (mipi_dsi_phy_status_t event);
static void mipi_dsi_ulps_enter(void);
static void mipi_dsi_ulps_exit(void);
void bsp_sdram_init (void);

/* Variables to store resolution information */
//uint16_t g_hz_size, g_vr_size;
///* Variables used for buffer usage */
//uint32_t g_buffer_size, g_hstride;
//uint32_t * gp_single_buffer = NULL;
//uint32_t * gp_double_buffer = NULL;
//uint32_t * gp_frame_buffer  = NULL;
uint8_t read_data              = RESET_VALUE;
uint16_t period_sec           = RESET_VALUE;
volatile mipi_dsi_phy_status_t g_phy_status;
timer_info_t timer_info = { .clock_frequency = RESET_VALUE, .count_direction = RESET_VALUE, .period_counts = RESET_VALUE };
volatile bool g_vsync_flag, g_message_sent, g_ulps_flag, g_irq_state, g_timer_overflow  = RESET_FLAG;
coord_t touch_coordinates[5];


void DWT_init();
uint32_t DWT_get_count();
void DWT_clean_count();
uint32_t DWT_count_to_us(uint32_t delta_count);

#define DWT_DEM *(uint32_t*)0xE000EDFC

void DWT_init()
{
    DWT->CTRL = 0;
    DWT_DEM |= 1<<24;
    DWT->CYCCNT = 0;
    DWT->CTRL |= 1<<0;
}


uint32_t DWT_get_count()
{
    return DWT->CYCCNT;
}

void DWT_clean_count()
{
    DWT->CYCCNT = 0;
}

uint32_t DWT_count_to_us(uint32_t delta_count)
{
    return delta_count/480;
}


uint32_t DWT_pre_count=0, DWT_post_count=0, time_sdram_access=0;
uint32_t DWT_delta=0;

/* This table of commands was adapted from sample code provided by FocusLCD
 * Page Link: https://focuslcds.com/product/4-5-tft-display-capacitive-tp-e45ra-mw276-c/
 * File Link: https://focuslcds.com/content/E45RA-MW276-C_init_code.txt
 */

const lcd_table_setting_t g_lcd_init_focuslcd[] =
{
#if 0
 {6,  {0xFF, 0xFF, 0x98, 0x06, 0x04, 0x01}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE,        MIPI_DSI_CMD_FLAG_LOW_POWER},        // Change to Page 1 CMD
 {2,  {0x08, 0x10},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Output SDA
 {2,  {0x21, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // DE = 1 Active

 {2,  {0x30, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Resolution setting 480 X 800
 {2,  {0x31, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Inversion setting

 {2,  {0x40, 0x14},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // BT 15
 {2,  {0x41, 0x33},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // avdd +5.2v,avee-5.2v
 {2,  {0x42, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // VGL=DDVDL+VCL-VCIP,VGH=2DDVDH-DDVDL
 {2,  {0x43, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Set VGH clamp level
 {2,  {0x44, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Set VGL clamp level
 {2,  {0x50, 0x70},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Set VREG1
 {2,  {0x51, 0x70},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Set VREG2
 {2,  {0x52, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Flicker MSB
 {2,  {0x53, 0x48},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Flicker LSB
 {2,  {0x60, 0x07},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Timing Adjust
 {2,  {0x61, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Timing Adjust
 {2,  {0x62, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Timing Adjust
 {2,  {0x63, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Timing Adjust

 {2,  {0xa0, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 1
 {2,  {0xa1, 0x03},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 2
 {2,  {0xa2, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 3
 {2,  {0xa3, 0x0d},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 4
 {2,  {0xa4, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 5
 {2,  {0xa5, 0x16},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 6
 {2,  {0xa6, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 7
 {2,  {0xa7, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 8
 {2,  {0xa8, 0x03},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 9
 {2,  {0xa9, 0x07},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 10
 {2,  {0xaa, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 11
 {2,  {0xab, 0x05},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 12
 {2,  {0xac, 0x0d},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 13
 {2,  {0xad, 0x2c},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 14
 {2,  {0xae, 0x26},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 15
 {2,  {0xaf, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Positive Gamma Control 16

 {2,  {0xc0, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 1
 {2,  {0xc1, 0x04},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 2
 {2,  {0xc2, 0x0b},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 3
 {2,  {0xc3, 0x0f},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 4
 {2,  {0xc4, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 5
 {2,  {0xc5, 0x18},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 6
 {2,  {0xc6, 0x07},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 7
 {2,  {0xc7, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 8
 {2,  {0xc8, 0x05},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 9
 {2,  {0xc9, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 10
 {2,  {0xca, 0x07},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 11
 {2,  {0xcb, 0x05},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 12
 {2,  {0xcc, 0x0c},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 13
 {2,  {0xcd, 0x2d},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 14
 {2,  {0xce, 0x28},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 15
 {2,  {0xcf, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // Negative Gamma Correction 16

 {6,  {0xFF, 0xFF, 0x98, 0x06, 0x04, 0x06}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE,        MIPI_DSI_CMD_FLAG_LOW_POWER},        // Change to Page 6 CMD for GIP timing
 {2,  {0x00, 0x21},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x01, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x02, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x03, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x04, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x05, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x06, 0x80},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x07, 0x05},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x08, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x09, 0x80},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0c, 0x0a},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0d, 0x0a},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0e, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x0f, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x10, 0xe0},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x11, 0xe4},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x12, 0x04},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x13, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x14, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x15, 0xc0},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x16, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x17, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x18, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x19, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x1a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x1b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x1c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1
 {2,  {0x1d, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 1

 {2,  {0x20, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x21, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x22, 0x45},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x23, 0x67},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x24, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x25, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x26, 0x45},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2
 {2,  {0x27, 0x67},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 2

 {2,  {0x30, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x31, 0x11},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x32, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x33, 0xee},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x34, 0xff},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x35, 0xcb},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x36, 0xda},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x37, 0xad},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x38, 0xbc},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x39, 0x76},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3a, 0x67},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3b, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3c, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3d, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3e, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x3f, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3
 {2,  {0x40, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GIP Control 3

 {2,  {0x53, 0x10},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GOUT VGLO Control
 {2,  {0x54, 0x10},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // GOUT VGHO Control

 {6,  {0xFF, 0xFF, 0x98, 0x06, 0x04, 0x07}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE,        MIPI_DSI_CMD_FLAG_LOW_POWER},        // Change to Page 7 CMD for Normal command
 {2,  {0x18, 0x1d},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // VREG1/2OUT ENABLE
 {2,  {0x26, 0xb2},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
 {2,  {0x02, 0x77},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
 {2,  {0xe1, 0x79},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
 {2,  {0x17, 0x22},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // VGL_REG Disable

 {6,  {0xFF, 0xFF, 0x98, 0x06, 0x04, 0x00}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE,        MIPI_DSI_CMD_FLAG_LOW_POWER},        // Change to Page 0 CMD for Normal command
 {120, {0},                                 MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG,   (mipi_dsi_cmd_flag_t)0},         // Sleep out command may not be issued within 120 ms of GPIO HW reset. Wait to ensure timing maintained.
 {2,  {0x11, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},// Sleep-Out
 {5,   {0},                                 MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG,   (mipi_dsi_cmd_flag_t)0},         // Delay 5msec
 {2,  {0x29, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},// Display on
 {2,  {0x3a, 0x70},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},      // 24-bit / pixel

 {0x00, {0},                                MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE, (mipi_dsi_cmd_flag_t)0},         // End of table


#else           // this config data for 10.1 inch panel
#if 0
      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {4,  {0xFF, 0x98,0x81,0x03},               MIPI_DSI_CMD_ID_DCS_LONG_WRITE,             MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x01, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x02, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x03, 0x53},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x04, 0xD3},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x05, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x06, 0x0D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x07, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x08, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x09, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0d, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0e, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x0f, 0x28},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x10, 0x28},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x11, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x12, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x13, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x14, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x15, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x16, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x17, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x18, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x19, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1d, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1e, 0x40},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x1f, 0x80},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x20, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x21, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x22, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x23, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x24, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x25, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x26, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x27, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x28, 0x33},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x29, 0x33},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2d, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2e, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x2f, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x30, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x31, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x32, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x33, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x34, 0x03},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x35, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x36, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x37, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x38, 0x96},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x39, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3a, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3d, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3e, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3f, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x40, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x41, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x42, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x43, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x44, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x50, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x51, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x52, 0x45},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x53, 0x67},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x54, 0x89},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x55, 0xAB},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x56, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x57, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x58, 0x45},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x59, 0x67},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5a, 0x89},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5b, 0xAB},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5c, 0xCD},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5d, 0xEF},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5e, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x5f, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x60, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x61, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x62, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x63, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x64, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x65, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x66, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x67, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x68, 0x15},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x69, 0x15},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6a, 0x14},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6b, 0x14},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6c, 0x0D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6d, 0x0D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6e, 0x0C},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6f, 0x0C},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x70, 0x0F},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x71, 0x0F},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x72, 0x0E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x73, 0x0E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x74, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x75, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x76, 0x08},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x77, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x78, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x79, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7a, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7b, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7c, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7d, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7e, 0x15},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x7f, 0x15},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x80, 0x14},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x81, 0x14},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x82, 0x0D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x83, 0x0D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x84, 0x0C},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x85, 0x0C},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x86, 0x0F},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x87, 0x0F},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x88, 0x0E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x89, 0x0E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x8A, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {4,  {0xFF, 0x98,0x81,0x04},               MIPI_DSI_CMD_ID_DCS_LONG_WRITE,      MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6E, 0x2B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x6F, 0x37},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3A, 0x24},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x8D, 0x1A},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x87, 0xBA},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB2, 0xD1},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x88, 0x0B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x38, 0x01},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x39, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB5, 0x02},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x31, 0x25},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x3B, 0x98},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {4,  {0xFF, 0x98,0x81,0x01},               MIPI_DSI_CMD_ID_DCS_LONG_WRITE,      MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x22, 0x0A},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x31, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x53, 0x3D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x55, 0x3D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x50, 0xB5},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x51, 0xAD},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x60, 0x06},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x62, 0x20},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB7, 0x03},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA0, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA1, 0x21},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA2, 0x35},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA3, 0x19},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA4, 0x1E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA5, 0x33},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA6, 0x27},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA7, 0x26},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA8, 0xAF},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xA9, 0x1B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAA, 0x27},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAB, 0x8D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAC, 0x1A},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAD, 0x1B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAE, 0x50},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xAF, 0x26},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB0, 0x2B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB1, 0x54},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB2, 0x5E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xB3, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC0, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC1, 0x21},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC2, 0x35},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC3, 0x19},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC4, 0x1E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC5 ,0x33},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC6, 0x27},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC7, 0x26},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC8, 0xAF},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xC9, 0x1B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCA, 0x27},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCB, 0x8D},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCC, 0x1A},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCD, 0x1B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCE ,0x50},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xCF, 0x26},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xD0, 0x2B},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xD1, 0x54},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xD2, 0x5E},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0xD3, 0x23},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {4,  {0xFF, 0x98,0x81,0x00},               MIPI_DSI_CMD_ID_DCS_LONG_WRITE,              MIPI_DSI_CMD_FLAG_LOW_POWER},

      {120, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},  // 120ms

      {2,  {0x11, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x29, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {5, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},    // 5ms
      {2,  {0x35, 0x00},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {120, {0}, MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_id_t)0},  // 120ms

      {0x00, {0},                                MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE, (mipi_dsi_cmd_id_t)0},
#else  //   This is for 7inch panel config
#if 0

      {2,  {0xB2, 0x10},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,      MIPI_DSI_CMD_FLAG_LOW_POWER},

      {2,  {0x80, 0xAC},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x81, 0xB8},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,      MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x82, 0x09},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x83, 0x78},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,      MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x84, 0x7F},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x85, 0xBB},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,      MIPI_DSI_CMD_FLAG_LOW_POWER},
      {2,  {0x86, 0x70},                         MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM,       MIPI_DSI_CMD_FLAG_LOW_POWER},

      {0x00, {0},                                MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE, (mipi_dsi_cmd_id_t)0},
#endif

#endif

#if ST7796U
      // {2,     {0x01, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          // {120,    {0},                                   MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_flag_t)0},
          /* enable the BK function of Command2: BK3 */
          /************** BK3 Function start ***************/
//          {2,     {0x11},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          //0x11, sleep out,退出休眠
          {2,     {0x11, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},

          {2,     {0xF0, 0xC3},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xF0, 0x96},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0x36, 0x48},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0x3A, 0x55},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xB4, 0x01},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},

          //0xB5, VFP=2,VBP=6, HBP=48
//          {5,     {0xB5, 0x02,0x06,0x00,0x30},                           MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},

          {4,     {0xB6, 0x8A, 0x07, 0x3B},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {9,     {0xB6, 0x8A, 0x8A, 0x00,0x00,0x29,0x19,0xA5,0x33},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xB7, 0xC6},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {3,     {0xB9, 0x02,0xE0},                           MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {3,     {0xC0, 0xC0,0x64},                           MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xC1, 0x1D},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xC2, 0xA7},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xC5, 0x18},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {9,     {0xE8, 0x40, 0x8A, 0x00,0x00,0x29,0x19,0xA5,0x33},/*0x25,0x0A,0x38,0x33},*//*0x29,0x19,0xA5,0x33},*/   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          //0xE0, positive gamma 控制
          {15,    {0xE0, 0xF0, 0x0B, 0x12,0x09,0x0A,0x26,0x39,0x54,0x4E,0x38,0x13,0x13,0x2E,0x34},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          //0xE1, negative gamma 控制
          {15,    {0xE1, 0xF0, 0x10, 0x15,0x0D,0x0C,0x07,0x38,0x43,0x4D,0x3A,0x16,0x15,0x30,0x35},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xF0, 0x3C},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0xF0, 0x69},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0x35, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {1,     {0x29},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {1,     {0x21},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {2,     {0x29, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},

          {2,     {0x21, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},

          {5,     {0x2A, 0x00, 0x31, 0x01,0x0E},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {5,     {0x2B, 0x00, 0x00, 0x01,0xDF},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {5,     {0x2B, 0x00, 0x02, 0x01,0xE1},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},

          {2,     {0x2C, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
          {0x00,  {0},                                    MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE, (mipi_dsi_cmd_flag_t)0},

          /*------------- BK3 Function end ----------------*/

          /* enable the BK function of Command2: BK0 */
          /************** BK0 Function start ***************/
//          {6,     {0xFF, 0x77, 0x01, 0x00, 0x00, 0x10},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Display Line Setting: SCNL = (44Line+1)*8 = 360 */
//          {3,     {0xC0, 0x2C, 0x00},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},  // SCNL = (0x2c + 1) * 8 = 360
//          /* Porch Control: VBP(Back-Porch Vertical line) = 13, VFP(Front-Porch Vertical line) = 2*/
//          {3,     {0xC1, 0x0D, 0x02},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},  // VFB=0x08 VBF=0x02
//          /* Inversion selection & Frame Rate Control: Inversion Selection = 2 Dot, minimum number of pclk in each line = 5*/
//          {3,     {0xC2, 0x31, 0x05},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},  // PCLK= 512 + (0x05 * 16) = 592
//          /* TODO 0xC3, default: RGB DE mode, VSP=[x], Low active*, HSP=[x], Low active */
//          /* TODO 0xC3, default: The data is input on the positive edge of DOTCLK, The data DB23-0 is written when ENABLE = 1 */
//          /* TODO 0xC3, default: RGB interface Vsync back porch setting for HV mode = 16 */
//          /* TODO 0xC3, default: RGB interface Hsync back porch setting for HV mode = 8 */
//
//          /* not on data sheet! */
//          {2,     {0xCC, 0x10},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Positive Voltage Gamma Control */
//          {17,    {0xB0, 0x0A, 0x14, 0x1B, 0x0D, 0x10, 0x05, 0x07, 0x08, 0x06, 0x22, 0x03, 0x11, 0x10, 0xAD, 0x31, 0x1B}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},   // Negative Voltage Gamma Control
//          /* Negative Voltage Gamma Control */
//          {17,    {0xB1, 0x0A, 0x14, 0x1B, 0x0D, 0x10, 0x05, 0x07, 0x08, 0x06, 0x22, 0x03, 0x11, 0x10, 0xAD, 0x31, 0x1B}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /*------------- BK0 Function end ----------------*/
//
//          /* enable the BK function of Command2: BK1 */
//          /************** BK1 Function start ***************/
//          {6,     {0xFF, 0x77, 0x01, 0x00, 0x00, 0x11},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Vop Amplitude setting: VRH = 80 */
//          {2,     {0xB0, 0x50},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* VCOM amplitude setting: VCOM = 94 (1.275V)*/
//          {2,     {0xB1, 0x5E},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* VGH Voltage setting: VGHSS = 0x87 (15V)*/
//          {2,     {0xB2, 0x87},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* TEST Command Setting: TESTCMD = 0x80 */
//          {2,     {0xB3, 0x80},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* VGL Voltage setting: Gate Low Voltage setting = -9.51V*/
//          {2,     {0xB5, 0x47},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Power Control 1: Gamma OP bias current selection AP = 1(Min), APIS = 1(Min), APOS = 1(Min) */
//          {2,     {0xB7, 0x85},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Power Control 2: AVDD voltage setting = 6.6V, AVCL voltage setting = -4.6V */
//          {2,     {0xB8, 0x21},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Power Control 2: source pumping cell setting SVPO_PUM, SVNO_PUM */
//          // {2,     {0xB9, 0x10},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Source pre_drive timing set1: 8(1.6uS)*/
//          {2,     {0xC1, 0x78},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Source EQ2 Setting */
//          {2,     {0xC2, 0x78},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* MIPI Setting 1: EOT_EN = 1, ERR_SEL = 0 (disable) */
//          {2,     {0xD0, 0x88},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* TODO MIPI Setting 2, default: */
//          /* TODO MIPI Setting 3, default: */
//          /* TODO MIPI Setting 4, default: */
//          {2,     {0xE0, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {2,     {0x1B, 0x02},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//
//          {12,    {0xE1, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44},       MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {13,    {0xE2, 0x11, 0x11, 0x44, 0x44, 0x75, 0xA0, 0x00, 0x00, 0x74, 0xA0, 0x00, 0x00}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {5,     {0xE3, 0x00, 0x00, 0x11, 0x11},         MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {3,     {0xE4, 0x44, 0x44},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {17,    {0xE5, 0x0A, 0x71, 0xD8, 0xA0, 0x0C, 0x73, 0xD8, 0xA0, 0x0E, 0x75, 0xD8, 0xA0, 0x10, 0x77, 0xD8, 0xA0}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {5,     {0xE6, 0x00, 0x00, 0x11, 0x11},         MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {3,     {0xE7, 0x44, 0x44},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {17,    {0xE8, 0x09, 0x70, 0xD8, 0xA0, 0x0B, 0x72, 0xD8, 0xA0, 0x0D, 0x74, 0xD8, 0xA0, 0x0F, 0x76, 0xD8, 0xA0}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {8,     {0xEB, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40},                                                       MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {3,     {0xEC, 0x3C, 0x00},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {17,    {0xED, 0xAB, 0x89, 0x76, 0x54, 0x02, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0x20, 0x45, 0x67, 0x98, 0XBA}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {7,     {0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54},                                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /*------------- BK1 Function end -----------------/
//
//          /* enable the BK function of Command2: BK3 */
//          /************** BK3 Function start ***************/
//          {6,     {0xFF, 0x77, 0x01, 0x00, 0x00, 0x13},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//
//          {3,     {0xE8, 0x00, 0x0E},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {3,     {0xE8, 0x00, 0x0C},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {3,     {0xE8, 0x00, 0x00},                     MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /*------------- BK3 Function end -----------------/
//
//          /* disable the BK function of Command2 */
//          {6,     {0xFF, 0x77, 0x01, 0x00, 0x00, 0x00},   MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
//
//          /* Interface Pixel Format: 0x55(16-bit/pixel) 0x66(18-bit/pixel) 0x77(24-bit/pixel) */
//          {2,     {0x3A, 0x55},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Display data access control: Get normal scan , BGR mode */
//          // {2,     {0x36, 0x48},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Display data access control: Get normal scan , RGB mode */
//          {2,     {0x36, 0x40},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Sleep Out */
//          {2,     {0x11, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          {120,    {0},                                   MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_flag_t)0},
//          /* Display On */
//          {2,     {0x29, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /* Tearing Effect Line ON: TE consists of V-Blanking information only */
//          // {2,     {0x35, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          // {60,    {0},                                    MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG, (mipi_dsi_cmd_flag_t)0},
//          /*  All Pixel ON */
//          // {2,     {0x23, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//          /*  Display Inversion On */
//          // {2,     {0x21, 0x00},                           MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
//
//          {0x00,  {0},                                    MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE, (mipi_dsi_cmd_flag_t)0},
#endif

#endif
};

/*******************************************************************************************************************//**
 * @brief      Initialize LCD
 *
 * @param[in]  table  LCD Controller Initialization structure.
 * @retval     None.
 **********************************************************************************************************************/
void mipi_dsi_push_table (const lcd_table_setting_t *table)
{
    fsp_err_t err = FSP_SUCCESS;
    const lcd_table_setting_t *p_entry = table;

    while (MIPI_DSI_DISPLAY_CONFIG_DATA_END_OF_TABLE != p_entry->cmd_id)
    {
        mipi_dsi_cmd_t msg =
        {
          .channel = 0,
          .cmd_id = p_entry->cmd_id,
          .flags = p_entry->flags,
          .tx_len = p_entry->size,
          .p_tx_buffer = p_entry->buffer,
        };

        if (MIPI_DSI_DISPLAY_CONFIG_DATA_DELAY_FLAG == msg.cmd_id)
        {
            R_BSP_SoftwareDelay (table->size, BSP_DELAY_UNITS_MILLISECONDS);
        }
        else
        {
            g_message_sent = false;
            /* Send a command to the peripheral device */
            err = R_MIPI_DSI_Command (&g_mipi_dsi0_ctrl, &msg);
            handle_error(err, "** MIPI DSI Command API failed ** \r\n");
            /* Wait */
            while (!g_message_sent);
        }
        p_entry++;
    }
}

#define RGB_565_RED    (0x1F << 11)
#define RGB_565_GREEN  (0x3F << 5)
#define RGB_565_BLUE   (0x1F << 0)
#define RGB_565_WHITE   (0xFFFF)
#define RGB_565_GRAY   (0xFFFF<<2)

#define act_hz 222
#define act_vz 480

//uint16_t color[4] = {RGB_565_RED, RGB_565_GREEN, RGB_565_BLUE, RGB_565_WHITE };
//uint16_t color[4+8] = {RGB_565_RED,RGB_565_GREEN,RGB_565_BLUE,RGB_565_WHITE,0xe6d7, 0xcee7,0xe5c0,0xd1cf,0x67FC,0xCE7F,0xFEA0,0xC618 } ;
uint16_t color[4+8] = {RGB_565_RED,RGB_565_GREEN,RGB_565_BLUE,RGB_565_WHITE,0xe6d7, 0xcee7,0xe5c0,0xd1cf,0x67FC,0xCE7F,0xFEA0,0xC618 };
uint32_t color_32bit[4] = {0xffffffff,0xffff0000,0xff00ff00,0xff0000ff};
typedef enum
{
    simple = 0,
    partition = 1,
    gradient = 2
} color_pattern_t;

void show_pattern(color_pattern_t pattern);

color_pattern_t color_p = simple;

//uint16_t color_temp;
void show_RGB(uint8_t R, uint8_t G, uint8_t B);
void show_RGB(uint8_t R, uint8_t G, uint8_t B)
{
    uint16_t * p = (uint16_t *)&fb_background[0][0];
    uint16_t color_temp;
    color_temp = (uint16_t)(((R&0x1F)<<11)|((G&0x3F)<<5)|(B&0x1F));

    for(uint32_t x=0;x<g_vr_size+100;x++) //g_vr_size
    {
        for(uint32_t y=0;y<g_hz_size;y++)
        {
            p[y+x*g_hz_size] = color_temp;
        }
        //color_temp+=0x1863;
    }

}

void show_GRAY();
void show_GRAY()
{
    uint16_t * p = (uint16_t *)&fb_background[0][0];
    uint16_t color_temp;
    color_temp = (uint16_t)((((0x1F/8)&0x1F)<<11)|(((0x3F/8)&0x3F)<<5)|((0x1F/8)&0x1F));

    for(uint32_t x=0;x<g_vr_size+100;x++) //g_vr_size
    {
        for(uint32_t y=0;y<g_hz_size;y++)
        {
            p[y+x*g_hz_size] = color_temp;
        }
        color_temp+=0x0821;
    }

}


void show_pic()
{
    uint8_t * p = (uint8_t *)&fb_background[0][0];
    R_BSP_SoftwareDelay(400, BSP_DELAY_UNITS_MILLISECONDS);

    for(uint32_t x=0;x<g_vr_size*2;x++)
    {
        for(uint32_t y=0;y<g_hz_size;y++)
        {

           p[y+x*(g_hz_size)] = gImage_qier[y+x*(g_hz_size-34)];

        }
        R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

void show_pattern(color_pattern_t pattern)
{
    uint16_t * p = (uint16_t *)&fb_background[0][0];
    R_BSP_SoftwareDelay(400, BSP_DELAY_UNITS_MILLISECONDS);
    count++;
    if(count>=sizeof(color))
    {
        count=0;
    }

    switch(pattern){
        case simple:
            for(uint32_t x=0;x<g_vr_size;x++) //g_vr_size
            {
                for(uint32_t y=0;y<g_hz_size;y++)
                {
                    p[y+x*g_hz_size] = color[count];
                }
            }
            break;
        case partition:
            for(uint32_t x=0;x<g_vr_size/2;x++)
            {
                for(uint32_t y=0;y<(g_hz_size-34)/2;y++)
                {
                    p[y+x*g_hz_size] = color[0];
                }
//                R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
            }

            for(uint32_t x=0;x<g_vr_size/2;x++)
            {
                for(uint32_t y=(g_hz_size-34)/2;y<g_hz_size;y++)
                {
                    p[y+x*g_hz_size] = color[1];
                }
//                R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
            }
            for(uint32_t x=g_vr_size/2;x<g_vr_size;x++)
            {
                for(uint32_t y=0;y<act_hz/2;y++)
                {
                    p[y+x*g_hz_size] = color[2];
                }
//                R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
            }
            for(uint32_t x=g_vr_size/2;x<g_vr_size;x++)
            {
                for(uint32_t y=act_hz/2;y<g_hz_size;y++)
                {
                    p[y+x*g_hz_size] = color[3];
                }
//                R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
            }
            break;
        case gradient:
            for(uint32_t x=0;x<g_vr_size;x++)
            {
                for(uint32_t y=0;y<g_hz_size;y++)
                {
                    p[y+x*g_hz_size] = x;
                }
            }
            break;
        default :
            break;

    }
#if defined(RENESAS_CORTEX_M85)
#if (BSP_CFG_DCACHE_ENABLED)
    int32_t size;
    /* Invalidate cache - so the HW can access any data written by the CPU */

        size = sizeof(fb_background[0])/2;

    SCB_CleanInvalidateDCache_by_Addr(p, size);
#endif
#endif
        R_GLCDC_BufferChange (&g_display_ctrl, (uint8_t*) p, DISPLAY_FRAME_LAYER_1);

        /* Wait for a Vsync event */
        g_vsync_flag = RESET_FLAG;
        while (g_vsync_flag);
}


///extern void mpu_direct_config(void);
//void DWT_init();
//uint32_t DWT_get_count();
//void DWT_clean_count();
//uint32_t DWT_count_to_us(uint32_t delta_count);
//
//#define DWT_DEM *(uint32_t*)0xE000EDFC
//
//void DWT_init()
//{
//    DWT->CTRL = 0;
//    DWT_DEM |= 1<<24;
//    DWT->CYCCNT = 0;
//    DWT->CTRL |= 1<<0;
//}
//
//
//uint32_t DWT_get_count()
//{
//    return DWT->CYCCNT;
//}
//
//void DWT_clean_count()
//{
//    DWT->CYCCNT = 0;
//}
//
//uint32_t DWT_count_to_us(uint32_t delta_count)
//{
//    return delta_count/480;
//}
//
//
//uint32_t DWT_pre_count=0, DWT_post_count=0, time_sdram_access=0;
//uint32_t DWT_delta=0;

uint32_t pixel=0;
/*******************************************************************************************************************//**
 * @brief      Start video mode and draw color bands on the LCD screen
 *
 * @param[in]  None.
 * @retval     None.
 **********************************************************************************************************************/
void mipi_dsi_start_display(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint8_t color_count;

    /* Get LCDC configuration */
    g_hz_size = (g_display_cfg.input[0].hsize);
    g_vr_size = (g_display_cfg.input[0].vsize);
    g_hstride = (g_display_cfg.input[0].hstride);
//    SCB_EnableDCache();
    DWT_init();
    DWT_clean_count();

    uint16_t * p = (uint16_t *)&fb_background[0][0];
    DWT_pre_count = DWT_get_count();
    for(uint32_t x=0;x<g_vr_size;x++) //g_vr_size
    {
        for(uint32_t y=0;y<g_hz_size;y++)
        {
            p[y+x*g_hz_size] = y;
        }
    }

    DWT_post_count = DWT_get_count();

    DWT_delta = DWT_post_count - DWT_pre_count;

    APP_PRINT("\r\n");
    APP_PRINT("Before start GLCDC, DWT count:%d\r\n", DWT_delta);
    APP_PRINT("Before start GLCDC, time:%d\r\n", DWT_count_to_us(DWT_delta) );

    APP_PRINT("Before start GLCDC, speed:%dMB/s\r\n", (g_hz_size*g_vr_size*2)/(DWT_count_to_us(DWT_delta)) );
//    APP_PRINT("Before start GLCDC, speed:%dMB/s\r\n", (g_hz_size*g_vr_size*4)/(DWT_count_to_us(DWT_delta)) ); // 32bit


    /* Initialize buffer pointers */
    g_buffer_size = (uint32_t) (g_hz_size * g_vr_size * BYTES_PER_PIXEL);
    gp_single_buffer = (uint32_t*) g_display_cfg.input[0].p_base;

    /* Double buffer for drawing color bands with good quality */
    gp_double_buffer = gp_single_buffer + g_buffer_size;

    /* Get timer information */
    err = R_GPT_InfoGet (&g_timer0_ctrl, &timer_info);
    /* Handle error */
    handle_error(err, "** GPT InfoGet API failed ** \r\n");
    R_BSP_SoftwareDelay(500,1000);
    R_BSP_SoftwareDelay(500,1000);
    R_BSP_SoftwareDelay(500,1000);

    /* Start video mode */
    err = R_GLCDC_Start(&g_display_ctrl);

    /* Handle error */
    handle_error(err, "** GLCDC Start API failed ** \r\n");

    /* Enable external interrupt */
    err = R_ICU_ExternalIrqEnable(&g_external_irq_ctrl);
    /* Handle error */
    handle_error(err, "** ICU ExternalIrqEnable API failed ** \r\n");

    DWT_clean_count();
    DWT_pre_count = DWT_get_count();
        for(uint32_t x=0;x<g_vr_size;x++) //g_vr_size
        {
            for(uint32_t y=0;y<g_hz_size;y++)
            {
                p[y+x*g_hz_size] = color[0];
            }
        }



     DWT_post_count = DWT_get_count();
     DWT_delta = DWT_post_count - DWT_pre_count;

     APP_PRINT("After start GLCDC, DWT count:%d\r\n", DWT_delta);
     APP_PRINT("After start GLCDC, time:%d\r\n", DWT_count_to_us(DWT_delta) );

     APP_PRINT("After start GLCDC, speed:%dMB/s\r\n", (g_hz_size*g_vr_size*2)/(DWT_count_to_us(DWT_delta)) );

     R_BSP_SoftwareDelay(500,1000);

//     uint8_t color_R,color_G,color_B;
//     while(1){
////         color_R = 0x1F/2;
////         color_G = 0x3F/2;
////         color_B = 0x1F/2;
////         show_RGB(color_R,color_G,color_B);
//         show_GRAY();
//     }


//while(1){
//        show_pattern(partition);
////        show_pic();
//    }


}

/*******************************************************************************************************************//**
 * @brief      User-defined function to draw the current display to a framebuffer.
 *
 * @param[in]  framebuffer   Pointer to frame buffer.
 * @retval     FSP_SUCCESS : Upon successful operation, otherwise: failed.
 **********************************************************************************************************************/
static uint8_t mipi_dsi_set_display_time (void)
{
    fsp_err_t err = FSP_SUCCESS;
    if(APP_CHECK_DATA)
        {
            /* Conversion from input string to integer value */
            read_data = process_input_data();
            switch (read_data)
            {
                /* Set 5 seconds to enter Ultra-Low Power State (ULPS)  */
                case RTT_SELECT_5S:
                {
                    APP_PRINT(MIPI_DSI_INFO_5S);
                    period_sec = GPT_DESIRED_PERIOD_5SEC;
                    break;
                }

                /* Set 15 seconds to enter Ultra-Low Power State (ULPS)  */
                case RTT_SELECT_15S:
                {
                    APP_PRINT(MIPI_DSI_INFO_15S);
                    period_sec = GPT_DESIRED_PERIOD_15SEC;
                    break;
                }

                /* Set 30 seconds to enter Ultra-Low Power State (ULPS)  */
                case RTT_SELECT_30S:
                {
                    APP_PRINT(MIPI_DSI_INFO_30S);
                    period_sec = GPT_DESIRED_PERIOD_30SEC;
                    break;
                }
                /* Stop timer to always display.*/
                case RTT_SELECT_DISABLE_ULPS:
                {
                    APP_PRINT(MIPI_DSI_INFO_DISABLE_ULPS);
                    g_timer_overflow = RESET_FLAG;
                    err = R_GPT_Stop (&g_timer0_ctrl);
                    APP_ERR_RETURN(err, " ** GPT Stop API failed ** \r\n");
                    break;
                }
                default:
                {
                    APP_PRINT("\r\nInvalid Option Selected\r\n");
                    APP_PRINT(MIPI_DSI_MENU);
                    break;
                }
            }

            if (RTT_SELECT_DISABLE_ULPS != read_data)
            {
                /* Calculate the desired period based on the current clock. Note that this calculation could overflow if the
                 * desired period is larger than UINT32_MAX / pclkd_freq_hz. A cast to uint64_t is used to prevent this. */
                uint32_t period_counts = (uint32_t) (((uint64_t) timer_info.clock_frequency * period_sec) / GPT_UNITS_SECONDS);
                /* Set the calculated period. */
                err = R_GPT_PeriodSet (&g_timer0_ctrl, period_counts);
                APP_ERR_RETURN(err, " ** GPT PeriodSet API failed ** \r\n");
                err = R_GPT_Reset (&g_timer0_ctrl);
                APP_ERR_RETURN(err, " ** GPT Reset API failed ** \r\n");
                g_timer_overflow = RESET_FLAG;
                err = R_GPT_Start (&g_timer0_ctrl);
                APP_ERR_RETURN(err, " ** GPT Start API failed ** \r\n");
            }
            /* Reset buffer*/
            read_data = RESET_VALUE;
        }

        return err;
}



/*******************************************************************************************************************//**
 * @brief      User-defined function to draw the current display to a framebuffer.
 *
 * @param[in]  framebuffer    Pointer to frame buffer.
 * @retval     None.
 **********************************************************************************************************************/
static void display_draw (uint32_t * framebuffer)
{
    /* Draw buffer */
    uint32_t color[COLOR_BAND_COUNT]= {BLUE, LIME, RED, BLACK, WHITE, YELLOW, AQUA, MAGENTA};
    uint16_t bit_width = g_hz_size / COLOR_BAND_COUNT;
//    memset(framebuffer , 0x00, g_hz_size*g_vr_size);
    for (uint32_t y = 0; y < g_vr_size; y++)
    {
        for (uint32_t x = 0; x < g_hz_size; x++)
        {
            uint32_t bit       = x / bit_width;
            framebuffer[x+y*800] = color [bit];
//            framebuffer[x+1] = color [2];
//            framebuffer[x+2] = color [3];
//            framebuffer[x+3] = color [4];
//            rega_8 = vld1q_u8(&color[1]);
//            vst1q_u8(&framebuffer[x], rega_8);

//            rega_8 = vld1q_u32(&color[1]);
//            vst1q_u32(&framebuffer[x], rega_8);
        }
        framebuffer += g_hstride;
    }
//    for (uint32_t y = 0; y < g_vr_size; y++)
//        {
//            for (uint32_t x = g_hz_size/2; x < g_hz_size; x++)
//            {
//                uint32_t bit       = x / bit_width;
//    //            color [1] = x;
//                framebuffer[x+y*800] = 0x0000FF00;
//    //            framebuffer[x+1] = color [2];
//    //            framebuffer[x+2] = color [3];
//    //            framebuffer[x+3] = color [4];
//    //            rega_8 = vld1q_u8(&color[1]);
//    //            vst1q_u8(&framebuffer[x], rega_8);
//
//    //            rega_8 = vld1q_u32(&color[1]);
//    //            vst1q_u32(&framebuffer[x], rega_8);
//            }
//            framebuffer += g_hstride;
//        }


//    neon_memcpy(framebuffer , &color[1] , 854*480*4);


}

/*******************************************************************************************************************//**
 * @brief      Touch IRQ callback function
 * NOTE:       This function will return to the highest priority available task.
 * @param[in]  p_args  IRQ callback data
 * @retval     None.
 **********************************************************************************************************************/
void external_irq_callback(external_irq_callback_args_t *p_args)
{
    if (g_external_irq_cfg.channel == p_args->channel)
    {
        g_irq_state =true;
    }
}

/*****************************************************************************************************************
 *  @brief      Process input string to integer value
 *
 *  @param[in]  None
 *  @retval     integer value of input string.
 ****************************************************************************************************************/
static uint8_t process_input_data(void)
{
    unsigned char buf[BUFFER_SIZE_DOWN] = {INITIAL_VALUE};
    uint32_t num_bytes                  = RESET_VALUE;
    uint8_t  value                      = RESET_VALUE;

    while (RESET_VALUE == num_bytes)
    {
        if (APP_CHECK_DATA)
        {
            num_bytes = APP_READ(buf);
            if (RESET_VALUE == num_bytes)
            {
                APP_PRINT("\r\nInvalid Input\r\n");
            }
        }
    }

    /* Conversion from input string to integer value */
    value =  (uint8_t) (atoi((char *)buf));
    return value;
}

/*******************************************************************************************************************//**
 * @brief      This function is used to enter Ultra-low Power State (ULPS) and turn off the backlight.
 *
 * @param[in]  none
 * @retval     none
 **********************************************************************************************************************/
static void mipi_dsi_ulps_enter(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t timedelay_us = ENTER_ULPS_DELAY;
    /* Enter Ultra-low Power State (ULPS) */
    g_phy_status = MIPI_DSI_PHY_STATUS_NONE;
    err = R_MIPI_DSI_UlpsEnter (&g_mipi_dsi0_ctrl, (mipi_dsi_lane_t) MIPI_DSI_LANE_DATA_ALL);
    handle_error (err, "** MIPI DSI UlpsEnter API failed ** \r\n");

    /* Wait for a ULPS event */
    err = wait_for_mipi_dsi_event(MIPI_DSI_PHY_STATUS_DATA_LANE_ULPS_ENTER);
    handle_error (err, "** MIPI DSI phy event timeout ** \r\n");
    g_ulps_flag = SET_FLAG;
    APP_PRINT("\r\nEntered Ultra-low Power State (ULPS)\r\n");

    /* Wait about 8 seconds */
    while (!g_irq_state)
    {
        timedelay_us--;
        R_BSP_SoftwareDelay (1U, BSP_DELAY_UNITS_MICROSECONDS);

        /* Check for time elapse*/
        if (RESET_VALUE == timedelay_us)
        {
            /* Display Off */
            R_IOPORT_PinWrite (&g_ioport_ctrl, PIN_DISPLAY_BACKLIGHT, BSP_IO_LEVEL_LOW);
            break;
        }
    }
}

/*******************************************************************************************************************//**
 * @brief      This function is used to exit Ultra-low Power State (ULPS) and turn on the backlight.
 *
 * @param[in]  none
 * @retval     none
 **********************************************************************************************************************/
static void mipi_dsi_ulps_exit(void)
{
    fsp_err_t err = FSP_SUCCESS;
    /* Exit Ultra-low Power State (ULPS) */
    g_phy_status = MIPI_DSI_PHY_STATUS_NONE;
    err = R_MIPI_DSI_UlpsExit (&g_mipi_dsi0_ctrl, (mipi_dsi_lane_t) (MIPI_DSI_LANE_DATA_ALL));
    handle_error (err, "** MIPI DSI UlpsExit API failed ** \r\n");

    /* Wait for a ULPS event */
    err = wait_for_mipi_dsi_event(MIPI_DSI_PHY_STATUS_DATA_LANE_ULPS_EXIT);
    handle_error (err, "** MIPI DSI phy event timeout ** \r\n");
    g_ulps_flag = RESET_FLAG;
    APP_PRINT("\r\nExited Ultra-low Power State (ULPS) due to touch with co-ordinates x: %u, ; y: %u. \r\n", touch_coordinates[0].x, touch_coordinates[0].y);

    /* Display On */
    R_IOPORT_PinWrite (&g_ioport_ctrl, PIN_DISPLAY_BACKLIGHT, BSP_IO_LEVEL_HIGH);
}

/*******************************************************************************************************************//**
 * @brief      This function is used to reset the LCD after power on.
 *
 * @param[in]  none
 * @retval     none
 **********************************************************************************************************************/
void touch_screen_reset(void)
{
     /* Reset touch chip by setting GPIO reset pin low. */
    R_BSP_PinAccessEnable();

//     R_IOPORT_PinWrite(&g_ioport_ctrl, PIN_DISPLAY_RST, BSP_IO_LEVEL_LOW);
//     R_IOPORT_PinCfg(&g_ioport_ctrl, PIN_DISPLAY_INT, IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
//     R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MICROSECONDS);
//
//     /* Start Delay to set the device slave address to 0x28/0x29 */
//     R_IOPORT_PinWrite(&g_ioport_ctrl, PIN_DISPLAY_INT, BSP_IO_LEVEL_HIGH);
//     R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MICROSECONDS);
//
//     /* Release touch chip from reset */
//     R_IOPORT_PinWrite(&g_ioport_ctrl, PIN_DISPLAY_RST, BSP_IO_LEVEL_HIGH);
//     R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
//
//     /* Set GPIO INT pin low */
//     R_IOPORT_PinWrite(&g_ioport_ctrl, PIN_DISPLAY_INT, BSP_IO_LEVEL_LOW);
//     R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
//
//      /* Release Touch chip interrupt pin for control  */
//     R_IOPORT_PinCfg(&g_ioport_ctrl, PIN_DISPLAY_INT, IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_EVENT_RISING_EDGE | IOPORT_CFG_IRQ_ENABLE);


     R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);
     R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MICROSECONDS);
     R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_LOW);
     R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MICROSECONDS);
     R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);
     R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);

}

/*******************************************************************************************************************//**
 * @brief       This function is used to Wait for mipi dsi event.
 *
 * @param[in]   event   : Expected events
 * @retval      FSP_SUCCESS : Upon successful operation, otherwise: failed
 **********************************************************************************************************************/
static fsp_err_t wait_for_mipi_dsi_event (mipi_dsi_phy_status_t event)
{
    uint32_t timeout = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_ICLK) / 10;
    while (timeout-- && ((g_phy_status & event) != event))
    {
        ;
    }
    return timeout ? FSP_SUCCESS : FSP_ERR_TIMEOUT;
}
/*******************************************************************************************************************//**
 *  @brief       This function handles errors, closes all opened modules, and prints errors.
 *
 *  @param[in]   err       error status
 *  @param[in]   err_str   error string
 *  @retval      None
 **********************************************************************************************************************/
void handle_error (fsp_err_t err,  const char * err_str)
{
    if(FSP_SUCCESS != err)
    {
        /* Print the error */
        APP_ERR_PRINT(err_str);

        /* Close opened GPT module*/
        if(RESET_VALUE != g_timer0_ctrl.open)
        {
            if(FSP_SUCCESS != R_GPT_Close (&g_timer0_ctrl))
            {
                APP_ERR_PRINT("GPT Close API failed\r\n");
            }
        }
        /* Close opened GLCD module*/
        if(RESET_VALUE != g_display_ctrl.state)
        {

            if(FSP_SUCCESS != R_GLCDC_Close (&g_display_ctrl))
            {
                APP_ERR_PRINT("GLCDC Close API failed\r\n");
            }

        }

        /* Close opened ICU module*/
        if(RESET_VALUE != g_external_irq_ctrl.open)
        {
            if(FSP_SUCCESS != R_ICU_ExternalIrqClose (&g_external_irq_ctrl))
            {
                APP_ERR_PRINT("ICU ExternalIrqClose API failed\r\n");
            }
        }

        /* Close opened IIC master module*/
        if(RESET_VALUE != g_i2c_master1_ctrl.open)
        {
            if(FSP_SUCCESS != R_IIC_MASTER_Close(&g_i2c_master1_ctrl))
            {
                APP_ERR_PRINT("IIC MASTER Close API failed\r\n");
            }
        }

        APP_ERR_TRAP(err);
    }
}

/*******************************************************************************************************************//**
 * @brief      Callback functions for GLCDC interrupts
 *
 * @param[in]  p_args    Callback arguments
 * @retval     none
 **********************************************************************************************************************/
void glcdc_callback (display_callback_args_t * p_args)
{
    if (DISPLAY_EVENT_LINE_DETECTION == p_args->event)
    {
        g_vsync_flag = SET_FLAG;
    }
}

/*******************************************************************************************************************//**
 * @brief      Callback functions for MIPI DSI interrupts
 *
 * @param[in]  p_args    Callback arguments
 * @retval     none
 **********************************************************************************************************************/
void mipi_dsi_callback(mipi_dsi_callback_args_t *p_args)
{
    switch (p_args->event)
    {
        case MIPI_DSI_EVENT_SEQUENCE_0:
        {
            if (MIPI_DSI_SEQUENCE_STATUS_DESCRIPTORS_FINISHED == p_args->tx_status)
            {
                g_message_sent = SET_FLAG;
            }
            break;
        }
        case MIPI_DSI_EVENT_PHY:
        {
            g_phy_status |= p_args->phy_status;
            break;
        }
        default:
        {
            break;
        }

    }
}

/*******************************************************************************************************************//**
 * @brief      Callback functions for gpt interrupts
 *
 * @param[in]  p_args    Callback arguments
 * @retval     none
 **********************************************************************************************************************/
void gpt_callback(timer_callback_args_t *p_args)
{
    /* Check for the event */
    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        g_timer_overflow = SET_FLAG;
    }
}
uint16_t color_dtcm[1]  = {RGB_565_RED};

void ST7796U_init();
void ST7796U_init_HW();
void ST7796U_init_HW()
{
    R_BSP_PinAccessEnable();
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(4, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);

}
void ST7796U_init()
{

//    R_SPI_B_Open(&g_spi1_ctrl, &g_spi1_cfg);
//    R_SPI_B_Write(&g_spi1_ctrl, p_src, length, bit_width);

}


/*******************************************************************************************************************//**
 * @brief      This function is used initialize related module and start display operation.
 *
 * @param[in]  none
 * @retval     none
 **********************************************************************************************************************/
void mipi_dsi_entry(void)
{
    fsp_err_t          err        = FSP_SUCCESS;
    fsp_pack_version_t version    = {RESET_VALUE};
    g_hz_size = (g_display_cfg.input[0].hsize);
    g_vr_size = (g_display_cfg.input[0].vsize);
    g_hstride = (g_display_cfg.input[0].hstride);

    /* version get API for FLEX pack information */
    R_FSP_VersionGet(&version);

    /* Project information printed on the Console */
    APP_PRINT(BANNER_INFO, EP_VERSION, version.version_id_b.major, version.version_id_b.minor, version.version_id_b.patch);
    APP_PRINT(EP_INFO);
    APP_PRINT(MIPI_DSI_MENU);

    /* Initialize SDRAM. */
    bsp_sdram_init();
    ST7796U_init_HW();
    /* Get LCDC configuration */
        g_hz_size = (g_display_cfg.input[0].hsize);
        g_vr_size = (g_display_cfg.input[0].vsize);
        g_hstride = (g_display_cfg.input[0].hstride);

        DWT_init();
        DWT_clean_count();

//        uint32_t * p = (uint32_t *)&fb_background[0][0];
//        DWT_pre_count = DWT_get_count();
//            for(uint32_t x=0;x<g_vr_size;x++) //g_vr_size
//            {
//                for(uint32_t y=0;y<g_hz_size;y++)
//                {
//                    p[y+x*g_hz_size] = color_32bit[0];
//                }
//            }
//        SCB_EnableDCache();
    uint16_t * p = (uint16_t *)&fb_background[0][0];
        DWT_pre_count = DWT_get_count();
            for(uint32_t x=0;x<g_vr_size;x++) //g_vr_size
            {
                for(uint32_t y=0;y<g_hz_size;y++)
                {
                    p[y+x*g_hz_size] = color[0];
                }
            }
        DWT_post_count = DWT_get_count();

        DWT_delta = DWT_post_count - DWT_pre_count;

        APP_PRINT("\r\n");
        APP_PRINT("11Before open GLCDC, DWT count:%d\r\n", DWT_delta);
        APP_PRINT("11Before open GLCDC, time:%d\r\n", DWT_count_to_us(DWT_delta) );

        APP_PRINT("11Before open GLCDC, speed:%dMB/s\r\n", (g_hz_size*g_vr_size*2)/(DWT_count_to_us(DWT_delta)) );
//        APP_PRINT("Before start GLCDC, speed:%dMB/s\r\n", (g_hz_size*g_vr_size*4)/(DWT_count_to_us(DWT_delta)) ); // 32bit

    /* Initialize GLCDC module */
    err = R_GLCDC_Open(&g_display_ctrl, &g_display_cfg);

    /* Handle error */
    handle_error(err, "** GLCDC driver initialization FAILED ** \r\n");

    /* Initialize GPT module */
    err = R_GPT_Open(&g_timer0_ctrl, &g_timer0_cfg);
    /* Handle error */
    handle_error(err, "** R_GPT_Open API failed ** \r\n");

    /* LCD reset */
    touch_screen_reset();

    /* Initialize IIC MASTER module */
    err = R_IIC_MASTER_Open(&g_i2c_master1_ctrl, &g_i2c_master1_cfg);
    /* Handle error */
    handle_error(err, "** IIC MASTER Open API failed ** \r\n");

    /* Initialize LCD. */
    mipi_dsi_push_table(g_lcd_init_focuslcd);

    /* Initialize ICU module */
    err = R_ICU_ExternalIrqOpen(&g_external_irq_ctrl, &g_external_irq_cfg);
    /* Handle error */
    handle_error(err, "** ICU ExternalIrqOpen API failed ** \r\n");

    /* Start display 8-color bars */
    mipi_dsi_start_display();
}

/*******************************************************************************************************************//**
 * @} (end addtogroup mipi_dsi_ep)
 **********************************************************************************************************************/
