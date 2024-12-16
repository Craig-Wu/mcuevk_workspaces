//------------------------------------------------------------------------------
// File Name: siHdmiTx_902x_TPI.c
// File Description: this file is used to call initalization of 902x, including HDMI TX TPI,
//            processing EDID, HDCP and CEC.
//
// Create Date: 07/22/2010
//
// Modification history:
//			 - 07/22/2010  Add file desription and comment
//
//
// Copyright ?2002-2010, Silicon Image, Inc.  All rights reserved.
//
// No part of this work may be reproduced, modified, distributed, transmitted,
// transcribed, or translated into any language or computer format, in any form
// or by any means without written permission of: Silicon Image, Inc.,
// 1060 East Arques Avenue, Sunnyvale, California 94085
//------------------------------------------------------------------------------
/*
 *
 * Copyright (c) 2006 Hisilicon Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 *
 * History:
 *      10-April-2006 create this file
 */




#include "sii902_linux.h"
#include "common_utils.h"
#include "hal_data.h"
// #include "main.h"
// #include "i2c.h"
#include "stdio.h"

// I2C Slave Addresses
//====================================================
//#define TX_SLAVE_ADDR       	0x72
#define SII9022_ADD 	    	0x72
#define CBUS_SLAVE_ADDR     	0xC8
#define EDID_ROM_ADDR       	0xA0
#define EDID_SEG_ADDR	    	0x60

#define EDID_I2C_ADD		0x50



#define DEV_NAME "sil9024"
#define DEBUG_LEVEL 1
//#define DPRINTK(level,fmt,args...) do{ if(level < DEBUG_LEVEL)\
//            print(KERN_INFO "%s [%s ,%d]: " fmt "\n",DEV_NAME,__FUNCTION__,__LINE__,##args);\
//    }while(0)

#define DPRINTK(level,fmt,args...) APP_PRINT(fmt, ##args);

static unsigned int open_cnt = 0;
static unsigned int norm = 9;
//#include "siHdmiTx_902x_TPI.h"
#define TIMER_DELAY (HZ/10)

SIHDMITX_CONFIG	siHdmiTx;
GLOBAL_SYSTEM 		g_sys;
GLOBAL_HDCP 		g_hdcp;
GLOBAL_EDID 		g_edid;
byte 				tpivmode[3];  // saved TPI Reg0x08/Reg0x09/Reg0x0A values.

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////         The following functions are related with target system!!!    //////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//siHdmiTx_HwResetPin = P2^1; 	// Connected to 9022A/4A pin C3 (CRST#)

//------------------------------------
extern void DelayMS (word MS);
/*-----define micron
extern byte I2CReadByte ( byte SlaveAddr, byte RegAddr );
extern void I2CWriteByte ( byte SlaveAddr, byte RegAddr, byte Data );
-------------------------------------------------------------------*/
#define  I2CReadByte   gpio_i2c_read
#define  I2CWriteByte  gpio_i2c_write
/*----------------------------------------------------------------------------

extern byte I2CReadBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
extern byte I2CWriteBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
***************************************************************************/
//extern byte siiReadSegmentBlockEDID(byte SlaveAddr, byte Segment, byte Offset, byte *Buffer, byte Length);


