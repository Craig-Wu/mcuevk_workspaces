/**********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO
 * THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2025 Renesas Electronics Corporation. All rights reserved.
 *********************************************************************************************************************/
/******************************************************************************************************************//**
 * @file          LoaderInfo.c
 * @version       1.00
 * @brief         Implements information struct declared in LoaderInfo.h for external flash programming.
 *********************************************************************************************************************/
/**********************************************************************************************************************
 * History : DD.MM.YYYY Version  Description
 *         : 01.01.2026 1.00     First Release
 *********************************************************************************************************************/

/**********************************************************************************************************************
 Includes   <System Includes> , "Project Includes"
 *********************************************************************************************************************/
#include "LoaderInfo.h"

/**********************************************************************************************************************
 Impletent struct variables
 *********************************************************************************************************************/
__attribute__((section("loader_info")))
struct LoaderInfo const g_loader_info = {
    /* fileVersion */           0x0001,
    /* formatVersion */         0x0001,
    /* funcOffset */            0x00100000,
    /* funcSize */              0x00037000,
    /* name */                  "sample",
    /* devName */               "R7KA8P1KFLCAC",
    /* extFlashDevName */       "MX25LW51245G",
    /* devAdr */                0x80000000,
    /* extFlashSize */          0x02000000,
    /* pageSize */              0x00000100,
    /* paramType */             PARAM_TYPE_RA8_BFW,
    /* parameters */            { 0x00000001, 0x00000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    /* timeoutProg */           50000,
    /* timeoutErase */          100000,
    /* reserved */              { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    /* sectors */               { { 0x10000, 512 }, { 0xFFFFFFFF, 0xFFFFFFFF } }
};
