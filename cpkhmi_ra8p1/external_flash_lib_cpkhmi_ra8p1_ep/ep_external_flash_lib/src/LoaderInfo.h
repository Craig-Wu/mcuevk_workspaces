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
 * @file          LoaderInfo.h
 * @Version       1.0
 * @brief         Header file including information struct definitions for external flash programming.
 *********************************************************************************************************************/
/**********************************************************************************************************************
 * History : DD.MM.YYYY Version  Description
 *         : 01.01.2026 1.00     First Release
 *********************************************************************************************************************/

/**********************************************************************************************************************
 Macro definitions
 *********************************************************************************************************************/
#ifndef LOADER_INFO_H_
#define LOADER_INFO_H_

#define PARAM_TYPE_RA8_BFW      (0x00000001)

/**********************************************************************************************************************
 Includes   <System Includes> , "Project Includes"
 *********************************************************************************************************************/
#include <stdint.h>

/**********************************************************************************************************************
 Global struct definitions
 *********************************************************************************************************************/
struct __attribute__((packed)) Sector
{
    uint32_t size;                      // Sector size in bytes
    uint32_t num;                       // Number of sectors
};

struct __attribute__((packed)) LoaderInfo {
    uint16_t    fileVersion;            // Library file version (for user)
    uint16_t    formatVersion;          // Library file format version
    uint32_t    funcOffset;             // Offset address for driver code
    uint32_t    funcSize;               // Max code size for driver code
    char        name[64];               // Name of flash loader
    char        devName[64];            // Name of MCU
    char        extFlashDevName[64];    // Name of external flash device
    uint32_t    devAdr;                 // External flash base address on MCU
    uint32_t    extFlashSize;           // External flash size
    uint32_t    pageSize;               // Program Page size
    uint32_t    paramType;              // Parameter type
    uint32_t    parameters[8];          // Parameters
    uint32_t    timeoutProg;            // Program timeout duration (ms)
    uint32_t    timeoutErase;           // Erase timeout duration (ms)
    uint32_t    reserved[4];            // Reserved
    struct Sector sectors[32];          // Sector information for erasure
};

#endif /* LOADER_INFO_H_ */