const VModeInfoType VModesTable[] =
{
    //===================================================================================================================================================================================================================================
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals  Position     Active     Aspect   Int  Length          Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        1,  0, NSM},  2517,  {ProgrVNegHNeg,     6000, { 800,  525}}, {144, 35}, { 640, 480}, R_4,     {0,  96, 2, 33,  48,  16,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 0 - 1.       640  x 480p @ 60 VGA
    {{        2,  3, NSM},  2700,  {ProgrVNegHNeg,     6000, { 858,  525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  19,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 1 - 2,3      720  x 480p
    {{        4,  0, NSM},  7425,  {ProgrVPosHPos,     6000, {1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 2 - 4        1280 x 720p@60Hz
    {{        5,  0, NSM},  7425,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920, 1080}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 3 - 5        1920 x 1080i
    {{        6,  7, NSM},  2700,  {InterlaceVNegHNeg, 6000, {1716,  264}}, {119, 18}, { 720, 480}, R_4or16, {3,  62, 3, 15, 114,  17,   5,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 4 - 6,7      1440 x 480i,pix repl
    {{        8,  9,   1},  2700,  {ProgrVNegHNeg,     6000, {1716,  262}}, {119, 18}, {1440, 240}, R_4or16, {0, 124, 3, 15, 114,  38,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 5 - 8,9(1)   1440 x 240p
    {{        8,  9,   2},  2700,  {ProgrVNegHNeg,     6000, {1716,  263}}, {119, 18}, {1440, 240}, R_4or16, {0, 124, 3, 15, 114,  38,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 6 - 8,9(2)   1440 x 240p
    {{       10, 11, NSM},  5400,  {InterlaceVNegHNeg, 6000, {3432,  525}}, {238, 18}, {2880, 480}, R_4or16, {0, 248, 3, 15, 228,  76,   4, 1716},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 7 - 10,11    2880 x 480i
    {{       12, 13,   1},  5400,  {ProgrVNegHNeg,     6000, {3432,  262}}, {238, 18}, {2880, 240}, R_4or16, {0, 248, 3, 15, 228,  76,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 8 - 12,13(1) 2880 x 240p
    {{       12, 13,   2},  5400,  {ProgrVNegHNeg,     6000, {3432,  263}}, {238, 18}, {2880, 240}, R_4or16, {0, 248, 3, 15, 228,  76,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 9 - 12,13(2) 2880 x 240p
    {{       14, 15, NSM},  5400,  {ProgrVNegHNeg,     6000, {1716,  525}}, {244, 36}, {1440, 480}, R_4or16, {0, 124, 6, 30, 120,  32,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 10 - 14,15    1440 x 480p
    {{       16,  0, NSM}, 14835,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 11 - 16       1920 x 1080p
    {{       17, 18, NSM},  2700,  {ProgrVNegHNeg,     5000, { 864,  625}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 12 - 17,18    720  x 576p
    {{       19,  0, NSM},  7425,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 13 - 19       1280 x 720p@50Hz
    {{       20,  0, NSM},  7425,  {InterlaceVPosHPos, 5000, {2640, 1125}}, {192, 20}, {1920, 1080}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1320},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 14 - 20       1920 x 1080i
    {{       21, 22, NSM},  2700,  {InterlaceVNegHNeg, 5000, {1728,  625}}, {132, 22}, { 720, 576}, R_4,     {3,  63, 3, 19, 138,  24,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 15 - 21,22    1440 x 576i
    {{       23, 24,   1},  2700,  {ProgrVNegHNeg,     5000, {1728,  312}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 16 - 23,24(1) 1440 x 288p
    {{       23, 24,   2},  2700,  {ProgrVNegHNeg,     5000, {1728,  313}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 17 - 23,24(2) 1440 x 288p
    {{       23, 24,   3},  2700,  {ProgrVNegHNeg,     5000, {1728,  314}}, {132, 22}, {1440, 288}, R_4or16, {0, 126, 3, 19, 138,  24,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 18 - 23,24(3) 1440 x 288p
    {{       25, 26, NSM},  5400,  {InterlaceVNegHNeg, 5000, {3456,  625}}, {264, 22}, {2880, 576}, R_4or16, {0, 252, 3, 19, 276,  48,   2, 1728},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 19 - 25,26    2880 x 576i
    {{       27, 28,   1},  5400,  {ProgrVNegHNeg,     5000, {3456,  312}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   2,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 20 - 27,28(1) 2880 x 288p
    {{       27, 28,   2},  5400,  {ProgrVNegHNeg,     5000, {3456,  313}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   3,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 21 - 27,28(2) 2880 x 288p
    {{       27, 28,   3},  5400,  {ProgrVNegHNeg,     5000, {3456,  314}}, {264, 22}, {2880, 288}, R_4or16, {0, 252, 3, 19, 276,  48,   4,    0},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 22 - 27,28(3) 2880 x 288p
    {{       29, 30, NSM},  5400,  {ProgrVPosHNeg,     5000, {1728,  625}}, {264, 44}, {1440, 576}, R_4or16, {0, 128, 5, 39, 136,  24,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 23 - 29,30    1440 x 576p
    {{       31,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 24 - 31(1)    1920 x 1080p
    {{       32,  0, NSM},  7417,  {ProgrVPosHPos,     2400, {2750, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 638,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 25 - 32(2)    1920 x 1080p@24Hz
    {{       33,  0, NSM},  7425,  {ProgrVPosHPos,     2500, {2640, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 26 - 33(3)    1920 x 1080p
    {{       34,  0, NSM},  7417,  {ProgrVPosHPos,     3000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 27 - 34(4)    1920 x 1080p
    {{       35, 36, NSM}, 10800,  {ProgrVNegHNeg,     5994, {3432,  525}}, {488, 36}, {2880, 480}, R_4or16, {0, 248, 6, 30, 240,  64,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 28 - 35, 36   2880 x 480p@59.94/60Hz
    {{       37, 38, NSM}, 10800,  {ProgrVNegHNeg,     5000, {3456,  625}}, {272, 39}, {2880, 576}, R_4or16, {0, 256, 5, 40, 272,  48,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 29 - 37, 38   2880 x 576p@50Hz
    {{       39,  0, NSM},  7200,  {InterlaceVNegHNeg, 5000, {2304, 1250}}, {352, 62}, {1920, 1080}, R_16,    {0, 168, 5, 87, 184,  32,  24,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 30 - 39       1920 x 1080i@50Hz
    {{       40,  0, NSM}, 14850,  {InterlaceVPosHPos, 10000, {2640, 1125}}, {192, 20}, {1920, 1080}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1320},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 31 - 40       1920 x 1080i@100Hz
    {{       41,  0, NSM}, 14850,  {InterlaceVPosHPos, 10000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 400,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 32 - 41       1280 x 720p@100Hz
    {{       42, 43, NSM},  5400,  {ProgrVNegHNeg,     10000, { 864,  144}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 33 - 42, 43,  720p x 576p@100Hz
    {{       44, 45, NSM},  5400,  {InterlaceVNegHNeg, 10000, { 864,  625}}, {132, 22}, { 720, 576}, R_4or16, {0,  63, 3, 19,  69,  12,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 34 - 44, 45,  720p x 576i@100Hz, pix repl
    {{       46,  0, NSM}, 14835,  {InterlaceVPosHPos, 11988, {2200, 1125}}, {192, 20}, {1920, 1080}, R_16,    {0,  44, 5, 15, 149,  88,   2, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 35 - 46,      1920 x 1080i@119.88/120Hz
    {{       47,  0, NSM}, 14835,  {ProgrVPosHPos,     11988, {1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5, 1100},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 36 - 47,      1280 x 720p@119.88/120Hz
    {{       48, 49, NSM},  5400,  {ProgrVNegHNeg,     11988, { 858,  525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,  10,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 37 - 48, 49   720  x 480p@119.88/120Hz
    {{       50, 51, NSM},  5400,  {InterlaceVNegHNeg, 11988, { 858,  525}}, {119, 18}, { 720, 480}, R_4or16, {0,  62, 3, 15,  57,  19,   4,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 38 - 50, 51   720  x 480i@119.88/120Hz
    {{       52, 53, NSM}, 10800,  {ProgrVNegHNeg,     20000, { 864,  625}}, {132, 44}, { 720, 576}, R_4or16, {0,  64, 5, 39,  68,  12,   5,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 39 - 52, 53,  720  x 576p@200Hz
    {{       54, 55, NSM}, 10800,  {InterlaceVNegHNeg, 20000, { 864,  625}}, {132, 22}, { 720, 576}, R_4or16, {0,  63, 3, 19,  69,  12,   2,  432},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 40 - 54, 55,  1440 x 576i @200Hz, pix repl
    {{       56, 57, NSM}, 10800,  {ProgrVNegHNeg,     24000, { 858,  525}}, {122, 42}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,   9,    0},    0,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 41 - 56, 57,  720  x 480p @239.76/240Hz
    {{       58, 59, NSM}, 10800,  {InterlaceVNegHNeg, 24000, { 858,  525}}, {119, 18}, { 720, 480}, R_4or16, {0,  62, 3, 15,  57,  19,   4,  429},    1,    {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 42 - 58, 59,  1440 x 480i @239.76/240Hz, pix repl

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 4K x 2K Modes:
    //===================================================================================================================================================================================================================================
    //                                                                                                            Pulse
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals   Position     Active    Aspect   Int  Width           Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        1,  0, NSM}, 297000, {ProgrVNegHNeg,     30000, {4400, 2250}}, {384, 82}, {3840, 2160}, R_16,    {0,  88, 10, 72, 296, 176,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 43 - 4k x 2k 29.97/30Hz (297.000 MHz)
    {{        2,  0, NSM}, 297000, {ProgrVNegHNeg,     29700, {5280, 2250}}, {384, 82}, {3840, 2160}, R_16,    {0,  88, 10, 72, 296, 1056,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 44 - 4k x 2k 25Hz
    {{        3,  0, NSM}, 297000, {ProgrVNegHNeg,     24000, {5500, 2250}}, {384, 82}, {3840, 2160}, R_16,    {0,  88, 10, 72, 296, 1276,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 45 - 4k x 2k 24Hz (297.000 MHz)
    {{        4, 0, NSM}, 297000, {ProgrVNegHNeg,     24000, {6500, 2250}}, {384, 82}, {4096, 2160}, R_16,    {0,  88, 10, 72, 296, 1020,  8,    0},   0,     {0,     0,     0,     0,    0},    NO_3D_SUPPORT}, // 46 - 4k x 2k 24Hz (SMPTE)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 3D Modes:
    //===================================================================================================================================================================================================================================
    //                                                                                                            Pulse
    //         VIC                  Refresh type Refresh-Rate Pixel-Totals  Position      Active    Aspect   Int  Width           Hbit  Vbit  Field  Pixel          Vact Space/Blank
    //        1   2  SubM   PixClk  V/H Position       VFreq   H      V      H    V       H    V    Ratio    Adj  H   V  Top  Dly HSync VSync Offset Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{        2,  3, NSM},  2700,  {ProgrVPosHPos,     6000, {858,   525}}, {122, 36}, { 720, 480}, R_4or16, {0,  62, 6, 30,  60,  16,   9,    0},    0,    {0,     0,     0,     0,    0},     8}, // 47 - 3D, 2,3 720p x 480p /60Hz, Side-by-Side (Half)
    {{        4,  0, NSM}, 14850,  {ProgrVPosHPos,     6000, {1650,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 110,   5,    0},    0,    {0,     0,     0,     0,    0},     0}, // 48 - 3D  4   1280 x 720p@60Hz,  Frame Packing
    {{        5,  0, NSM}, 14850,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {23,   22,     0,     0,    0},     0}, // 49 - 3D, 5,  1920 x 1080i/60Hz, Frame Packing
    {{        5,  0, NSM}, 14850,  {InterlaceVPosHPos, 6000, {2200,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148,  88,   2, 1100},    0,    {0,     0,    22,    22,   23},     1}, // 50 - 3D, 5,  1920 x 1080i/60Hz, Field Alternative
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 51 - 3D, 16, 1920 x 1080p/60Hz, Frame Packing
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     2}, // 52 - 3D, 16, 1920 x 1080p/60Hz, Line Alternative
    {{       16,  0, NSM}, 29700,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     3}, // 53 - 3D, 16, 1920 x 1080p/60Hz, Side-by-Side (Full)
    {{       16,  0, NSM}, 14850,  {ProgrVPosHPos,     6000, {2200, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148,  88,   4,    0},    0,    {0,     0,     0,     0,    0},     8}, // 54 - 3D, 16, 1920 x 1080p/60Hz, Side-by-Side (Half)
    {{       19,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     0}, // 55 - 3D, 19, 1280 x 720p@50Hz,  Frame Packing
    {{       19,  0, NSM}, 14850,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     4}, // 56 - 3D, 19, 1280 x 720p/50Hz,  (L + depth)
    {{       19,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {1980,  750}}, {260, 25}, {1280, 720}, R_16,    {0,  40, 5, 20, 220, 440,   5,    0},    0,    {0,     0,     0,     0,    0},     5}, // 57 - 3D, 19, 1280 x 720p/50Hz,  (L + depth + Gfx + G-depth)
    {{       20,  0, NSM}, 14850,  {InterlaceVPosHPos, 5000, {2640,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1220},    0,    {23,   22,     0,     0,    0},     0}, // 58 - 3D, 20, 1920 x 1080i/50Hz, Frame Packing
    {{       20,  0, NSM}, 14850,  {InterlaceVPosHPos, 5000, {2640,  562}}, {192, 20}, {1920, 540}, R_16,    {0,  44, 5, 15, 148, 528,   2, 1220},    0,    {0,     0,    22,    22,   23},     1}, // 59 - 3D, 20, 1920 x 1080i/50Hz, Field Alternative
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 60 - 3D, 31, 1920 x 1080p/50Hz, Frame Packing
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2640, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     2}, // 61 - 3D, 31, 1920 x 1080p/50Hz, Line Alternative
    {{       31,  0, NSM}, 29700,  {ProgrVPosHPos,     5000, {2650, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 528,   4,    0},    0,    {0,     0,     0,     0,    0},     3}, // 62 - 3D, 31, 1920 x 1080p/50Hz, Side-by-Side (Full)
    {{       32,  0, NSM}, 14850,  {ProgrVPosHPos,     2400, {2750, 1125}}, {192, 41}, {1920, 1080}, R_16,    {0,  44, 5, 36, 148, 638,   4,    0},    0,    {0,     0,     0,     0,    0},     0}, // 63 - 3D, 32, 1920 x 1080p@24Hz, Frame Packing

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// NOTE: DO NOT ATTEMPT INPUT RESOLUTIONS THAT REQUIRE PIXEL CLOCK FREQUENCIES HIGHER THAN THOSE SUPPORTED BY THE TRANSMITTER CHIP

    //===================================================================================================================================================================================================================================
    //                                                                                                              Sync Pulse
    //  VIC                          Refresh type   fresh-Rate  Pixel-Totals    Position    Active     Aspect   Int  Width            Hbit  Vbit  Field  Pixel          Vact Space/Blank
    // 1   2  SubM         PixClk    V/H Position       VFreq   H      V        H    V       H    V     Ratio   {Adj  H   V  Top  Dly HSync VSync Offset} Repl  Space1 Space2 Blank1 Blank2 Blank3  3D
    //===================================================================================================================================================================================================================================
    {{PC_BASE, 0, NSM},    3150,   {ProgrVNegHPos,     8508,   {832, 445}},    {160, 63},   {640, 350},   R_16,  {0,  64,  3,  60,  96,  32,  32,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 64 - 640x350@85.08
    {{PC_BASE + 1, 0, NSM},    3150,   {ProgrVPosHNeg,     8508,   {832, 445}},    {160, 44},   {640, 400},   R_16,  {0,  64,  3,  41,  96,  32,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 65 - 640x400@85.08
    {{PC_BASE + 2, 0, NSM},    2700,   {ProgrVPosHNeg,     7008,   {900, 449}},    {0, 0},      {720, 400},   R_16,  {0,   0,  0,   0,   0,   0,   0,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 66 - 720x400@70.08
    {{PC_BASE + 3, 0, NSM},    3500,   {ProgrVPosHNeg,     8504,   {936, 446}},    {20, 45},    {720, 400},   R_16,  {0,  72,  3,  42, 108,  36,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 67 - 720x400@85.04
    {{PC_BASE + 4, 0, NSM},    2517,   {ProgrVNegHNeg,     5994,   {800, 525}},    {144, 35},   {640, 480},   R_4,   {0,  96,  2,  33,  48,  16,  10,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 68 - 640x480@59.94
    {{PC_BASE + 5, 0, NSM},    3150,   {ProgrVNegHNeg,     7281,   {832, 520}},    {144, 31},   {640, 480},   R_4,   {0,  40,  3,  28, 128, 128,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 69 - 640x480@72.80
    {{PC_BASE + 6, 0, NSM},    3150,   {ProgrVNegHNeg,     7500,   {840, 500}},    {21, 19},    {640, 480},   R_4,   {0,  64,  3,  28, 128,  24,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 70 - 640x480@75.00
    {{PC_BASE + 7, 0, NSM},     3600,   {ProgrVNegHNeg,     8500,   {832, 509}},    {168, 28},   {640, 480},   R_4,   {0,  56,  3,  25, 128,  24,   9,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 71 - 640x480@85.00
    {{PC_BASE + 8, 0, NSM},     3600,   {ProgrVPosHPos,     5625,   {1024, 625}},   {200, 24},   {800, 600},   R_4,   {0,  72,  2,  22, 128,  24,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 72 - 800x600@56.25
    {{PC_BASE + 9, 0, NSM},     4000,   {ProgrVPosHPos,     6032,   {1056, 628}},   {216, 27},   {800, 600},   R_4,   {0, 128,  4,  23,  88,  40,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 73 - 800x600@60.317
    {{PC_BASE + 10, 0, NSM},    5000,   {ProgrVPosHPos,     7219,   {1040, 666}},   {184, 29},   {800, 600},   R_4,   {0, 120,  6,  23,  64,  56,  37,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 74 - 800x600@72.19
    {{PC_BASE + 11, 0, NSM},    4950,   {ProgrVPosHPos,     7500,   {1056, 625}},   {240, 24},   {800, 600},   R_4,   {0,  80,  3,  21, 160,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 75 - 800x600@75
    {{PC_BASE + 12, 0, NSM},    5625,   {ProgrVPosHPos,     8506,   {1048, 631}},   {216, 30},   {800, 600},   R_4,   {0,  64,  3,  27, 152,  32,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 76 - 800x600@85.06
    {{PC_BASE + 13, 0, NSM},    6500,   {ProgrVNegHNeg,     6000,   {1344, 806}},   {296, 35},   {1024, 768},  R_4,   {0, 136,  6,  29, 160,  24,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 77 - 1024x768@60
    {{PC_BASE + 14, 0, NSM},    7500,   {ProgrVNegHNeg,     7007,   {1328, 806}},   {280, 35},   {1024, 768},  R_4,   {0, 136,  6,  19, 144,  24,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 78 - 1024x768@70.07
    {{PC_BASE + 15, 0, NSM},    7875,   {ProgrVPosHPos,     7503,   {1312, 800}},   {272, 31},   {1024, 768},  R_4,   {0,  96,  3,  28, 176,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 79 - 1024x768@75.03
    {{PC_BASE + 16, 0, NSM},    9450,   {ProgrVPosHPos,     8500,   {1376, 808}},   {304, 39},   {1024, 768},  R_4,   {0,  96,  3,  36, 208,  48,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 80 - 1024x768@85
    {{PC_BASE + 17, 0, NSM},   10800,   {ProgrVPosHPos,     7500,   {1600, 900}},   {384, 35},   {1152, 864},  R_4,   {0, 128,  3,  32, 256,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 81 - 1152x864@75
    {{PC_BASE + 18, 0, NSM},   16200,   {ProgrVPosHPos,     6000,   {2160, 1250}},  {496, 49},   {1600, 1200}, R_4,   {0, 304,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 82 - 1600x1200@60
    {{PC_BASE + 19, 0, NSM},    6825,   {ProgrVNegHPos,     6000,   {1440, 790}},   {112, 19},   {1280, 768},  R_16,  {0,  32,  7,  12,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 83 - 1280x768@59.95
    {{PC_BASE + 20, 0, NSM},    7950,   {ProgrVPosHNeg,     5987,   {1664, 798}},   {320, 27},   {1280, 768},  R_16,  {0, 128,  7,  20, 192,  64,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 84 - 1280x768@59.87
    {{PC_BASE + 21, 0, NSM},   10220,   {ProgrVPosHNeg,     6029,   {1696, 805}},   {320, 27},   {1280, 768},  R_16,  {0, 128,  7,  27, 208,  80,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 85 - 1280x768@74.89
    {{PC_BASE + 22, 0, NSM},   11750,   {ProgrVPosHNeg,     8484,   {1712, 809}},   {352, 38},   {1280, 768},  R_16,  {0, 136,  7,  31, 216,  80,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 86 - 1280x768@85
    {{PC_BASE + 23, 0, NSM},   10800,   {ProgrVPosHPos,     6000,   {1800, 1000}},  {424, 39},   {1280, 960},  R_4,   {0, 112,  3,  36, 312,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 87 - 1280x960@60
    {{PC_BASE + 24, 0, NSM},   14850,   {ProgrVPosHPos,     8500,   {1728, 1011}},  {384, 50},   {1280, 960},  R_4,   {0, 160,  3,  47, 224,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 88 - 1280x960@85
    {{PC_BASE + 25, 0, NSM},   10800,   {ProgrVPosHPos,     6002,   {1688, 1066}},  {360, 41},   {1280, 1024}, R_4,   {0, 112,  3,  38, 248,  48,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 89 - 1280x1024@60
    {{PC_BASE + 26, 0, NSM},   13500,   {ProgrVPosHPos,     7502,   {1688, 1066}},  {392, 41},   {1280, 1024}, R_4,   {0, 144,  3,  38, 248,  16,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 90 - 1280x1024@75
    {{PC_BASE + 27, 0, NSM},   15750,   {ProgrVPosHPos,     8502,   {1728, 1072}},  {384, 47},   {1280, 1024}, R_4,   {0, 160,  3,   4, 224,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 91 - 1280x1024@85
    {{PC_BASE + 28, 0, NSM},    8550,   {ProgrVPosHPos,     6002,   {1792, 795}},   {368, 24},   {1360, 768},  R_16,  {0, 112,  6,  18, 256,  64,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 92 - 1360x768@60
    {{PC_BASE + 29, 0, NSM},   10100,   {ProgrVNegHPos,     5995,   {1560, 1080}},  {112, 27},   {1400, 1050}, R_4,   {0,  32,  4,  23,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 93 - 1400x105@59.95
    {{PC_BASE + 30, 0, NSM},   12175,   {ProgrVPosHNeg,     5998,   {1864, 1089}},  {376, 36},   {1400, 1050}, R_4,   {0, 144,  4,  32, 232,  88,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 94 - 1400x105@59.98
    {{PC_BASE + 31, 0, NSM},   15600,   {ProgrVPosHNeg,     7487,   {1896, 1099}},  {392, 46},   {1400, 1050}, R_4,   {0, 144,  4,  22, 248, 104,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 95 - 1400x105@74.87
    {{PC_BASE + 32, 0, NSM},   17950,   {ProgrVPosHNeg,     8496,   {1912, 1105}},  {408, 52},   {1400, 1050}, R_4,   {0, 152,  4,  48, 256, 104,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 96 - 1400x105@84.96
    {{PC_BASE + 33, 0, NSM},   17550,   {ProgrVPosHPos,     6500,   {2160, 1250}},  {496, 49},   {1600, 1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 97 - 1600x1200@65
    {{PC_BASE + 34, 0, NSM},   18900,   {ProgrVPosHPos,     7000,   {2160, 1250}},  {496, 49},   {1600, 1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 98 - 1600x1200@70
    {{PC_BASE + 35, 0, NSM},   20250,   {ProgrVPosHPos,     7500,   {2160, 1250}},  {496, 49},   {1600, 1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 99 - 1600x1200@75
    {{PC_BASE + 36, 0, NSM},   22950,   {ProgrVPosHPos,     8500,   {2160, 1250}},  {496, 49},   {1600, 1200}, R_4,   {0, 192,  3,  46, 304,  64,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 100 - 1600x1200@85
    {{PC_BASE + 37, 0, NSM},   20475,   {ProgrVPosHNeg,     6000,   {2448, 1394}},  {528, 49},   {1792, 1344}, R_4,   {0, 200,  3,  46, 328, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 101 - 1792x1344@60
    {{PC_BASE + 38, 0, NSM},   26100,   {ProgrVPosHNeg,     7500,   {2456, 1417}},  {568, 72},   {1792, 1344}, R_4,   {0, 216,  3,  69, 352,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 102 - 1792x1344@74.997
    {{PC_BASE + 39, 0, NSM},   21825,   {ProgrVPosHNeg,     6000,   {2528, 1439}},  {576, 46},   {1856, 1392}, R_4,   {0, 224,  3,  43, 352,  96,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 103 - 1856x1392@60
    {{PC_BASE + 40, 0, NSM},   28800,   {ProgrVPosHNeg,     7500,   {2560, 1500}},  {576, 107},  {1856, 1392}, R_4,   {0, 224,  3, 104, 352, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 104 - 1856x1392@75
    {{PC_BASE + 41, 0, NSM},   15400,   {ProgrVNegHPos,     5995,   {2080, 1235}},  {112, 32},   {1920, 1200}, R_16,  {0,  32,  6,  26,  80,  48,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 106 - 1920x1200@59.95
    {{PC_BASE + 42, 0, NSM},   19325,   {ProgrVPosHNeg,     5988,   {2592, 1245}},  {536, 42},   {1920, 1200}, R_16,  {0, 200,  6,  36, 336, 136,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 107 - 1920x1200@59.88
    {{PC_BASE + 43, 0, NSM},   24525,   {ProgrVPosHNeg,     7493,   {2608, 1255}},  {552, 52},   {1920, 1200}, R_16,  {0, 208,  6,  46, 344, 136,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 108 - 1920x1200@74.93
    {{PC_BASE + 44, 0, NSM},   28125,   {ProgrVPosHNeg,     8493,   {2624, 1262}},  {560, 59},   {1920, 1200}, R_16,  {0, 208,  6,  53, 352, 144,   3,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 109 - 1920x1200@84.93
    {{PC_BASE + 45, 0, NSM},   23400,   {ProgrVPosHNeg,     6000,   {2600, 1500}},  {552, 59},   {1920, 1440}, R_4,   {0, 208,  3,  56, 344, 128,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 110 - 1920x1440@60
    {{PC_BASE + 46, 0, NSM},   29700,   {ProgrVPosHNeg,     7500,   {2640, 1500}},  {576, 59},   {1920, 1440}, R_4,   {0, 224,  3,  56, 352, 144,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 111 - 1920x1440@75
    {{PC_BASE + 47, 0, NSM},   24150,   {ProgrVPosHNeg,     6000,   {2720, 1481}},  {48,  3},   {2560, 1440}, R_16,  {0,  32,  5,  56, 352, 144,   1,       0},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 112 - 2560x1440@60 // %%% need work
    {{PC_BASE + 48, 0, NSM},    2700,   {InterlaceVNegHNeg, 6000,   {1716,  264}},  {244, 18},   {1440, 480}, R_4or16, {3, 124,  3,  15, 114,  17,   5,     429},  0,  {0,     0,     0,     0,    0},   NO_3D_SUPPORT}, // 113 - 1440 x 480i
};

//------------------------------------------------------------------------------
// Aspect Ratio table defines the aspect ratio as function of VIC. This table
// should be used in conjunction with the 861-D part of VModeInfoType VModesTable[]
// (formats 0 - 59) because some formats that differ only in their AR are grouped
// together (e.g., formats 2 and 3).
//------------------------------------------------------------------------------
const byte AspectRatioTable[] =
{
    R_4,  R_4, R_16, R_16, R_16,  R_4, R_16,  R_4, R_16,  R_4,
    R_16,  R_4, R_16,  R_4, R_16, R_16,  R_4, R_16, R_16, R_16,
    R_4, R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16,
    R_16, R_16, R_16, R_16,  R_4, R_16,  R_4, R_16, R_16, R_16,
    R_16,  R_4, R_16,  R_4, R_16, R_16, R_16,  R_4, R_16,  R_4,
    R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16,  R_4, R_16
};

//------------------------------------------------------------------------------
// VIC to Indexc table defines which VideoModeTable entry is appropreate for this VIC code.
// Note: This table is valid ONLY for VIC codes in 861-D formats, NOT for HDMI_VIC codes
// or 3D codes!
//------------------------------------------------------------------------------
const byte VIC2Index[] =
{
    0,  0,  1,  1,  2,  3,  4,  4,  5,  5,
    7,  7,  8,  8, 10, 10, 11, 12, 12, 13,
    14, 15, 15, 16, 16, 19, 19, 20, 20, 23,
    23, 24, 25, 26, 27, 28, 28, 29, 29, 30,
    31, 32, 33, 33, 34, 34, 35, 36, 37, 37,
    38, 38, 39, 39, 40, 40, 41, 41, 42, 42
};

static volatile bool gI2C1TxCplt = false;
static volatile bool gI2C1RxCplt = false;
void sci_i2c_master_callback(i2c_master_callback_args_t *p_args)
{
    if(I2C_MASTER_EVENT_RX_COMPLETE == p_args->event)
    {
        gI2C1RxCplt = true;
    }
    else if(I2C_MASTER_EVENT_TX_COMPLETE == p_args->event)
    {
        gI2C1TxCplt = true;
    }
}
static void I2C1WaitTxCplt(void)
{
    uint16_t wTimeout = 50;
    while(!gI2C1TxCplt && wTimeout)
    {
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        wTimeout--;
    }
    gI2C1TxCplt = false;
}
static void I2C1WaitRxCplt(void)
{
    uint16_t wTimeout = 50;
    while(!gI2C1RxCplt && wTimeout)
    {
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        wTimeout--;
    }
    gI2C1RxCplt = false;
}

static volatile bool gI2C0TxCplt = false;
static volatile bool gI2C0RxCplt = false;
void sci_i2c_master0_callback(i2c_master_callback_args_t *p_args)
{
    if(I2C_MASTER_EVENT_RX_COMPLETE == p_args->event)
    {
        gI2C0RxCplt = true;
    }
    else if(I2C_MASTER_EVENT_TX_COMPLETE == p_args->event)
    {
        gI2C0TxCplt = true;
    }
}
static void I2C0WaitTxCplt(void)
{
    uint16_t wTimeout = 50;
    while(!gI2C0TxCplt && wTimeout)
    {
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        wTimeout--;
    }
    gI2C0TxCplt = false;
}
static void I2C0WaitRxCplt(void)
{
    uint16_t wTimeout = 50;
    while(!gI2C0RxCplt && wTimeout)
    {
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        wTimeout--;
    }
    gI2C0RxCplt = false;
}



//------------------------------------------------------------------------------
// Function Name: DelayMS()
// Function Description: Introduce a busy-wait delay equal, in milliseconds, to the input parameter.
//
// Accepts: Length of required delay in milliseconds (max. 65535 ms)
//------------------------------------------------------------------------------
void DelayMS (word MS)
{
    // HAL_Delay(MS);
    R_BSP_SoftwareDelay (MS, BSP_DELAY_UNITS_MILLISECONDS);
    /*
    dword i;
    i = MS * 100;

    while (i)
    {
        i--;
    }
    */
}
byte ReadBlockEDID( byte RegAddr, byte NBytes, byte * Data )
{
//       HAL_StatusTypeDef status = HAL_OK;
//       status =  HAL_I2C_Mem_Read(&hi2c4, SlaveAddr, RegAddr, I2C_MEMADD_SIZE_8BIT, Data, NBytes, 300000);
// //   status = HAL_I2C_Mem_Write(&I2C_Handle, OV5640_DEVICE_ADDRESS, (uint16_t)Addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)&Data, 1, 1000);
  
//   /* Check the communication status */
//   if(status != HAL_OK)
//   {
//     /* Re-Initiaize the I2C Bus */
//       APP_PRINT("I2CReadBlock ERROR-----------%d\r\n",status);
//   }
//   return status;
    /*
    int i ;
    for(i=0;i<NBytes;i++)
    	{
    		HAL_I2C_Mem_Read(

    	   Data[i]=I2CReadByte(SlaveAddr,RegAddr+i);
    	}
    return 0;
     */
    g_i2c_master1.p_api->slaveAddressSet(g_i2c_master1.p_ctrl, 0x50, 1);
    g_i2c_master1.p_api->write(g_i2c_master1.p_ctrl, (unsigned char*)&RegAddr, (unsigned int)1, true);
    I2C1WaitTxCplt();
    // R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MICROSECONDS);
    /* 读取该地址的一个字节数据 */
    g_i2c_master1.p_api->read(g_i2c_master1.p_ctrl, (unsigned char*)Data, (unsigned int)NBytes, false);
    I2C1WaitRxCplt();
    return 0;
}

byte I2CReadBlock( byte RegAddr, byte NBytes, byte * Data )
{
//       HAL_StatusTypeDef status = HAL_OK;
//       status =  HAL_I2C_Mem_Read(&hi2c4, SlaveAddr, RegAddr, I2C_MEMADD_SIZE_8BIT, Data, NBytes, 300000);
// //   status = HAL_I2C_Mem_Write(&I2C_Handle, OV5640_DEVICE_ADDRESS, (uint16_t)Addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)&Data, 1, 1000);
  
//   /* Check the communication status */
//   if(status != HAL_OK)
//   {
//     /* Re-Initiaize the I2C Bus */
//       APP_PRINT("I2CReadBlock ERROR-----------%d\r\n",status);
//   }
//   return status;
    /*
    int i ;
    for(i=0;i<NBytes;i++)
    	{
    		HAL_I2C_Mem_Read(

    	   Data[i]=I2CReadByte(SlaveAddr,RegAddr+i);
    	}
    return 0;
     */
    g_i2c_master1.p_api->slaveAddressSet(g_i2c_master1.p_ctrl, 0x39, 1);
    g_i2c_master1.p_api->write(g_i2c_master1.p_ctrl, (unsigned char*)&RegAddr, (unsigned int)1, true);
    I2C1WaitTxCplt();
    // R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MICROSECONDS);
    /* 读取该地址的一个字节数据 */
    g_i2c_master1.p_api->read(g_i2c_master1.p_ctrl, (unsigned char*)Data, (unsigned int)NBytes, false);
    I2C1WaitRxCplt();
    return 0;
}
byte I2CWriteBlock( byte RegAddr, byte NBytes, byte * Data )
{
//       HAL_StatusTypeDef status = HAL_OK;
//       status = HAL_I2C_Mem_Write(&hi2c4, SlaveAddr, RegAddr, I2C_MEMADD_SIZE_8BIT, Data, NBytes, 300000);
//   if(status != HAL_OK)
//   {
//     /* Re-Initiaize the I2C Bus */
//       APP_PRINT("I2CWriteBlock ERROR-----------%d\r\n",status);
//   }
//   return status;
    unsigned char data[512] = {0};
    data[0] = RegAddr;
    for(unsigned char i=0; i<NBytes; i++)
    {
        data[i+1] = Data[i];
    }
    g_i2c_master1.p_api->slaveAddressSet(g_i2c_master1.p_ctrl, 0x39, 1);
    g_i2c_master1.p_api->write(g_i2c_master1.p_ctrl, (unsigned char*)data, (unsigned int)(NBytes+1), false);
    I2C1WaitTxCplt();
    return 0;
    /*
    int i ;
    for(i=0;i<NBytes;i++)
    {
       I2CWriteByte(SlaveAddr,RegAddr+i,Data[i]);
    }
    return 0;
    */

}
//------------------------------------------------------------------------------
// Function Name: ReadByteTPI()
// Function Description: I2C read
//------------------------------------------------------------------------------
byte ReadByteTPI (byte RegOffset)
{
    byte  Readnum;
    I2CReadBlock(RegOffset, 1, &Readnum);
    //APP_PRINT("number:%d\n", Readnum);
    return Readnum;

    //return I2CReadByte(TX_SLAVE_ADDR, RegOffset);
}

//------------------------------------------------------------------------------
// Function Name: WriteByteTPI()
// Function Description: I2C write
//------------------------------------------------------------------------------
void WriteByteTPI (byte RegOffset, byte Data)
{
    I2CWriteBlock( RegOffset, 1, &Data );
    //I2CWriteByte(TX_SLAVE_ADDR, RegOffset, Data);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////              END!!!            /////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "1" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadSetWriteTPI (byte Offset, byte Pattern)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);

    Tmp |= Pattern;
    WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "0" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Pattern"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadClearWriteTPI (byte Offset, byte Pattern)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);

    Tmp &= ~Pattern;
    WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadSetWriteTPI()
// Function Description: Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteTPI (byte Offset, byte Mask, byte Value)
{
    byte Tmp;

    Tmp = ReadByteTPI(Offset);
    Tmp &= ~Mask;
    Tmp |= (Value & Mask);
    WriteByteTPI(Offset, Tmp);
}

//------------------------------------------------------------------------------
// Function Name: ReadBlockTPI()
// Function Description: Read NBytes from offset Addr of the TPI slave address
//                      into a byte Buffer pointed to by Data
//------------------------------------------------------------------------------
void ReadBlockTPI (byte TPI_Offset, word NBytes, byte * pData)
{
    I2CReadBlock( TPI_Offset, NBytes, pData);
}

//------------------------------------------------------------------------------
// Function Name: WriteBlockTPI()
// Function Description: Write NBytes from a byte Buffer pointed to by Data to
//                      the TPI I2C slave starting at offset Addr
//------------------------------------------------------------------------------
void WriteBlockTPI (byte TPI_Offset, word NBytes, byte * pData)
{
    I2CWriteBlock( TPI_Offset, NBytes, pData);
}

//------------------------------------------------------------------------------
// Function Name: ReadIndexedRegister()
// Function Description: Read an indexed register value
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//
//                  Read:
//                      3. 0xBE => Returns the indexed register value
//------------------------------------------------------------------------------
byte ReadIndexedRegister(byte PageNum, byte RegOffset)
{
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);		// Internal page
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);	// Indexed register
    return ReadByteTPI(TPI_INDEXED_VALUE_REG); 		// Return read value
}

//------------------------------------------------------------------------------
// Function Name: WriteIndexedRegister()
// Function Description: Write a value to an indexed register
//
//                  Write:
//                      1. 0xBC => Internal page num
//                      2. 0xBD => Indexed register offset
//                      3. 0xBE => Set the indexed register value
//------------------------------------------------------------------------------
void WriteIndexedRegister (byte PageNum, byte RegOffset, byte RegValue)
{
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);  // Internal page
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);  // Indexed register
    WriteByteTPI(TPI_INDEXED_VALUE_REG, RegValue);    // Read value into buffer
}

//------------------------------------------------------------------------------
// Function Name: ReadModifyWriteIndexedRegister()
// Function Description: Write "Value" to all bits in TPI offset "Offset" that are set
//                  to "1" in "Mask"; Leave all other bits in "Offset"
//                  unchanged.
//------------------------------------------------------------------------------
void ReadModifyWriteIndexedRegister(byte PageNum, byte RegOffset, byte Mask, byte Value)
{
    byte Tmp;

    WriteByteTPI(TPI_INTERNAL_PAGE_REG, PageNum);
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, RegOffset);
    Tmp = ReadByteTPI(TPI_INDEXED_VALUE_REG);

    Tmp &= ~Mask;
    Tmp |= (Value & Mask);

    WriteByteTPI(TPI_INDEXED_VALUE_REG, Tmp);
}

//------------------------------------------------------------------------------
void TXHAL_InitPostReset (void)
{
    // Set terminations to default.
    WriteByteTPI(TMDS_CONT_REG, 0x25);
    // HW debounce to 64ms (0x14)
    WriteByteTPI(0x7C, 0x14);
}

//------------------------------------------------------------------------------
// Function Name: TxHW_Reset()
// Function Description: Hardware reset Tx
//------------------------------------------------------------------------------
void TxHW_Reset (void)
{
    APP_PRINT(">>TxHW_Reset()\r\n");

//	siHdmiTx_HwResetPin = LOW;
    DelayMS(TX_HW_RESET_PERIOD);
//	siHdmiTx_HwResetPin = HIGH;

    TXHAL_InitPostReset();
}

//------------------------------------------------------------------------------
// Function Name: InitializeStateVariables()
// Function Description: Initialize system state variables
//------------------------------------------------------------------------------
void InitializeStateVariables (void)
{
    g_sys.tmdsPoweredUp = FALSE;
    g_sys.hdmiCableConnected = FALSE;
    g_sys.dsRxPoweredUp = FALSE;

    #ifdef DEV_SUPPORT_EDID
    g_edid.edidDataValid = FALSE;
    #endif
}

//------------------------------------------------------------------------------
// Function Name: EnableTMDS()
// Function Description: Enable TMDS
//------------------------------------------------------------------------------
void EnableTMDS (void)
{
    APP_PRINT("TMDS -> Enabled\r\n");
    ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK, TMDS_OUTPUT_CONTROL_ACTIVE);
    WriteByteTPI(TPI_PIX_REPETITION, tpivmode[0]);      		// Write register 0x08
    g_sys.tmdsPoweredUp = TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DisableTMDS()
// Function Description: Disable TMDS
//------------------------------------------------------------------------------
void DisableTMDS (void)
{
    APP_PRINT("TMDS -> Disabled\r\n");
    ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK,
                       TMDS_OUTPUT_CONTROL_POWER_DOWN | AV_MUTE_MUTED);
    g_sys.tmdsPoweredUp = FALSE;
}

//------------------------------------------------------------------------------
// Function Name: EnableInterrupts()
// Function Description: Enable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  set in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte EnableInterrupts(byte Interrupt_Pattern)
{
    APP_PRINT(">>EnableInterrupts()\r\n");
    ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DisableInterrupts()
// Function Description: Disable the interrupts specified in the input parameter
//
// Accepts: A bit pattern with "1" for each interrupt that needs to be
//                  cleared in the Interrupt Enable Register (TPI offset 0x3C)
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte DisableInterrupts (byte Interrupt_Pattern)
{
    APP_PRINT(">>DisableInterrupts()\r\n");
    ReadClearWriteTPI(TPI_INTERRUPT_ENABLE_REG, Interrupt_Pattern);

    return TRUE;
}



#ifdef DEV_SUPPORT_EDID
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////                   EDID                 ///////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

byte g_CommData [EDID_BLOCK_SIZE];

// #define ReadBlockEDID(a,b,c)            		I2CReadBlock(EDID_ROM_ADDR, a, b, c)
#define ReadSegmentBlockEDID(a,b,c,d)   	  //siiReadSegmentBlockEDID(EDID_ROM_ADDR, a, b, d, c)

//------------------------------------------------------------------------------
// Function Name: GetDDC_Access()
// Function Description: Request access to DDC bus from the receiver
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
#define T_DDC_ACCESS    50

byte GetDDC_Access (byte* SysCtrlRegVal)
{
    byte sysCtrl;
    byte DDCReqTimeout = T_DDC_ACCESS;
    byte TPI_ControlImage;

    APP_PRINT(">>GetDDC_Access()\r\n");

    sysCtrl = ReadByteTPI (TPI_SYSTEM_CONTROL_DATA_REG);			// Read and store original value. Will be passed into ReleaseDDC()
    *SysCtrlRegVal = sysCtrl;

    sysCtrl |= DDC_BUS_REQUEST_REQUESTED;
    WriteByteTPI (TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);

    while (DDCReqTimeout--)											// Loop till 0x1A[1] reads "1"
    {
        TPI_ControlImage = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);

        if (TPI_ControlImage & DDC_BUS_GRANT_MASK)					// When 0x1A[1] reads "1"
        {
            sysCtrl |= DDC_BUS_GRANT_GRANTED;
            WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);		// lock host DDC bus access (0x1A[2:1] = 11)
            return TRUE;
        }

        WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);			// 0x1A[2] = "1" - Requst the DDC bus
        DelayMS(200);
    }

    WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, sysCtrl);				// Failure... restore original value.
    return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: ReleaseDDC()
// Function Description: Release DDC bus
//
// Accepts: none
// Returns: TRUE if bus released successfully. FALSE if failed.
// Globals: none
//------------------------------------------------------------------------------
byte ReleaseDDC (byte SysCtrlRegVal)
{
    byte DDCReqTimeout = T_DDC_ACCESS;
    byte TPI_ControlImage;

    APP_PRINT(">>ReleaseDDC()\r\n");

    SysCtrlRegVal &= ~BITS_2_1;					// Just to be sure bits [2:1] are 0 before it is written

    while (DDCReqTimeout--)						// Loop till 0x1A[1] reads "0"
    {
        // Cannot use ReadClearWriteTPI() here. A read of TPI_SYSTEM_CONTROL is invalid while DDC is granted.
        // Doing so will return 0xFF, and cause an invalid value to be written back.
        //ReadClearWriteTPI(TPI_SYSTEM_CONTROL,BITS_2_1); // 0x1A[2:1] = "0" - release the DDC bus

        WriteByteTPI(TPI_SYSTEM_CONTROL_DATA_REG, SysCtrlRegVal);
        TPI_ControlImage = ReadByteTPI(TPI_SYSTEM_CONTROL_DATA_REG);

        if (!(TPI_ControlImage & BITS_2_1))		// When 0x1A[2:1] read "0"
            return TRUE;
    }

    return FALSE;								// Failed to release DDC bus control
}

//------------------------------------------------------------------------------
// Function Name: CheckEDID_Header()
// Function Description: Checks if EDID header is correct per VESA E-EDID standard
//
// Accepts: Pointer to 1st EDID block
// Returns: TRUE or FLASE
// Globals: EDID data
//------------------------------------------------------------------------------
byte CheckEDID_Header (byte *Block)
{
    byte i = 0;

    if (Block[i])               // byte 0 must be 0
        return FALSE;

    for (i = 1; i < 1 + EDID_HDR_NO_OF_FF; i++)
    {
        if(Block[i] != 0xFF)    // bytes [1..6] must be 0xFF
            return FALSE;
    }

    if (Block[i])               // byte 7 must be 0
        return FALSE;

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: DoEDID_Checksum()
// Function Description: Calculte checksum of the 128 byte block pointed to by the
//                  pointer passed as parameter
//
// Accepts: Pointer to a 128 byte block whose checksum needs to be calculated
// Returns: TRUE or FLASE
// Globals: EDID data
//------------------------------------------------------------------------------
byte DoEDID_Checksum (byte *Block)
{
    byte i;
    byte CheckSum = 0;

    for (i = 0; i < EDID_BLOCK_SIZE; i++)
        CheckSum += Block[i];

    if (CheckSum)
        return FALSE;

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseEstablishedTiming()
// Function Description: Parse the established timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
#if (CONF__TPI_EDID_PRINT == ENABLE)
void ParseEstablishedTiming (byte *Data)
{
    APP_PRINT("Parsing Established Timing:\r\n");
    APP_PRINT("===========================\r\n");

    // Parse Established Timing Byte #0
    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_7)
        APP_PRINT("720 x 400 @ 70Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_6)
        APP_PRINT("720 x 400 @ 88Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_5)
        APP_PRINT("640 x 480 @ 60Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_4)
        APP_PRINT("640 x 480 @ 67Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_3)
        APP_PRINT("640 x 480 @ 72Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_2)
        APP_PRINT("640 x 480 @ 75Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_1)
        APP_PRINT("800 x 600 @ 56Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX] & BIT_0)
        APP_PRINT("800 x 400 @ 60Hz\r\n");

    // Parse Established Timing Byte #1:
    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_7)
        APP_PRINT("800 x 600 @ 72Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_6)
        APP_PRINT("800 x 600 @ 75Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_5)
        APP_PRINT("832 x 624 @ 75Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_4)
        APP_PRINT("1024 x 768 @ 87Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_3)
        APP_PRINT("1024 x 768 @ 60Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_2)
        APP_PRINT("1024 x 768 @ 70Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_1)
        APP_PRINT("1024 x 768 @ 75Hz\r\n");

    if(Data[ESTABLISHED_TIMING_INDEX + 1]  & BIT_0)
        APP_PRINT("1280 x 1024 @ 75Hz\r\n");

    // Parse Established Timing Byte #2:
    if(Data[ESTABLISHED_TIMING_INDEX + 2] & 0x80)
        APP_PRINT("1152 x 870 @ 75Hz\r\n");

    if((!Data[0]) && (!Data[ESTABLISHED_TIMING_INDEX + 1]  ) && (!Data[2]))
        APP_PRINT("No established video modes\r\n");
}

//------------------------------------------------------------------------------
// Function Name: ParseStandardTiming()
// Function Description: Parse the standard timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
void ParseStandardTiming (byte *Data)
{
    byte i;
    byte AR_Code;

    APP_PRINT("Parsing Standard Timing:\r\n");
    APP_PRINT("========================\r\n");

    for (i = 0; i < NUM_OF_STANDARD_TIMINGS; i += 2)
    {
        if ((Data[STANDARD_TIMING_OFFSET + i] == 0x01) && ((Data[STANDARD_TIMING_OFFSET + i + 1]) == 1))
        {
            APP_PRINT("Standard Timing Undefined\r\n"); // per VESA EDID standard, Release A, Revision 1, February 9, 2000, Sec. 3.9
        }
        else
        {
            APP_PRINT("Horizontal Active pixels: %i\n", (int)((Data[STANDARD_TIMING_OFFSET + i] + 31) * 8));  // per VESA EDID standard, Release A, Revision 1, February 9, 2000, Table 3.15

            AR_Code = (Data[STANDARD_TIMING_OFFSET + i + 1] & TWO_MSBITS) >> 6;
            APP_PRINT("Aspect Ratio: ");

            switch(AR_Code)
            {
                case AR16_10:
                    APP_PRINT("16:10\r\n");
                    break;

                case AR4_3:
                    APP_PRINT("4:3\r\n");
                    break;

                case AR5_4:
                    APP_PRINT("5:4\r\n");
                    break;

                case AR16_9:
                    APP_PRINT("16:9\r\n");
                    break;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Function Name: ParseDetailedTiming()
// Function Description: Parse the detailed timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
//              Offset to the beginning of the Detailed Timing Descriptor data.
//
//              Block indicator to distinguish between block #0 and blocks #2, #3
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
byte ParseDetailedTiming (byte *Data, byte DetailedTimingOffset, byte Block)
{
    byte TmpByte;
    byte i;
    word TmpWord;

    TmpWord = Data[DetailedTimingOffset + PIX_CLK_OFFSET] +
              256 * Data[DetailedTimingOffset + PIX_CLK_OFFSET + 1];

    if (TmpWord == 0x00)            // 18 byte partition is used as either for Monitor Name or for Monitor Range Limits or it is unused
    {
        if (Block == EDID_BLOCK_0)      // if called from Block #0 and first 2 bytes are 0 => either Monitor Name or for Monitor Range Limits
        {
            if (Data[DetailedTimingOffset + 3] == 0xFC) // these 13 bytes are ASCII coded monitor name
            {
                APP_PRINT("Monitor Name: ");

                for (i = 0; i < 13; i++)
                {
                    APP_PRINT("%c", Data[DetailedTimingOffset + 5 + i]); // Display monitor name on SiIMon
                }

                APP_PRINT("\r\n");
            }

            else if (Data[DetailedTimingOffset + 3] == 0xFD) // these 13 bytes contain Monitor Range limits, binary coded
            {
                APP_PRINT("Monitor Range Limits:\n\r\n");

                i = 0;
                APP_PRINT("Min Vertical Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Max Vertical Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Min Horizontal Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Max Horizontal Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Max Supported pixel clock rate in MHz/10: %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Tag for secondary timing formula (00h=not used): %d\n", (int) Data[DetailedTimingOffset + 5 + i++]); //
                APP_PRINT("Min Vertical Rate in Hz %d\n", (int) Data[DetailedTimingOffset + 5 + i]); //
                APP_PRINT("\r\n");
            }
        }

        else if (Block == EDID_BLOCK_2_3)                          // if called from block #2 or #3 and first 2 bytes are 0x00 (padding) then this
        {
            // descriptor partition is not used and parsing should be stopped
            APP_PRINT("No More Detailed descriptors in this block\r\n");
            APP_PRINT("\r\n");
            return FALSE;
        }
    }

    else                                            // first 2 bytes are not 0 => this is a detailed timing descriptor from either block
    {
        if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x36))
        {
            APP_PRINT("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 1:\r\n");
            APP_PRINT("===========================================================\n\r\n");
        }
        else if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x48))
        {
            APP_PRINT("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 2:\r\n");
            APP_PRINT("===========================================================\n\r\n");
        }

        APP_PRINT("Pixel Clock (MHz * 100): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + H_ACTIVE_OFFSET] +
                  256 * ((Data[DetailedTimingOffset + H_ACTIVE_OFFSET + 2] >> 4) & FOUR_LSBITS);
        APP_PRINT("Horizontal Active Pixels: %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + H_BLANKING_OFFSET] +
                  256 * (Data[DetailedTimingOffset + H_BLANKING_OFFSET + 1] & FOUR_LSBITS);
        APP_PRINT("Horizontal Blanking (Pixels): %d\n", (int)TmpWord);

        TmpWord = (Data[DetailedTimingOffset + V_ACTIVE_OFFSET] ) +
                  256 * ((Data[DetailedTimingOffset + (V_ACTIVE_OFFSET) + 2] >> 4) & FOUR_LSBITS);
        APP_PRINT("Vertical Active (Lines): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + V_BLANKING_OFFSET] +
                  256 * (Data[DetailedTimingOffset + V_BLANKING_OFFSET + 1] & LOW_NIBBLE);
        APP_PRINT("Vertical Blanking (Lines): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + H_SYNC_OFFSET] +
                  256 * ((Data[DetailedTimingOffset + (H_SYNC_OFFSET + 3)] >> 6) & TWO_LSBITS);
        APP_PRINT("Horizontal Sync Offset (Pixels): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + H_SYNC_PW_OFFSET] +
                  256 * ((Data[DetailedTimingOffset + (H_SYNC_PW_OFFSET + 2)] >> 4) & TWO_LSBITS);
        APP_PRINT("Horizontal Sync Pulse Width (Pixels): %d\n", (int)TmpWord);

        TmpWord = (Data[DetailedTimingOffset + V_SYNC_OFFSET] >> 4) & FOUR_LSBITS +
                  256 * ((Data[DetailedTimingOffset + (V_SYNC_OFFSET + 1)] >> 2) & TWO_LSBITS);
        APP_PRINT("Vertical Sync Offset (Lines): %d\n", (int)TmpWord);

        TmpWord = (Data[DetailedTimingOffset + V_SYNC_PW_OFFSET]) & FOUR_LSBITS +
                  256 * (Data[DetailedTimingOffset + (V_SYNC_PW_OFFSET + 1)] & TWO_LSBITS);
        APP_PRINT("Vertical Sync Pulse Width (Lines): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + H_IMAGE_SIZE_OFFSET] +
                  256 * (((Data[DetailedTimingOffset + (H_IMAGE_SIZE_OFFSET + 2)]) >> 4) & FOUR_LSBITS);
        APP_PRINT("Horizontal Image Size (mm): %d\n", (int)TmpWord);

        TmpWord = Data[DetailedTimingOffset + V_IMAGE_SIZE_OFFSET] +
                  256 * (Data[DetailedTimingOffset + (V_IMAGE_SIZE_OFFSET + 1)] & FOUR_LSBITS);
        APP_PRINT("Vertical Image Size (mm): %d\n", (int)TmpWord);

        TmpByte = Data[DetailedTimingOffset + H_BORDER_OFFSET];
        APP_PRINT("Horizontal Border (Pixels): %d\n", (int)TmpByte);

        TmpByte = Data[DetailedTimingOffset + V_BORDER_OFFSET];
        APP_PRINT("Vertical Border (Lines): %d\n", (int)TmpByte);

        TmpByte = Data[DetailedTimingOffset + FLAGS_OFFSET];

        if (TmpByte & BIT_7)
            APP_PRINT("Interlaced\r\n");
        else
            APP_PRINT("Non-Interlaced\r\n");

        if (!(TmpByte & BIT_5) && !(TmpByte & BIT_6))
            APP_PRINT("Normal Display, No Stereo\r\n");
        else
            APP_PRINT("Refer to VESA E-EDID Release A, Revision 1, table 3.17\r\n");

        if (!(TmpByte & BIT_3) && !(TmpByte & BIT_4))
            APP_PRINT("Analog Composite\r\n");

        if ((TmpByte & BIT_3) && !(TmpByte & BIT_4))
            APP_PRINT("Bipolar Analog Composite\r\n");
        else if (!(TmpByte & BIT_3) && (TmpByte & BIT_4))
            APP_PRINT("Digital Composite\r\n");

        else if ((TmpByte & BIT_3) && (TmpByte & BIT_4))
            APP_PRINT("Digital Separate\r\n");

        APP_PRINT("\r\n");
    }

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseBlock_0_TimingDescripors()
// Function Description: Parse EDID Block 0 timing descriptors per EEDID 1.3
//                  standard. APP_PRINT() values to screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
void ParseBlock_0_TimingDescripors (byte *Data)
{
    byte i;
    byte Offset;

    ParseEstablishedTiming(Data);
    ParseStandardTiming(Data);

    for (i = 0; i < NUM_OF_DETAILED_DESCRIPTORS; i++)
    {
        Offset = DETAILED_TIMING_OFFSET + (LONG_DESCR_LEN * i);
        ParseDetailedTiming(Data, Offset, EDID_BLOCK_0);
    }
}
#endif

//------------------------------------------------------------------------------
// Function Name: ParseEDID()
// Function Description: Extract sink properties from its EDID file and save them in
//                  global structure g_edid.
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: EDID data
// NOTE: Fields that are not supported by the 9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte ParseEDID (byte *pEdid, byte *numExt)
{
    byte i, j, k;

    APP_PRINT("\r\n");
    APP_PRINT("EDID DATA (Segment = 0 Block = 0 Offset = %d):\n", (int) EDID_BLOCK_0_OFFSET);

    for (j = 0, i = 0; j < 128; j++)
    {
        k = pEdid[j];
        APP_PRINT("%2.2X ", (int) k);
        i++;

        if (i == 0x10)
        {
            APP_PRINT("\r\n");
            i = 0;
        }
    }

    APP_PRINT("\r\n");

    if (!CheckEDID_Header(pEdid))
    {
        // first 8 bytes of EDID must be {0, FF, FF, FF, FF, FF, FF, 0}
        APP_PRINT("EDID -> Incorrect Header\r\n");
        return EDID_INCORRECT_HEADER;
    }

    if (!DoEDID_Checksum(pEdid))
    {
        // non-zero EDID checksum
        APP_PRINT("EDID -> Checksum Error\r\n");
        return EDID_CHECKSUM_ERROR;
    }

    #if (CONF__TPI_EDID_PRINT == ENABLE)
    ParseBlock_0_TimingDescripors(pEdid);			// Parse EDID Block #0 Desctiptors
    #endif

    *numExt = pEdid[NUM_OF_EXTEN_ADDR];	// read # of extensions from offset 0x7E of block 0
    APP_PRINT("EDID -> 861 Extensions = %d\n", (int) *numExt);

    if (!(*numExt))
    {
        // No extensions to worry about
        return EDID_NO_861_EXTENSIONS;
    }

    //return Parse861Extensions(NumOfExtensions);			// Parse 861 Extensions (short and long descriptors);
    return (EDID_OK);
}

//------------------------------------------------------------------------------
// Function Name: Parse861ShortDescriptors()
// Function Description: Parse CEA-861 extension short descriptors of the EDID block
//                  passed as a parameter and save them in global structure g_edid.
//
// Accepts: A pointer to the EDID 861 Extension block being parsed.
// Returns: EDID_PARSED_OK if EDID parsed correctly. Error code if failed.
// Globals: EDID data
// NOTE: Fields that are not supported by the 9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte Parse861ShortDescriptors (byte *Data)
{
    byte LongDescriptorOffset;
    byte DataBlockLength;
    byte DataIndex;
    byte ExtendedTagCode;
    byte VSDB_BaseOffset = 0;

    byte V_DescriptorIndex = 0;  // static to support more than one extension
    byte A_DescriptorIndex = 0;  // static to support more than one extension

    byte TagCode;

    byte i;
    byte j;

    if (Data[EDID_TAG_ADDR] != EDID_EXTENSION_TAG)
    {
        APP_PRINT("EDID -> Extension Tag Error\r\n");
        return EDID_EXT_TAG_ERROR;
    }

    if (Data[EDID_REV_ADDR] != EDID_REV_THREE)
    {
        APP_PRINT("EDID -> Revision Error\r\n");
        return EDID_REV_ADDR_ERROR;
    }

    LongDescriptorOffset = Data[LONG_DESCR_PTR_IDX];    // block offset where long descriptors start

    g_edid.UnderScan = ((Data[MISC_SUPPORT_IDX]) >> 7) & LSBIT;  // byte #3 of CEA extension version 3
    g_edid.BasicAudio = ((Data[MISC_SUPPORT_IDX]) >> 6) & LSBIT;
    g_edid.YCbCr_4_4_4 = ((Data[MISC_SUPPORT_IDX]) >> 5) & LSBIT;
    g_edid.YCbCr_4_2_2 = ((Data[MISC_SUPPORT_IDX]) >> 4) & LSBIT;

    DataIndex = EDID_DATA_START;            // 4

    while (DataIndex < LongDescriptorOffset)
    {
        TagCode = (Data[DataIndex] >> 5) & THREE_LSBITS;
        DataBlockLength = Data[DataIndex++] & FIVE_LSBITS;

        if ((DataIndex + DataBlockLength) > LongDescriptorOffset)
        {
            APP_PRINT("EDID -> V Descriptor Overflow\r\n");
            return EDID_V_DESCR_OVERFLOW;
        }

        i = 0;                                  // num of short video descriptors in current data block

        switch (TagCode)
        {
            case VIDEO_D_BLOCK:
                while ((i < DataBlockLength) && (i < MAX_V_DESCRIPTORS))        // each SVD is 1 byte long
                {
                    g_edid.VideoDescriptor[V_DescriptorIndex++] = Data[DataIndex++];
                    i++;
                }

                DataIndex += DataBlockLength - i;   // if there are more STDs than MAX_V_DESCRIPTORS, skip the last ones. Update DataIndex

                APP_PRINT("EDID -> Short Descriptor Video Block\r\n");
                break;

            case AUDIO_D_BLOCK:
                while (i < DataBlockLength / 3)     // each SAD is 3 bytes long
                {
                    j = 0;

                    while (j < AUDIO_DESCR_SIZE)    // 3
                    {
                        g_edid.AudioDescriptor[A_DescriptorIndex][j++] = Data[DataIndex++];
                    }

                    A_DescriptorIndex++;
                    i++;
                }

                APP_PRINT("EDID -> Short Descriptor Audio Block\r\n");
                break;

            case  SPKR_ALLOC_D_BLOCK:
                g_edid.SpkrAlloc[i++] = Data[DataIndex++];       // although 3 bytes are assigned to Speaker Allocation, only
                DataIndex += 2;                                     // the first one carries information, so the next two are ignored by this code.
                APP_PRINT("EDID -> Short Descriptor Speaker Allocation Block\r\n");
                break;

            case USE_EXTENDED_TAG:
                ExtendedTagCode = Data[DataIndex++];

                switch (ExtendedTagCode)
                {
                    case VIDEO_CAPABILITY_D_BLOCK:
                        APP_PRINT("EDID -> Short Descriptor Video Capability Block\r\n");

                        // TO BE ADDED HERE: Save "video capability" parameters in g_edid data structure
                        // Need to modify that structure definition
                        // In the meantime: just increment DataIndex by 1
                        DataIndex += 1;    // replace with reading and saving the proper data per CEA-861 sec. 7.5.6 while incrementing DataIndex
                        break;

                    case COLORIMETRY_D_BLOCK:
                        g_edid.ColorimetrySupportFlags = Data[DataIndex++] & BITS_1_0;
                        g_edid.MetadataProfile = Data[DataIndex++] & BITS_2_1_0;

                        APP_PRINT("EDID -> Short Descriptor Colorimetry Block\r\n");
                        break;
                }

                break;

            case VENDOR_SPEC_D_BLOCK:
                VSDB_BaseOffset = DataIndex - 1;

                if ((Data[DataIndex++] == 0x03) &&    // check if sink is HDMI compatible
                        (Data[DataIndex++] == 0x0C) &&
                        (Data[DataIndex++] == 0x00))

                    g_edid.HDMI_Sink = TRUE;
                else
                    g_edid.HDMI_Sink = FALSE;

                g_edid.CEC_A_B = Data[DataIndex++];  // CEC Physical address
                g_edid.CEC_C_D = Data[DataIndex++];

                #ifdef DEV_SUPPORT_CEC
                // Take the Address that was passed in the EDID and use this API
                // to set the physical address for CEC.
                {
                    word	phyAddr;
                    phyAddr = (word)g_edid.CEC_C_D;	 // Low-order nibbles
                    phyAddr |= ((word)g_edid.CEC_A_B << 8); // Hi-order nibbles

                    // Is the new PA different from the current PA?
                    if (phyAddr != SI_CecGetDevicePA ())
                    {
                        // Yes!  So change the PA
                        SI_CecSetDevicePA (phyAddr);
                    }
                }
                #endif

                if ((DataIndex + 7) > VSDB_BaseOffset + DataBlockLength)        // Offset of 3D_Present bit in VSDB
                    g_edid._3D_Supported = FALSE;
                else if (Data[DataIndex + 7] >> 7)
                    g_edid._3D_Supported = TRUE;
                else
                    g_edid._3D_Supported = FALSE;

                DataIndex += DataBlockLength - HDMI_SIGNATURE_LEN - CEC_PHYS_ADDR_LEN; // Point to start of next block
                APP_PRINT("EDID -> Short Descriptor Vendor Block\r\n");
                APP_PRINT("\r\n");
                break;

            default:
                APP_PRINT("EDID -> Unknown Tag Code\r\n");
                return EDID_UNKNOWN_TAG_CODE;

        }                   // End, Switch statement
    }                       // End, while (DataIndex < LongDescriptorOffset) statement

    return EDID_SHORT_DESCRIPTORS_OK;
}

//------------------------------------------------------------------------------
// Function Name: Parse861LongDescriptors()
// Function Description: Parse CEA-861 extension long descriptors of the EDID block
//                  passed as a parameter and APP_PRINT() them to the screen.
//
// Accepts: A pointer to the EDID block being parsed
// Returns: An error code if no long descriptors found; EDID_PARSED_OK if descriptors found.
// Globals: none
//------------------------------------------------------------------------------
byte Parse861LongDescriptors (byte *Data)
{
    byte LongDescriptorsOffset;
    byte DescriptorNum = 1;

    LongDescriptorsOffset = Data[LONG_DESCR_PTR_IDX];   // EDID block offset 2 holds the offset

    if (!LongDescriptorsOffset)                         // per CEA-861-D, table 27
    {
        APP_PRINT("EDID -> No Detailed Descriptors\r\n");
        return EDID_NO_DETAILED_DESCRIPTORS;
    }

    // of the 1st 18-byte descriptor
    while (LongDescriptorsOffset + LONG_DESCR_LEN < EDID_BLOCK_SIZE)
    {
        APP_PRINT("Parse Results - CEA-861 Long Descriptor #%d:\n", (int) DescriptorNum);
        APP_PRINT("===============================================================\r\n");

        #if (CONF__TPI_EDID_PRINT == ENABLE)

        if (!ParseDetailedTiming(Data, LongDescriptorsOffset, EDID_BLOCK_2_3))
            break;

        #endif
        LongDescriptorsOffset +=  LONG_DESCR_LEN;
        DescriptorNum++;
    }

    return EDID_LONG_DESCRIPTORS_OK;
}

//------------------------------------------------------------------------------
// Function Name: Parse861Extensions()
// Function Description: Parse CEA-861 extensions from EDID ROM (EDID blocks beyond
//                  block #0). Save short descriptors in global structure
//                  g_edid. APP_PRINT() long descriptors to the screen.
//
// Accepts: The number of extensions in the EDID being parsed
// Returns: EDID_PARSED_OK if EDID parsed correctly. Error code if failed.
// Globals: EDID data
// NOTE: Fields that are not supported by the 9022/4 (such as deep color) were not parsed.
//------------------------------------------------------------------------------
byte Parse861Extensions (byte NumOfExtensions)
{
    byte i, j, k;

    byte ErrCode;

    byte V_DescriptorIndex = 0;
    byte A_DescriptorIndex = 0;

    byte Segment = 0;
    byte Block = 0;
    byte Offset = 0;

    g_edid.HDMI_Sink = FALSE;

    do
    {
        Block++;

        Offset = 0;

        if ((Block % 2) > 0)
        {
            Offset = EDID_BLOCK_SIZE;
        }

        Segment = (byte) (Block / 2);

        if (Block == 1)
        {
            ReadBlockEDID(EDID_BLOCK_1_OFFSET, EDID_BLOCK_SIZE, g_CommData);    // read first 128 bytes of EDID ROM
        }
        else
        {
            ReadSegmentBlockEDID(Segment, Offset, EDID_BLOCK_SIZE, g_CommData);     // read next 128 bytes of EDID ROM
        }

        APP_PRINT("\r\n");
        APP_PRINT("EDID DATA (Segment = %d Block = %d Offset = %d):\n", (int) Segment, (int) Block, (int) Offset);

        for (j = 0, i = 0; j < 128; j++)
        {
            k = g_CommData[j];
            APP_PRINT("%2.2X ", (int) k);
            i++;

            if (i == 0x10)
            {
                APP_PRINT("\r\n");
                i = 0;
            }
        }

        APP_PRINT("\r\n");

        if ((NumOfExtensions > 1) && (Block == 1))
        {
            continue;
        }

        ErrCode = Parse861ShortDescriptors(g_CommData);

        if (ErrCode != EDID_SHORT_DESCRIPTORS_OK)
        {
            return ErrCode;
        }

        ErrCode = Parse861LongDescriptors(g_CommData);

        if (ErrCode != EDID_LONG_DESCRIPTORS_OK)
        {
            return ErrCode;
        }

    }
    while (Block < NumOfExtensions);

    return EDID_OK;
}

//------------------------------------------------------------------------------
// Function Name: DoEdidRead()
// Function Description: EDID processing
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
byte DoEdidRead (void)
{
    byte SysCtrlReg;
    byte Result;
    byte NumOfExtensions;

    // If we already have valid EDID data, ship this whole thing
    if (g_edid.edidDataValid == FALSE)
    {
        // Request access to DDC bus from the receiver
        if (GetDDC_Access(&SysCtrlReg))
        {
            ReadBlockEDID(EDID_BLOCK_0_OFFSET, EDID_BLOCK_SIZE, g_CommData);		// read first 128 bytes of EDID ROM
            Result = ParseEDID(g_CommData, &NumOfExtensions);

            if (Result != EDID_OK)
            {
                if (Result == EDID_NO_861_EXTENSIONS)
                {
                    APP_PRINT("EDID -> No 861 Extensions\r\n");
                    g_edid.HDMI_Sink = FALSE;
                    g_edid.YCbCr_4_4_4 = FALSE;
                    g_edid.YCbCr_4_2_2 = FALSE;
                    g_edid.CEC_A_B = 0x00;
                    g_edid.CEC_C_D = 0x00;
                }
                else
                {
                    APP_PRINT("EDID -> Parse FAILED\r\n");
                    g_edid.HDMI_Sink = TRUE;
                    g_edid.YCbCr_4_4_4 = FALSE;
                    g_edid.YCbCr_4_2_2 = FALSE;
                    g_edid.CEC_A_B = 0x00;
                    g_edid.CEC_C_D = 0x00;
                }
            }
            else
            {
                APP_PRINT("EDID -> Parse OK\r\n");
                Result = Parse861Extensions(NumOfExtensions);		// Parse 861 Extensions (short and long descriptors);

                if (Result != EDID_OK)
                {
                    APP_PRINT("EDID -> Extension Parse FAILED\r\n");
                    g_edid.HDMI_Sink = TRUE;
                    g_edid.YCbCr_4_4_4 = FALSE;
                    g_edid.YCbCr_4_2_2 = FALSE;
                    g_edid.CEC_A_B = 0x00;
                    g_edid.CEC_C_D = 0x00;
                }
            }

            if (!ReleaseDDC(SysCtrlReg))				// Host must release DDC bus once it is done reading EDID
            {
                APP_PRINT("EDID -> DDC bus release failed\r\n");
                return EDID_DDC_BUS_RELEASE_FAILURE;
            }
        }
        else
        {
            APP_PRINT("EDID -> DDC bus request failed\r\n");
            g_edid.HDMI_Sink = TRUE;
            g_edid.YCbCr_4_4_4 = FALSE;
            g_edid.YCbCr_4_2_2 = FALSE;
            g_edid.CEC_A_B = 0x00;
            g_edid.CEC_C_D = 0x00;
            return EDID_DDC_BUS_REQ_FAILURE;
        }

        APP_PRINT("EDID -> g_edid.HDMI_Sink = %d\n", (int)g_edid.HDMI_Sink);
        APP_PRINT("EDID -> g_edid.YCbCr_4_4_4 = %d\n", (int)g_edid.YCbCr_4_4_4);
        APP_PRINT("EDID -> g_edid.YCbCr_4_2_2 = %d\n", (int)g_edid.YCbCr_4_2_2);
        APP_PRINT("EDID -> g_edid.CEC_A_B = 0x%x\n", (int)g_edid.CEC_A_B);
        APP_PRINT("EDID -> g_edid.CEC_C_D = 0x%x\n", (int)g_edid.CEC_C_D);

        g_edid.edidDataValid = TRUE;
    }

    return 0;
}
#endif



//------------------------------------------------------------------------------
// Function Name: ConvertVIC_To_VM_Index()
// Function Description: Convert Video Identification Code to the corresponding
//                  		index of VModesTable[]. Conversion also depends on the
//					value of the 3D_Structure parameter in the case of 3D video format.
// Accepts: VIC to be converted; 3D_Structure value
// Returns: Index into VModesTable[] corrsponding to VIC
// Globals: VModesTable[] siHdmiTx
// Note: Conversion is for 861-D formats, HDMI_VIC or 3D
//------------------------------------------------------------------------------
byte ConvertVIC_To_VM_Index (void)
{
    byte index;

    //
    // The global VideoModeDescription contains all the information we need about
    // the Video mode for use to find its entry in the Videio mode table.
    //
    // The first issue.  The "VIC" may be a 891-D VIC code, or it might be an
    // HDMI_VIC code, or it might be a 3D code.  Each require different handling
    // to get the proper video mode table index.
    //
    if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_CEA_VIC)
    {
        //
        // This is a regular 861-D format VIC, so we use the VIC to Index
        // table to look up the index.
        //
        index = VIC2Index[siHdmiTx.VIC];
    }
    else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_HDMI_VIC)
    {
        //
        // HDMI_VIC conversion is simple.  We need to subtract one because the codes start
        // with one instead of zero.  These values are from HDMI 1.4 Spec Table 8-13.
        //
        if ((siHdmiTx.VIC < 1) || (siHdmiTx.VIC > 4))
        {
            index = DEFAULT_VIDEO_MODE;
        }
        else
        {
            index = (HDMI_VIC_BASE - 1) + siHdmiTx.VIC;
        }
    }
    else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_3D)
    {
        //
        // Currently there are only a few VIC modes that we can do in 3D.  If the VIC code is not
        // one of these OR if the packing type is not supported for that VIC code, then it is an
        // error and we go to the default video mode.  See HDMI Spec 1.4 Table H-6.
        //
        switch (siHdmiTx.VIC)
        {
            case VIC_FOR_480P_60Hz_4X3:
            case VIC_FOR_480P_60Hz_16X9:

                // We only support Side-by-Side (Half) for these modes
                if (siHdmiTx.ThreeDStructure == SIDE_BY_SIDE_HALF)
                    index = VIC_3D_BASE + 0;
                else
                    index = DEFAULT_VIDEO_MODE;

                break;

            case VIC_FOR_720P_60Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 1;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_1080i_60Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 2;
                        break;

                    case VMD_3D_FIELDALTERNATIVE:
                        index = VIC_3D_BASE + 3;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_1080p_60Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 4;
                        break;

                    case VMD_3D_LINEALTERNATIVE:
                        index = VIC_3D_BASE + 5;
                        break;

                    case SIDE_BY_SIDE_FULL:
                        index = VIC_3D_BASE + 6;
                        break;

                    case SIDE_BY_SIDE_HALF:
                        index = VIC_3D_BASE + 7;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_720P_50Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 8;
                        break;

                    case VMD_3D_LDEPTH:
                        index = VIC_3D_BASE + 9;
                        break;

                    case VMD_3D_LDEPTHGRAPHICS:
                        index = VIC_3D_BASE + 10;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_1080i_50Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 11;
                        break;

                    case VMD_3D_FIELDALTERNATIVE:
                        index = VIC_3D_BASE + 12;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_1080p_50Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 13;
                        break;

                    case VMD_3D_LINEALTERNATIVE:
                        index = VIC_3D_BASE + 14;
                        break;

                    case SIDE_BY_SIDE_FULL:
                        index = VIC_3D_BASE + 15;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            case VIC_FOR_1080p_24Hz:
                switch(siHdmiTx.ThreeDStructure)
                {
                    case FRAME_PACKING:
                        index = VIC_3D_BASE + 16;
                        break;

                    default:
                        index = DEFAULT_VIDEO_MODE;
                        break;
                }

                break;

            default:
                index = DEFAULT_VIDEO_MODE;
                break;
        }
    }
    else if (siHdmiTx.HDMIVideoFormat == VMD_HDMIFORMAT_PC)
    {
        if (siHdmiTx.VIC < PC_SIZE)
        {
            index = siHdmiTx.VIC + PC_BASE;
        }
        else
        {
            index = DEFAULT_VIDEO_MODE;
        }
    }
    else
    {
        // This should never happen!  If so, default to first table entry
        index = DEFAULT_VIDEO_MODE;
    }

    return index;
}


// Patches
//========
byte TPI_REG0x63_SAVED = 0;

//------------------------------------------------------------------------------
// Function Name: SetEmbeddedSync()
// Function Description: Set the 9022/4 registers to extract embedded sync.
//
// Accepts: Index of video mode to set
// Returns: TRUE
// Globals: VModesTable[]
//------------------------------------------------------------------------------
byte SetEmbeddedSync (void)
{
    byte	ModeTblIndex;
    word H_Bit_2_H_Sync;
    word Field2Offset;
    word H_SyncWidth;

    byte V_Bit_2_V_Sync;
    byte V_SyncWidth;
    byte B_Data[8];

    APP_PRINT(">>SetEmbeddedSync()\r\n");

    ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x01, 0x01);

    ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	 // set 0x60[7] = 0 for DE mode
    WriteByteTPI(TPI_DE_CTRL, 0x30);
    ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);       // set 0x60[7] = 1 for Embedded Sync

    ModeTblIndex = ConvertVIC_To_VM_Index();

    H_Bit_2_H_Sync = VModesTable[ModeTblIndex]._656.HBit2HSync;
    Field2Offset = VModesTable[ModeTblIndex]._656.Field2Offset;
    H_SyncWidth = VModesTable[ModeTblIndex]._656.HLength;
    V_Bit_2_V_Sync = VModesTable[ModeTblIndex]._656.VBit2VSync;
    V_SyncWidth = VModesTable[ModeTblIndex]._656.VLength;

    B_Data[0] = H_Bit_2_H_Sync & LOW_BYTE;                  // Setup HBIT_TO_HSYNC 8 LSBits (0x62)

    B_Data[1] = (H_Bit_2_H_Sync >> 8) & TWO_LSBITS;         // HBIT_TO_HSYNC 2 MSBits
    //B_Data[1] |= BIT_EN_SYNC_EXTRACT;                     // and Enable Embedded Sync to 0x63
    TPI_REG0x63_SAVED = B_Data[1];

    B_Data[2] = Field2Offset & LOW_BYTE;                    // 8 LSBits of "Field2 Offset" to 0x64
    B_Data[3] = (Field2Offset >> 8) & LOW_NIBBLE;           // 2 MSBits of "Field2 Offset" to 0x65

    B_Data[4] = H_SyncWidth & LOW_BYTE;
    B_Data[5] = (H_SyncWidth >> 8) & TWO_LSBITS;                    // HWIDTH to 0x66, 0x67
    B_Data[6] = V_Bit_2_V_Sync;                                     // VBIT_TO_VSYNC to 0x68
    B_Data[7] = V_SyncWidth;                                        // VWIDTH to 0x69

    WriteBlockTPI(TPI_HBIT_TO_HSYNC_7_0, 8, &B_Data[0]);

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: EnableEmbeddedSync()
// Function Description: EnableEmbeddedSync
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void EnableEmbeddedSync (void)
{
    APP_PRINT(">>EnableEmbeddedSync()\r\n");

    ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	 // set 0x60[7] = 0 for DE mode
    WriteByteTPI(TPI_DE_CTRL, 0x30);
    ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);       // set 0x60[7] = 1 for Embedded Sync
    ReadSetWriteTPI(TPI_DE_CTRL, BIT_6);
}

//------------------------------------------------------------------------------
// Function Name: SetDE()
// Function Description: Set the 9022/4 internal DE generator parameters
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//
// NOTE: 0x60[7] must be set to "0" for the follwing settings to take effect
//------------------------------------------------------------------------------
byte SetDE (void)
{
    byte RegValue;
    byte	ModeTblIndex;

    word H_StartPos, V_StartPos;
    word Htotal, Vtotal;
    word H_Res, V_Res;

    byte Polarity;
    byte B_Data[12];

    APP_PRINT(">>SetDE()\r\n");

    ModeTblIndex = ConvertVIC_To_VM_Index();

    if (VModesTable[ModeTblIndex]._3D_Struct != NO_3D_SUPPORT)
    {
        return DE_CANNOT_BE_SET_WITH_3D_MODE;
        APP_PRINT(">>SetDE() not allowed with 3D video format\r\n");
    }

    // Make sure that External Sync method is set before enableing the DE Generator:
    RegValue = ReadByteTPI(TPI_SYNC_GEN_CTRL);

    if (RegValue & BIT_7)
    {
        return DE_CANNOT_BE_SET_WITH_EMBEDDED_SYNC;
    }

    H_StartPos = VModesTable[ModeTblIndex].Pos.H;
    V_StartPos = VModesTable[ModeTblIndex].Pos.V;

    Htotal = VModesTable[ModeTblIndex].Tag.Total.Pixels;
    Vtotal = VModesTable[ModeTblIndex].Tag.Total.Lines;

    Polarity = (~VModesTable[ModeTblIndex].Tag.RefrTypeVHPol) & TWO_LSBITS;

    H_Res = VModesTable[ModeTblIndex].Res.H;

    if ((VModesTable[ModeTblIndex].Tag.RefrTypeVHPol & 0x04))
    {
        V_Res = (VModesTable[ModeTblIndex].Res.V) >> 1;
    }
    else
    {
        V_Res = (VModesTable[ModeTblIndex].Res.V);
    }

    B_Data[0] = H_StartPos & LOW_BYTE;              // 8 LSB of DE DLY in 0x62

    B_Data[1] = (H_StartPos >> 8) & TWO_LSBITS;     // 2 MSBits of DE DLY to 0x63
    B_Data[1] |= (Polarity << 4);                   // V and H polarity
    B_Data[1] |= BIT_EN_DE_GEN;                     // enable DE generator

    B_Data[2] = V_StartPos & SEVEN_LSBITS;      // DE_TOP in 0x64
    B_Data[3] = 0x00;                           // 0x65 is reserved
    B_Data[4] = H_Res & LOW_BYTE;               // 8 LSBits of DE_CNT in 0x66
    B_Data[5] = (H_Res >> 8) & LOW_NIBBLE;      // 4 MSBits of DE_CNT in 0x67
    B_Data[6] = V_Res & LOW_BYTE;               // 8 LSBits of DE_LIN in 0x68
    B_Data[7] = (V_Res >> 8) & THREE_LSBITS;    // 3 MSBits of DE_LIN in 0x69
    B_Data[8] = Htotal & LOW_BYTE;				// 8 LSBits of H_RES in 0x6A
    B_Data[9] =	(Htotal >> 8) & LOW_NIBBLE;		// 4 MSBITS of H_RES in 0x6B
    B_Data[10] = Vtotal & LOW_BYTE;				// 8 LSBits of V_RES in 0x6C
    B_Data[11] = (Vtotal >> 8) & BITS_2_1_0;	// 3 MSBITS of V_RES in 0x6D

    WriteBlockTPI(TPI_DE_DLY, 12, &B_Data[0]);
    TPI_REG0x63_SAVED = B_Data[1];

    return DE_SET_OK;                               // Write completed successfully
}

//------------------------------------------------------------------------------
// Function Name: SetFormat()
// Function Description: Set the 9022/4 format
//
// Accepts: none
// Returns: DE_SET_OK
// Globals: none
//------------------------------------------------------------------------------
void SetFormat (byte *Data)
{
    ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI); // Set HDMI mode to allow color space conversion

    WriteBlockTPI(TPI_INPUT_FORMAT_REG, 2, Data);   // Program TPI AVI Input and Output Format
    WriteByteTPI(TPI_END_RIGHT_BAR_MSB, 0x00);	  // Set last byte of TPI AVI InfoFrame for TPI AVI I/O Format to take effect

    if (!IsHDMI_Sink())
    {
        ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
    }

    if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
        EnableEmbeddedSync();					// Last byte of TPI AVI InfoFrame resets Embedded Sync Extraction
}

//------------------------------------------------------------------------------
// Function Name: printVideoMode()
// Function Description: print video mode
//
// Accepts: siHdmiTx.VIC
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void printVideoMode (void)
{
    APP_PRINT(">>Video mode = ");

    switch (siHdmiTx.VIC)
    {
        case 6:
            APP_PRINT("HDMI_480I60_4X3 \r\n");
            break;

        case 21:
            APP_PRINT("HDMI_576I50_4X3 \r\n");
            break;

        case 2:
            APP_PRINT("HDMI_480P60_4X3 \r\n");
            break;

        case 17:
            APP_PRINT("HDMI_576P50_4X3 \r\n");
            break;

        case 4:
            APP_PRINT("HDMI_720P60 \r\n");
            break;

        case 19:
            APP_PRINT("HDMI_720P50 \r\n");
            break;

        case 5:
            APP_PRINT("HDMI_1080I60 \r\n");
            break;

        case 20:
            APP_PRINT("HDMI_1080I50 \r\n");
            break;

        case 16:
            APP_PRINT("HDMI_1080P60 \r\n");
            break;

        case 31:
            APP_PRINT("HDMI_1080P50 \r\n");
            break;

        default:
            break;
    }
}

//------------------------------------------------------------------------------
// Function Name: InitVideo()
// Function Description: Set the 9022/4 to the video mode determined by GetVideoMode()
//
// Accepts: Index of video mode to set; Flag that distinguishes between
//                  calling this function after power up and after input
//                  resolution change
// Returns: TRUE
// Globals: VModesTable, VideoCommandImage
//------------------------------------------------------------------------------
byte InitVideo (byte TclkSel)
{
    byte	ModeTblIndex;

    #ifdef DEEP_COLOR
    byte temp;
    #endif
    byte B_Data[8];

    byte EMB_Status;
    byte DE_Status;
    byte Pattern;

    APP_PRINT(">>InitVideo()\r\n");
    printVideoMode();
    APP_PRINT(" HF:%d", (int) siHdmiTx.HDMIVideoFormat);
    APP_PRINT(" VIC:%d", (int) siHdmiTx.VIC);
    APP_PRINT(" A:%x", (int) siHdmiTx.AspectRatio);
    APP_PRINT(" CS:%x", (int) siHdmiTx.ColorSpace);
    APP_PRINT(" CD:%x", (int) siHdmiTx.ColorDepth);
    APP_PRINT(" CR:%x", (int) siHdmiTx.Colorimetry);
    //printk((" SM:%x", (int) siHdmiTx.SyncMode);
    APP_PRINT(" TCLK:%x", (int) siHdmiTx.TclkSel);
    APP_PRINT(" 3D:%d", (int) siHdmiTx.ThreeDStructure);
    APP_PRINT(" 3Dx:%d\n", (int) siHdmiTx.ThreeDExtData);

    ModeTblIndex = (byte)ConvertVIC_To_VM_Index();

    Pattern = (TclkSel << 6) & TWO_MSBITS;							// Use TPI 0x08[7:6] for 9022A/24A video clock multiplier
    ReadSetWriteTPI(TPI_PIX_REPETITION, Pattern);

    // Take values from VModesTable[]:
    if( (siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
            (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) )	//576i
    {
        if( siHdmiTx.ColorSpace == YCBCR422_8BITS)	//27Mhz pixel clock
        {
            B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;
            B_Data[1] = (VModesTable[ModeTblIndex].PixClk >> 8) & 0xFF;
        }
        else											//13.5Mhz pixel clock
        {
            B_Data[0] = (VModesTable[ModeTblIndex].PixClk / 2) & 0x00FF;
            B_Data[1] = ((VModesTable[ModeTblIndex].PixClk / 2) >> 8) & 0xFF;
        }

    }
    else
    {
        B_Data[0] = VModesTable[ModeTblIndex].PixClk & 0x00FF;			// write Pixel clock to TPI registers 0x00, 0x01
        B_Data[1] = (VModesTable[ModeTblIndex].PixClk >> 8) & 0xFF;
    }

    B_Data[2] = VModesTable[ModeTblIndex].Tag.VFreq & 0x00FF;		// write Vertical Frequency to TPI registers 0x02, 0x03
    B_Data[3] = (VModesTable[ModeTblIndex].Tag.VFreq >> 8) & 0xFF;

    if( (siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
            (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) )	//576i
    {
        B_Data[4] = (VModesTable[ModeTblIndex].Tag.Total.Pixels / 2) & 0x00FF;	// write total number of pixels to TPI registers 0x04, 0x05
        B_Data[5] = ((VModesTable[ModeTblIndex].Tag.Total.Pixels / 2) >> 8) & 0xFF;
    }
    else
    {
        B_Data[4] = VModesTable[ModeTblIndex].Tag.Total.Pixels & 0x00FF;	// write total number of pixels to TPI registers 0x04, 0x05
        B_Data[5] = (VModesTable[ModeTblIndex].Tag.Total.Pixels >> 8) & 0xFF;
    }

    B_Data[6] = VModesTable[ModeTblIndex].Tag.Total.Lines & 0x00FF;	// write total number of lines to TPI registers 0x06, 0x07
    B_Data[7] = (VModesTable[ModeTblIndex].Tag.Total.Lines >> 8) & 0xFF;

    WriteBlockTPI(TPI_PIX_CLK_LSB, 8, B_Data);						// Write TPI Mode data.

    // TPI Input Bus and Pixel Repetition Data
    // B_Data[0] = Reg0x08;
    B_Data[0] = 0;  // Set to default 0 for use again
    //B_Data[0] = (VModesTable[ModeTblIndex].PixRep) & LOW_BYTE;		// Set pixel replication field of 0x08
    B_Data[0] |= BIT_BUS_24;										// Set 24 bit bus
    B_Data[0] |= (TclkSel << 6) & TWO_MSBITS;

    #ifdef CLOCK_EDGE_FALLING
    B_Data[0] &= ~BIT_EDGE_RISE;									// Set to falling edge
    #endif
    #ifdef CLOCK_EDGE_RISING
    B_Data[0] |= BIT_EDGE_RISE;									// Set to rising edge
    #endif
    tpivmode[0] = B_Data[0]; // saved TPI Reg0x08 value.
    WriteByteTPI(TPI_PIX_REPETITION, B_Data[0]);					// 0x08

    // TPI AVI Input and Output Format Data
    // B_Data[0] = Reg0x09;
    // B_Data[1] = Reg0x0A;
    B_Data[0] = 0;  // Set to default 0 for use again
    B_Data[1] = 0;  // Set to default 0 for use again

    if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
    {
        EMB_Status = SetEmbeddedSync();
        EnableEmbeddedSync();

    }

    if (siHdmiTx.SyncMode == INTERNAL_DE)
    {
        ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, MSBIT);	// set 0x60[7] = 0 for External Sync
        DE_Status = SetDE();								// Call SetDE() with Video Mode as a parameter
    }

    if (siHdmiTx.ColorSpace == RGB)
        B_Data[0] = (((BITS_IN_RGB | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // 0x09

    else if (siHdmiTx.ColorSpace == YCBCR444)
        B_Data[0] = (((BITS_IN_YCBCR444 | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // 0x09

    else if ((siHdmiTx.ColorSpace == YCBCR422_16BITS) || (siHdmiTx.ColorSpace == YCBCR422_8BITS))
        B_Data[0] = (((BITS_IN_YCBCR422 | BITS_IN_AUTO_RANGE) & ~BIT_EN_DITHER_10_8) & ~BIT_EXTENDED_MODE); // 0x09

    #ifdef DEEP_COLOR

    switch (siHdmiTx.ColorDepth)
    {
        case 0:
            temp = 0x00;
            ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00);
            break;

        case 1:
            temp = 0x80;
            ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
            break;

        case 2:
            temp = 0xC0;
            ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
            break;

        case 3:
            temp = 0x40;
            ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, BIT_2);
            break;

        default:
            temp = 0x00;
            ReadModifyWriteTPI(TPI_DEEP_COLOR_GCP, BIT_2, 0x00);
            break;
    }

    B_Data[0] = ((B_Data[0] & 0x3F) | temp);
    #endif

    B_Data[1] = (BITS_OUT_RGB | BITS_OUT_AUTO_RANGE);  //Reg0x0A

    if ((siHdmiTx.VIC == 6) || (siHdmiTx.VIC == 7) ||	//480i
            (siHdmiTx.VIC == 21) || (siHdmiTx.VIC == 22) ||//576i
            (siHdmiTx.VIC == 2) || (siHdmiTx.VIC == 3) ||	//480p
            (siHdmiTx.VIC == 17) || (siHdmiTx.VIC == 18))	//576p
    {
        B_Data[1] &= ~BIT_BT_709;
    }
    else
    {
        B_Data[1] |= BIT_BT_709;
    }

    #ifdef DEEP_COLOR
    B_Data[1] = ((B_Data[1] & 0x3F) | temp);
    #endif

    #ifdef DEV_SUPPORT_EDID

    if (!IsHDMI_Sink())
    {
        B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
    }
    else
    {
        // Set YCbCr color space depending on EDID
        if (g_edid.YCbCr_4_4_4)
        {
            B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_YCBCR444);
        }
        else
        {
            if (g_edid.YCbCr_4_2_2)
            {
                B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_YCBCR422);
            }
            else
            {
                B_Data[1] = ((B_Data[1] & 0xFC) | BITS_OUT_RGB);
            }
        }
    }

    #endif

    tpivmode[1] = B_Data[0];	// saved TPI Reg0x09 value.
    tpivmode[2] = B_Data[1];  // saved TPI Reg0x0A value.
    SetFormat(B_Data);

    ReadClearWriteTPI(TPI_SYNC_GEN_CTRL, BIT_2);	// Number HSync pulses from VSync active edge to Video Data Period should be 20 (VS_TO_VIDEO)

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: SetAVI_InfoFrames()
// Function Description: Load AVI InfoFrame data into registers and send to sink
//
// Accepts: An API_Cmd parameter that holds the data to be sent in the InfoFrames
// Returns: TRUE
// Globals: none
//
// Note:          : Infoframe contents are from spec CEA-861-D
//
//------------------------------------------------------------------------------
byte SetAVI_InfoFrames (void)
{
    byte B_Data[SIZE_AVI_INFOFRAME];
    byte i;
    byte TmpVal;
    byte VModeTblIndex;

    APP_PRINT(">>SetAVI_InfoFrames()\r\n");

    for (i = 0; i < SIZE_AVI_INFOFRAME; i++)
        B_Data[i] = 0;

    #ifdef DEV_SUPPORT_EDID

    if (g_edid.YCbCr_4_4_4)
        TmpVal = 2;
    else if (g_edid.YCbCr_4_2_2)
        TmpVal = 1;
    else
        TmpVal = 0;

    #else
    TmpVal = 0;
    #endif

    B_Data[1] = (TmpVal << 5) & BITS_OUT_FORMAT;	// AVI Byte1: Y1Y0 (output format)
    B_Data[1] |= 0x11;  // A0 = 1; Active format identification data is present in the AVI InfoFrame. // S1:S0 = 01; Overscanned (television).

    if (siHdmiTx.ColorSpace == XVYCC444)                // Extended colorimetry - xvYCC
    {
        B_Data[2] = 0xC0;                                          // Extended colorimetry info (B_Data[3] valid (CEA-861D, Table 11)

        if (siHdmiTx.Colorimetry == COLORIMETRY_601)   		// xvYCC601
            B_Data[3] &= ~BITS_6_5_4;

        else if (siHdmiTx.Colorimetry == COLORIMETRY_709)	// xvYCC709
            B_Data[3] = (B_Data[3] & ~BITS_6_5_4) | BIT_4;
    }

    else if (siHdmiTx.Colorimetry == COLORIMETRY_709)		// BT.709
        B_Data[2] = 0x80;		// AVI Byte2: C1C0

    else if (siHdmiTx.Colorimetry == COLORIMETRY_601)		// BT.601
        B_Data[2] = 0x40;		// AVI Byte2: C1C0

    else													// Carries no data
    {
        // AVI Byte2: C1C0
        B_Data[2] &= ~BITS_7_6;							// colorimetry = 0
        B_Data[3] &= ~BITS_6_5_4;						// Extended colorimetry = 0
    }

    VModeTblIndex = ConvertVIC_To_VM_Index();

    B_Data[4] = siHdmiTx.VIC;

    //  Set the Aspect Ration info into the Infoframe Byte 2
    if (siHdmiTx.AspectRatio == VMD_ASPECT_RATIO_16x9)
    {
        B_Data[2] |= _16_To_9;                          // AVI Byte2: M1M0

        // If the Video Mode table says this mode can be 4x3 OR 16x9, and we are pointing to the
        // table entry that is 4x3, then we bump to the next Video Table entry which will be for 16x9.
        if ((VModesTable[VModeTblIndex].AspectRatio == R_4or16) && (AspectRatioTable[siHdmiTx.VIC - 1] == R_4))
        {
            siHdmiTx.VIC++;
            B_Data[4]++;
        }
    }
    else
    {
        B_Data[2] |= _4_To_3;                       // AVI Byte4: VIC
    }

    B_Data[2] |= SAME_AS_AR;                        // AVI Byte2: R3..R1 - Set to "Same as Picture Aspect Ratio"
    B_Data[5] = VModesTable[VModeTblIndex].PixRep;      // AVI Byte5: Pixel Replication - PR3..PR0

    // Calculate AVI InfoFrame ChecKsum
    B_Data[0] = 0x82 + 0x02 + 0x0D;

    for (i = 1; i < SIZE_AVI_INFOFRAME; i++)
    {
        B_Data[0] += B_Data[i];
    }

    B_Data[0] = 0x100 - B_Data[0];

    // Write the Inforframe data to the TPI Infoframe registers
    WriteBlockTPI(TPI_AVI_BYTE_0, SIZE_AVI_INFOFRAME, B_Data);

    if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
        EnableEmbeddedSync();

    return TRUE;
}

extern void siHdmiTx_PowerStateD0 (void);
extern void SetAudioMute (byte audioMute);
//------------------------------------------------------------------------------
// Function Name: siHdmiTx_Init()
// Function Description: Set the 9022/4 video and video.
//
// Accepts: none
// Returns: none
// Globals: siHdmiTx
//------------------------------------------------------------------------------
void siHdmiTx_Init (void)
{
    APP_PRINT(">>siHdmiTx_Init()\r\n");

    // workaround for Bug#18128
    if (siHdmiTx.ColorDepth == VMD_COLOR_DEPTH_8BIT)
    {
        // Yes it is, so force 16bpps first!
        siHdmiTx.ColorDepth = VMD_COLOR_DEPTH_16BIT;
        InitVideo(siHdmiTx.TclkSel);
        // Now put it back to 8bit and go do the expected InitVideo() call
        siHdmiTx.ColorDepth = VMD_COLOR_DEPTH_8BIT;
    }

    // end workaround

    InitVideo(siHdmiTx.TclkSel);			// Set PLL Multiplier to x1 upon power up

    siHdmiTx_PowerStateD0();

    if (IsHDMI_Sink())					// Set InfoFrames only if HDMI output mode
    {
        SetAVI_InfoFrames();
        siHdmiTx_AudioSet();			// set audio interface to basic audio (an external command is needed to set to any other mode
    }
    else
    {
        SetAudioMute(AUDIO_MUTE_MUTED);
    }

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!

// THIS PATCH IS NEEDED BECAUSE SETTING UP AVI InfoFrames CLEARS 0x63 and 0x60[5]
    if (siHdmiTx.ColorSpace == YCBCR422_8BITS)
        ReadSetWriteTPI(TPI_SYNC_GEN_CTRL, BIT_5);       // Set 0x60[5] according to input color space.

// THIS PATCH IS NEEDED BECAUSE SETTING UP AVI InfoFrames CLEARS 0x63
    WriteByteTPI(TPI_DE_CTRL, TPI_REG0x63_SAVED);

// PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!PATCH!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    //==========================================================
    WriteByteTPI(TPI_YC_Input_Mode, 0x00);

    {
        APP_PRINT("TMDS -> Enabled\r\n");
        ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, LINK_INTEGRITY_MODE_MASK | TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK,
                           LINK_INTEGRITY_DYNAMIC | TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_NORMAL);

        WriteByteTPI(TPI_PIX_REPETITION, tpivmode[0]);      		// Write register 0x08
        g_sys.tmdsPoweredUp = TRUE;
        EnableInterrupts(HOT_PLUG_EVENT | RX_SENSE_EVENT | AUDIO_ERROR_EVENT);
    }
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSet()
// Function Description: Set the 9022/4 video resolution
//
// Accepts: none
// Returns: Success message if video resolution changed successfully.
//                  Error Code if resolution change failed
// Globals: siHdmiTx
//------------------------------------------------------------------------------
//============================================================
#define T_RES_CHANGE_DELAY      128         // delay between turning TMDS bus off and changing output resolution

byte siHdmiTx_VideoSet (void)
{
    APP_PRINT(">>siHdmiTx_VideoSet()\r\n");

    // Note: this's necessary for fixing 480i_13.5MHz to 1080p_148.5MHz no display issue.
    siHdmiTx_TPI_Init();
    g_sys.hdmiCableConnected = TRUE;
    g_sys.dsRxPoweredUp = TRUE;

    /*
    #ifdef DEV_SUPPORT_HDCP
        	HDCP_Off();
    #endif

        	DisableTMDS();                  // turn off TMDS output
        	DelayMS(T_RES_CHANGE_DELAY);    // allow control InfoFrames to pass through to the sink device.
    */
    siHdmiTx_Init();

    #ifdef HW_INT_ENABLE

    #endif

    return VIDEO_MODE_SET_OK;
}

//------------------------------------------------------------------------------
// Function Name: SetAudioInfoFrames()
// Function Description: Load Audio InfoFrame data into registers and send to sink
//
// Accepts: (1) Channel count
//              (2) speaker configuration per CEA-861D Tables 19, 20
//              (3) Coding type: 0x09 for DSD Audio. 0 (refer to stream header) for all the rest
//              (4) Sample Frequency. Non zero for HBR only
//              (5) Audio Sample Length. Non zero for HBR only.
// Returns: TRUE
// Globals: none
//------------------------------------------------------------------------------
byte SetAudioInfoFrames(byte ChannelCount, byte CodingType, byte SS, byte Fs, byte SpeakerConfig)
{
    byte B_Data[SIZE_AUDIO_INFOFRAME];  // 14
    byte i;
    byte TmpVal = 0;

    APP_PRINT(">>SetAudioInfoFrames()\r\n");

    for (i = 0; i < SIZE_AUDIO_INFOFRAME; i++)
        B_Data[i] = 0;

    WriteByteTPI(MISC_INFO_FRAMES_CTRL, DISABLE_AUDIO);         //  Disbale MPEG/Vendor Specific InfoFrames

    B_Data[0] = TYPE_AUDIO_INFOFRAMES;      // 0x84
    B_Data[1] = AUDIO_INFOFRAMES_VERSION;   // 0x01
    B_Data[2] = AUDIO_INFOFRAMES_LENGTH;    // 0x0A
    B_Data[3] = TYPE_AUDIO_INFOFRAMES + 		// Calculate checksum - 0x84 + 0x01 + 0x0A
                AUDIO_INFOFRAMES_VERSION +
                AUDIO_INFOFRAMES_LENGTH;

    B_Data[4] = ChannelCount;               // 0 for "Refer to Stream Header" or for 2 Channels. 0x07 for 8 Channels
    B_Data[4] |= (CodingType << 4);                 // 0xC7[7:4] == 0b1001 for DSD Audio

    B_Data[5] = ((Fs & THREE_LSBITS) << 2) | (SS & TWO_LSBITS);

    B_Data[7] = SpeakerConfig;

    for (i = 4; i < SIZE_AUDIO_INFOFRAME; i++)
        B_Data[3] += B_Data[i];

    B_Data[3] = 0x100 - B_Data[3];

    WriteByteTPI(MISC_INFO_FRAMES_CTRL, EN_AND_RPT_AUDIO);         // Re-enable Audio InfoFrame transmission and repeat

    WriteBlockTPI(MISC_INFO_FRAMES_TYPE, SIZE_AUDIO_INFOFRAME, B_Data);

    if (siHdmiTx.SyncMode == EMBEDDED_SYNC)
        EnableEmbeddedSync();

    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: SetAudioMute()
// Function Description: Mute audio
//
// Accepts: Mute or unmute.
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void SetAudioMute (byte audioMute)
{
    ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG, AUDIO_MUTE_MASK, audioMute);
}

#ifndef F_9022A_9334
//------------------------------------------------------------------------------
// Function Name: SetChannelLayout()
// Function Description: Set up the Channel layout field of internal register 0x2F (0x2F[1])
//
// Accepts: Number of audio channels: "0 for 2-Channels ."1" for 8.
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void SetChannelLayout (byte Count)
{
    // Indexed register 0x7A:0x2F[1]:
    WriteByteTPI(TPI_INTERNAL_PAGE_REG, 0x02); // Internal page 2
    WriteByteTPI(TPI_INDEXED_OFFSET_REG, 0x2F);

    Count &= THREE_LSBITS;

    if (Count == TWO_CHANNEL_LAYOUT)
    {
        // Clear 0x2F[1]:
        ReadClearWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
    }

    else if (Count == EIGHT_CHANNEL_LAYOUT)
    {
        // Set 0x2F[1]:
        ReadSetWriteTPI(TPI_INDEXED_VALUE_REG, BIT_1);
    }
}
#endif

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_AudioSet()
// Function Description: Set the 9022/4 audio interface to basic audio.
//
// Accepts: none
// Returns: Success message if audio changed successfully.
//                  Error Code if resolution change failed
// Globals: siHdmiTx
//------------------------------------------------------------------------------
byte siHdmiTx_AudioSet (void)
{
    APP_PRINT(">>siHdmiTx_AudioSet()\r\n");

    SetAudioMute(AUDIO_MUTE_MUTED);  // mute output

    if (siHdmiTx.AudioMode == AMODE_I2S)  	// I2S input
    {
        ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG, AUDIO_SEL_MASK, AUD_IF_I2S);	// 0x26 = 0x80
        WriteByteTPI(TPI_AUDIO_HANDLING, 0x08 | AUD_DO_NOT_CHECK);	  // 0x25
    }
    else									// SPDIF input
    {
        ReadModifyWriteTPI(TPI_AUDIO_INTERFACE_REG, AUDIO_SEL_MASK, AUD_IF_SPDIF);	// 0x26 = 0x40
        WriteByteTPI(TPI_AUDIO_HANDLING, AUD_PASS_BASIC);                   // 0x25 = 0x00
    }

    #ifndef F_9022A_9334

    if (siHdmiTx.AudioChannels == ACHANNEL_2CH)
        SetChannelLayout(TWO_CHANNELS);             // Always 2 channesl in S/PDIF
    else
        SetChannelLayout(EIGHT_CHANNELS);

    #else

    if (siHdmiTx.AudioChannels == ACHANNEL_2CH)
        ReadClearWriteTPI(TPI_AUDIO_INTERFACE_REG, BIT_5); // Use TPI 0x26[5] for 9022A/24A and 9334 channel layout
    else
        ReadSetWriteTPI(TPI_AUDIO_INTERFACE_REG, BIT_5); // Use TPI 0x26[5] for 9022A/24A and 9334 channel layout

    #endif

    if (siHdmiTx.AudioMode == AMODE_I2S)  	// I2S input
    {
        // I2S - Map channels - replace with call to API MAPI2S
        WriteByteTPI(TPI_I2S_EN, 0x80); // 0x1F

        if (siHdmiTx.AudioChannels > ACHANNEL_2CH)
            WriteByteTPI(TPI_I2S_EN, 0x91);

        if (siHdmiTx.AudioChannels > ACHANNEL_4CH)
            WriteByteTPI(TPI_I2S_EN, 0xA2);

        if (siHdmiTx.AudioChannels > ACHANNEL_6CH)
            WriteByteTPI(TPI_I2S_EN, 0xB3);

        // I2S - Stream Header Settings - replace with call to API SetI2S_StreamHeader
        WriteByteTPI(TPI_I2S_CHST_0, 0x00); // 0x21
        WriteByteTPI(TPI_I2S_CHST_1, 0x00);
        WriteByteTPI(TPI_I2S_CHST_2, 0x00);
        WriteByteTPI(TPI_I2S_CHST_3, siHdmiTx.AudioFs);
        WriteByteTPI(TPI_I2S_CHST_4, (siHdmiTx.AudioFs << 4) | siHdmiTx.AudioWordLength);

        // Oscar 20100929 added for 16bit auido noise issue
        WriteIndexedRegister(INDEXED_PAGE_1, AUDIO_INPUT_LENGTH, siHdmiTx.AudioWordLength);

        // I2S - Input Configuration
        WriteByteTPI(TPI_I2S_IN_CFG, siHdmiTx.AudioI2SFormat); //TPI_Reg0x20
    }

    WriteByteTPI(TPI_AUDIO_SAMPLE_CTRL, REFER_TO_STREAM_HDR);
    SetAudioInfoFrames(siHdmiTx.AudioChannels & THREE_LSBITS, REFER_TO_STREAM_HDR, REFER_TO_STREAM_HDR, REFER_TO_STREAM_HDR, 0x00);

    SetAudioMute(AUDIO_MUTE_NORMAL);  // unmute output

    return AUDIO_MODE_SET_OK;
}

#ifdef F_9022A_9334
//------------------------------------------------------------------------------
// Function Name: SetGBD_InfoFrame()
// Function Description: Sets and sends the the 9022A/4A GBD InfoFrames.
//
// Accepts: none
// Returns: Success message if GBD packet set successfully. Error
//                  Code if failed
// Globals: none
// NOTE: Currently this function is a place holder. It always returns a Success message
//------------------------------------------------------------------------------
byte SetGBD_InfoFrame (void)
{
    byte CheckSum;

    APP_PRINT(">>SetGBD_InfoFrame()\r\n");

    // Set MPEG InfoFrame Header to GBD InfoFrame Header values:
    WriteByteTPI(MISC_INFO_FRAMES_CTRL, DISABLE_MPEG);                    	// 0xBF = Use MPEG      InfoFrame for GBD - 0x03
    WriteByteTPI(MISC_INFO_FRAMES_TYPE, TYPE_GBD_INFOFRAME);                        	// 0xC0 = 0x0A
    WriteByteTPI(MISC_INFO_FRAMES_VER, NEXT_FIELD | GBD_PROFILE | AFFECTED_GAMUT_SEQ_NUM);   // 0x0C1 = 0x81
    WriteByteTPI(MISC_INFO_FRAMES_LEN, ONLY_PACKET | CURRENT_GAMUT_SEQ_NUM);                         // 0x0C2 = 0x31

    CheckSum = TYPE_GBD_INFOFRAME +
               NEXT_FIELD +
               GBD_PROFILE +
               AFFECTED_GAMUT_SEQ_NUM +
               ONLY_PACKET +
               CURRENT_GAMUT_SEQ_NUM;

    CheckSum = 0x100 - CheckSum;

    WriteByteTPI(MISC_INFO_FRAMES_CTRL, EN_AND_RPT_MPEG);  // Enable and Repeat MPEG InfoFrames
    WriteByteTPI(MISC_INFO_FRAMES_CHKSUM, CheckSum);                        // 0X00 - Send header only

    return GBD_SET_SUCCESSFULLY;
}
#endif

#ifdef DEV_SUPPORT_3D
//------------------------------------------------------------------------------
// Function Name: Set_VSIF()
// Function Description: Construct Vendor Specific InfoFrame for 3D support. use MPEG InfoFrame
//
// Accepts: none
// Returns: none
// Globals: siHdmiTx
//------------------------------------------------------------------------------
// VSIF Constants
//============================================================
#define VSIF_TYPE  			0x81
#define VSIF_VERSION		0x01
#define VSIF_LEN				0x06

void Set_VSIF (void)
{
    byte i;
    byte Data[SIZE_MPEG_INFOFRAME];		//10

    for (i = 0; i < SIZE_MPEG_INFOFRAME; i++)
    {
        Data[i] = 0;
    }

    // Disable transmission of VSIF during re-configuration
    WriteByteTPI(MISC_INFO_FRAMES_CTRL, DISABLE_MPEG);

    // Header Bytes
    Data[0] = VSIF_TYPE;            // HB0 Packet Type 0x81
    Data[1] = VSIF_VERSION; 		// HB1 Version = 0x01

    // PB1 - PB3 contain the 24bit IEEE Registration Identifier
    Data[4] = 0x03;                     // HDMI Signature LS Byte
    Data[5] = 0x0C;                     // HDMI Signature middle byte
    Data[6] = 0x00;                     // HDMI Signature MS Byte

    // PB4 - HDMI_Video_Format into bits 7:5
    Data[7] = siHdmiTx.HDMIVideoFormat << 5;

    // PB5 - Depending on the video format, this byte will contain either the HDMI_VIC
    // code in buts 7:0, OR the 3D_Structure in bits 7:4.
    switch(siHdmiTx.HDMIVideoFormat)
    {
        case VMD_HDMIFORMAT_HDMI_VIC:
            // This is a 2x4K mode, set the HDMI_VIC in buts 7:0.  Values
            // are from HDMI 1.4 Spec, 8.2.3.1 (Table 8-13).
            Data[8] = siHdmiTx.VIC;
            Data[9] = 0;
            break;

        case VMD_HDMIFORMAT_3D:
            // This is a 3D mode, set the 3D_Structure in buts 7:4
            // Bits 3:0 are reseved so set to 0.  Values are from HDMI 1.4
            // Spec, Appendix H (Table H-2).
            Data[8] = siHdmiTx.ThreeDStructure << 4;

            // Add the Extended data field when the 3D format is Side-by-Side(Half).
            // See Spec Table H-3 for details.
            if ((Data[8] >> 4) == VMD_3D_SIDEBYSIDEHALF)
            {
                Data[2] = VSIF_LEN;
                Data[9] = siHdmiTx.ThreeDExtData << 4;;
            }
            else
            {
                Data[2] = VSIF_LEN - 1;
            }

            break;

        case VMD_HDMIFORMAT_CEA_VIC:
        default:
            Data[8] = 0;
            Data[9] = 0;
            break;
    }

    // Packet Bytes
    Data[3] = VSIF_TYPE +               // PB0 partial checksum
              VSIF_VERSION +
              Data[2];

    // Complete the checksum with PB1 through PB7
    for (i = 4; i < SIZE_MPEG_INFOFRAME; i++)
    {
        Data[3] += Data[i];
    }

    // Data[3] %= 0x100;
    Data[3] = 0x100 - Data[3];            // Final checksum

    WriteByteTPI(MISC_INFO_FRAMES_CTRL, EN_AND_RPT_MPEG);         // Enable and Repeat MPEG/Vendor Specific InfoFrames

    WriteBlockTPI(MISC_INFO_FRAMES_TYPE, SIZE_MPEG_INFOFRAME, Data);             // Write VSIF to MPEG registers and start transmission
    WriteByteTPI(0xDE, 0x00);		// Set last byte of MPEG InfoFrame for data to be sent to sink.
}
#endif




///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////                  TPI                     ///////////////////////////////
///////////////////////*************************///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


//------------------------------------------------------------------------------
// Function Name: StartTPI()
// Function Description: Start HW TPI mode by writing 0x00 to TPI address 0xC7.
//
// Accepts: none
// Returns: TRUE if HW TPI started successfully. FALSE if failed to.
// Globals: none
//------------------------------------------------------------------------------
byte StartTPI (void)
{
    byte devID = 0x00;
    word wID = 0x0000;

    APP_PRINT(">>StartTPI()\r\n");

    WriteByteTPI(TPI_ENABLE, 0x00);            // Write "0" to 72:C7 to start HW TPI mode
    DelayMS(100);

    devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x03);
    wID = devID;
    wID <<= 8;
    devID = ReadIndexedRegister(INDEXED_PAGE_0, 0x02);
    wID |= devID;

    devID = ReadByteTPI(TPI_DEVICE_ID);

    APP_PRINT("0x%04X\n", (int)wID);

    if (devID == SII902XA_DEVICE_ID)
        return TRUE;

    APP_PRINT("Unsupported TX, devID = 0x%X\n", (int)devID);
    return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_TPI_Init()
// Function Description: TPI initialization: HW Reset, Interrupt enable.
//
// Accepts: none
// Returns: TRUE or FLASE
// Globals: none
//------------------------------------------------------------------------------
byte siHdmiTx_TPI_Init (void)
{
    APP_PRINT("\n>>siHdmiTx_TPI_Init()\r\n");
    APP_PRINT("\n%s\n", TPI_FW_VERSION);

    // Chip powers up in D2 mode.
    g_sys.txPowerState = TX_POWER_STATE_D0;

    InitializeStateVariables();

    // Toggle TX reset pin
    TxHW_Reset();

    // Enable HW TPI mode, check device ID
    if (StartTPI())
    {
        #ifdef DEV_SUPPORT_CEC
        //SI_CecInit();
        #endif

        EnableInterrupts(HOT_PLUG_EVENT);
        APP_PRINT("\n>>check device ID SUCCESS\r\n");

        return TRUE;
    }

    return FALSE;
}

//------------------------------------------------------------------------------
// Function Name: OnDownstreamRxPoweredDown()
// Function Description: HDMI cable unplug handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnDownstreamRxPoweredDown (void)
{
    APP_PRINT("DSRX -> Powered Down\r\n");
    g_sys.dsRxPoweredUp = FALSE;

    DisableTMDS();
    ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);	// Set to DVI output mode to reset HDCP
}

extern void HotPlugService(void);
//------------------------------------------------------------------------------
// Function Name: OnDownstreamRxPoweredUp()
// Function Description: DSRX power up handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnDownstreamRxPoweredUp (void)
{
    APP_PRINT("DSRX -> Powered Up\r\n");
    g_sys.dsRxPoweredUp = TRUE;

    HotPlugService();
}

//------------------------------------------------------------------------------
// Function Name: OnHdmiCableDisconnected()
// Function Description: HDMI cable unplug handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnHdmiCableDisconnected (void)
{
    APP_PRINT("HDMI Disconnected\r\n");

    g_sys.hdmiCableConnected = FALSE;

    #ifdef DEV_SUPPORT_EDID
    g_edid.edidDataValid = FALSE;
    #endif

    OnDownstreamRxPoweredDown();
}

//------------------------------------------------------------------------------
// Function Name: OnHdmiCableConnected()
// Function Description: HDMI cable plug in handle.
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void OnHdmiCableConnected (void)
{
    APP_PRINT("Cable Connected\n");
    // No need to call TPI_Init here unless TX has been powered down on cable removal.
    //TPI_Init();

    g_sys.hdmiCableConnected = TRUE;

    // Added for EDID read for Michael Wang recommaned by oscar 20100908
    //siHdmiTx_PowerStateD0();
    //ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_MUTED);

    #ifdef DEV_SUPPORT_EDID
    DoEdidRead();
    #endif

    #ifdef READKSV
    ReadModifyWriteTPI(0xBB, 0x08, 0x08);
    #endif

    if (IsHDMI_Sink())              // select output mode (HDMI/DVI) according to sink capabilty
    {
        APP_PRINT("HDMI Sink Detected\r\n");
        ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_HDMI);
    }
    else
    {
        APP_PRINT("DVI Sink Detected\r\n");
        ReadModifyWriteTPI(TPI_SYSTEM_CONTROL_DATA_REG, OUTPUT_MODE_MASK, OUTPUT_MODE_DVI);
    }
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD0()
// Function Description: Set TX to D0 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD0 (void)
{
    ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_MASK, TX_POWER_STATE_D0);
    APP_PRINT("TX Power State D0\r\n");
    g_sys.txPowerState = TX_POWER_STATE_D0;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD2()
// Function Description: Set TX to D2 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD2 (void)
{
    ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_MASK, TX_POWER_STATE_D2);
    APP_PRINT("TX Power State D2\r\n");
    g_sys.txPowerState = TX_POWER_STATE_D2;
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_PowerStateD0fromD2()
// Function Description: Set TX to D0 mode from D2 mode.
//------------------------------------------------------------------------------
void siHdmiTx_PowerStateD0fromD2 (void)
{
    ReadModifyWriteTPI(TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_MASK, TX_POWER_STATE_D0);

    EnableTMDS();

    APP_PRINT("TX Power State D0 from D2\r\n");
    g_sys.txPowerState = TX_POWER_STATE_D0;
}

//------------------------------------------------------------------------------
// Function Name: HotPlugService()
// Function Description: Implement Hot Plug Service Loop activities
//
// Accepts: none
// Returns: An error code that indicates success or cause of failure
// Globals: LinkProtectionLevel
//------------------------------------------------------------------------------
void HotPlugService (void)
{
    APP_PRINT(">>HotPlugService()\r\n");

    DisableInterrupts(0xFF);

    //siHdmiTx.VIC = g_edid.VideoDescriptor[0];	// use 1st mode supported by sink

    siHdmiTx_Init();
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_TPI_Poll()
// Function Description: Poll Interrupt Status register for new interrupts
//
// Accepts: none
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_TPI_Poll (void)
{
    byte InterruptStatus;

    #ifdef HW_INT_ENABLE

    if ((g_sys.txPowerState == TX_POWER_STATE_D0) && (!INT_PIN))
    #else
    if (g_sys.txPowerState == TX_POWER_STATE_D0)
    #endif
    {
        InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);

        if (InterruptStatus & HOT_PLUG_EVENT)
        {
            APP_PRINT("HPD  -> ");
            ReadSetWriteTPI(TPI_INTERRUPT_ENABLE_REG, HOT_PLUG_EVENT);  // Enable HPD interrupt bit

            // Repeat this loop while cable is bouncing:
            do
            {
                WriteByteTPI(TPI_INTERRUPT_STATUS_REG, HOT_PLUG_EVENT);
                DelayMS(T_HPD_DELAY); // Delay for metastability protection and to help filter out connection bouncing
                InterruptStatus = ReadByteTPI(TPI_INTERRUPT_STATUS_REG);    // Read Interrupt status register
            }
            while (InterruptStatus & HOT_PLUG_EVENT);                // loop as long as HP interrupts recur

            if (((InterruptStatus & HOT_PLUG_STATE) >> 2) != g_sys.hdmiCableConnected)
            {
                if (g_sys.hdmiCableConnected == TRUE)
                {
                    OnHdmiCableDisconnected();
                }
                else
                {
                    OnHdmiCableConnected();
                    ReadModifyWriteIndexedRegister(INDEXED_PAGE_0, 0x0A, 0x08, 0x08);
                }

                if (g_sys.hdmiCableConnected == FALSE)
                {
                    return;
                }
            }
        }

        // Check rx power
        if (((InterruptStatus & RX_SENSE_STATE) >> 3) != g_sys.dsRxPoweredUp)
        {
            if (g_sys.hdmiCableConnected == TRUE)
            {
                if (g_sys.dsRxPoweredUp == TRUE)
                {
                    OnDownstreamRxPoweredDown();
                }
                else
                {
                    OnDownstreamRxPoweredUp();
                }
            }

            ClearInterrupt(RX_SENSE_EVENT);
        }

        // Check if Audio Error event has occurred:
        if (InterruptStatus & AUDIO_ERROR_EVENT)
        {
            APP_PRINT("TP -> Audio Error Event\r\n");
            //  The hardware handles the event without need for host intervention (PR, p. 31)
            ClearInterrupt(AUDIO_ERROR_EVENT);
        }


        #ifdef DEV_SUPPORT_CEC
        //SI_CecHandler(0 , 0);
        #endif
    }

    //mod_timer(&sil902x_timer, jiffies + TIMER_DELAY);
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSel()
// Function Description: Select output video mode
//
// Accepts: Video mode
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_VideoSel (byte vmode)
{
    siHdmiTx.HDMIVideoFormat 	= VMD_HDMIFORMAT_CEA_VIC;
    //siHdmiTx.ColorSpace 	    = YCBCR422_16BITS;//RGB;
	siHdmiTx.ColorSpace 	    = RGB;//RGB;
    siHdmiTx.ColorDepth			= VMD_COLOR_DEPTH_8BIT;
    siHdmiTx.SyncMode 			= EMBEDDED_SYNC;//EXTERNAL_HSVSDE;

    //siHdmiTx.SyncMode 			= EXTERNAL_HSVSDE;
    switch (vmode)
    {
        case HDMI_480I60_4X3:
            APP_PRINT("HDMI_480I60_4X3\n");
            siHdmiTx.VIC 		= 6;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
            siHdmiTx.Colorimetry	= COLORIMETRY_601;
            siHdmiTx.TclkSel		= X2;
            break;

        case HDMI_576I50_4X3:
            APP_PRINT("HDMI_576I50_4X3\n");
            siHdmiTx.VIC 		= 21;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
            siHdmiTx.Colorimetry	= COLORIMETRY_601;
            siHdmiTx.TclkSel		= X2;
            break;

        case HDMI_480P60_4X3:
            APP_PRINT("HDMI_480P60_4X3\n");
            siHdmiTx.VIC 		= 2;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
            siHdmiTx.Colorimetry	= COLORIMETRY_601;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_576P50_4X3:
            APP_PRINT("HDMI_576P50_4X3\n");
            siHdmiTx.VIC 		= 17;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_4x3;
            siHdmiTx.Colorimetry	= COLORIMETRY_601;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_720P60:
            APP_PRINT("HDMI_720P60\n");
            siHdmiTx.VIC 		= 4;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_720P50:
            APP_PRINT("HDMI_720P50\n");
            siHdmiTx.VIC 		= 19;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080I60:
            APP_PRINT("HDMI_1080I60\n");
            siHdmiTx.VIC 		= 5;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080I50:
            APP_PRINT("HDMI_1080I50\n");
            siHdmiTx.VIC 		= 20;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080P60:
            APP_PRINT("HDMI_1080P60\n");
            siHdmiTx.VIC 		= 16;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080P50:
            APP_PRINT("HDMI_1080P50\n");
            siHdmiTx.VIC 		= 31;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        //zhy+ begin
        case HDMI_1080P25:
            APP_PRINT("HDMI_1080P25\n");
            siHdmiTx.VIC 		= 33;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080P24:
            APP_PRINT("HDMI_1080P24\n");
            siHdmiTx.VIC 		= 32;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        case HDMI_1080P30:
            APP_PRINT("HDMI_1080P30\n");
            siHdmiTx.VIC 		= 34;
            siHdmiTx.AspectRatio 	= VMD_ASPECT_RATIO_16x9;
            siHdmiTx.Colorimetry	= COLORIMETRY_709;
            siHdmiTx.TclkSel		= X1;
            break;

        //zhy+ end
        default:
            APP_PRINT("Havn't the mode:%02X\n", vmode);
            break;
    }
}

//------------------------------------------------------------------------------
// Function Name: siHdmiTx_AudioSel()
// Function Description: Select output audio mode
//
// Accepts: Audio Fs
// Returns: none
// Globals: none
//------------------------------------------------------------------------------

void siHdmiTx_AudioSel (byte Afs)
{
    siHdmiTx.AudioMode			= AMODE_I2S;
    siHdmiTx.AudioChannels		= ACHANNEL_2CH;
    siHdmiTx.AudioFs				= Afs;
    siHdmiTx.AudioWordLength		= ALENGTH_16BITS;
    siHdmiTx.AudioI2SFormat		= SCK_SAMPLE_RISING_EDGE | 0x00;
}



/*
 *  The various file operations we support.
 */

void gpio_i2c_write(char a, char b)
{
    // HAL_I2C_Mem_Write(&hi2c4, a, b, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&c, 1, 300000);
    uint8_t buf[2];
    buf[0] = a;
    buf[1] = b;
    g_i2c_master1.p_api->slaveAddressSet(g_i2c_master1.p_ctrl, 0x39, 1);
    g_i2c_master1.p_api->write(&g_i2c_master1_ctrl, &buf, 2, false);
    I2C1WaitTxCplt();
}

char gpio_i2c_read( char a)
{
    char c;
    uint8_t rbuf1[1];
    uint8_t buf[1];
    buf[0] = a;
    g_i2c_master1.p_api->slaveAddressSet(g_i2c_master1.p_ctrl, 0x39, 1);
    g_i2c_master1.p_api->write(&g_i2c_master1_ctrl, &buf, 1, true);
    I2C1WaitTxCplt();
    // R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MICROSECONDS);
    /* 读取该地址的一个字节数据 */
    g_i2c_master1.p_api->read(g_i2c_master1.p_ctrl, rbuf1, (unsigned int)1, false);
    I2C1WaitRxCplt();
    APP_PRINT("0x63 = 0x%x\n", rbuf1[0]);
    
    return rbuf1[0];
}




void sil902x_test(void)
{
    //HW Reset
    gpio_i2c_write(0x82, 0x25);
    gpio_i2c_write(0x08, 0x36);
    gpio_i2c_write(0x7c, 0x14);
    gpio_i2c_write(0xc7, 0x00);
    DelayMS(100);

    //9022 PLL Set:
    gpio_i2c_write(0xBC, 0x01);
    gpio_i2c_write(0xBD, 0x82);
    gpio_i2c_write(0xBE, 0x35);

    //9022A PLL Set
    gpio_i2c_write(0x08, 0x70);

    gpio_i2c_write(0x00, 0x8C);
    gpio_i2c_write(0x01, 0x0A);
    gpio_i2c_write(0x02, 0x70);
    gpio_i2c_write(0x03, 0x17);
    gpio_i2c_write(0x04, 0x5A);
    gpio_i2c_write(0x05, 0x03);
    gpio_i2c_write(0x06, 0x0D);
    gpio_i2c_write(0x07, 0x02);

    gpio_i2c_write(0x63, 0x00);	//bit6=1 for DE

    //Color Space
    gpio_i2c_write(0x08, 0x70);
    gpio_i2c_write(0x09, 0x00);
    gpio_i2c_write(0x0A, 0x00);

    gpio_i2c_write(0x60, 0x04);

    gpio_i2c_write(0xBC, 0x01);
    gpio_i2c_write(0xBD, 0x82);
    gpio_i2c_write(0xBE, 0x35);

    //Power Up
    gpio_i2c_write(0x1E, 0x00);

    //Set AVI
    //gpio_i2c_write(0x72,0x0C,14);
    #if 1
    gpio_i2c_write(0x09, 0x00);	//  ---Y422 input

    gpio_i2c_write(0x0A, 0x00);	// ---RGB output

    gpio_i2c_write(0x19, 0x00);	// ---enable color convert, must do it here

    gpio_i2c_write(0x60, 0x80);	// ---embeded sync select

    gpio_i2c_write(0x62, 0x58);	// ---HBIT_2_HSYNC L

    gpio_i2c_write(0x63, 0x40);	// ---embede enable and HBIT_2_HSYNC M

    gpio_i2c_write(0x64, 0x4C);	// ---FIELD2_OFST L

    gpio_i2c_write(0x65, 0x04);	// ---FIELD2_OFST M

    gpio_i2c_write(0x66, 0x2C);	// ---HWIDTH L

    gpio_i2c_write(0x67, 0x00);	// ---HWIDTH M

    gpio_i2c_write(0x68, 0x02);	// ---VBIT_2_VSYNC

    gpio_i2c_write(0x69, 0x05);	// ---VWIDTH
    #endif


    //Enable TMDS
    gpio_i2c_write(0x1A, 0x00);

    return;

}
int sil902x_device_init(void)
{
    
    siHdmiTx_TPI_Init();
    //siHdmiTx_VideoSel(norm);
    siHdmiTx_VideoSel(HDMI_720P60);

    siHdmiTx_AudioSel( AFS_44K1 );
    // sil902x_test();

    // add by z39183, drv cfg for embeded sync mode miss the following cfg.
    // HAL_Delay(1000); // why? I have to add the time because the following I2C is invalid without it. 9024 need to be stable??
    R_BSP_SoftwareDelay (1000, BSP_DELAY_UNITS_MILLISECONDS);

    gpio_i2c_write(0x63, 0x40);
    // R_BSP_SoftwareDelay (1000, BSP_DELAY_UNITS_MILLISECONDS);
    APP_PRINT("0x63 = 0x%x\n", gpio_i2c_read( 0x63));


    return 0;
}

static int sil902x_init(void)
{
    volatile unsigned int ret = 0;



    if(ret)
    {
        DPRINTK(0, "could not register sil902x device");
        return -1;
    }

    //sil902x_test();
    //return ret;
    if(sil902x_device_init() < 0)
    {
    }

    return ret;
}


//module_param(norm, uint, S_IRUGO);
//MODULE_PARM_DESC(norm, "5:720p@60, 6:720p@50, 7:1080i@60, 8:1080i@50, 9:1080p@60(default), 10:1080p@50, 11:1080p@25, 12:1080p@30");



