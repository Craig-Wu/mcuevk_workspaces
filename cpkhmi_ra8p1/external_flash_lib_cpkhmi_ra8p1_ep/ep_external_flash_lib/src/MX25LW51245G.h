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
 * @file          MX25LW51245G.h
 * @Version       1.0
 * @brief         Header file including information of external flash device.
 *********************************************************************************************************************/
/**********************************************************************************************************************
 * History : DD.MM.YYYY Version  Description
 *         : 01.01.2026 1.00     First Release
 *********************************************************************************************************************/

/**********************************************************************************************************************
 Macro definitions
 *********************************************************************************************************************/
#ifndef MX25LW51245G_H_
#define MX25LW51245G_H_

// Memory size
#define MEMORY_SIZE (0x02000000)

// Sector and Block parameter
#define BLOCK_64KB_SIZE (0x00010000)
#define BLOCK_64KB_NUM  (0x400)

// Manufacturer ID and Device ID
#define MANUFACTURER_ID       (0xEF)
#define DEVICE_ID_MEM_TYPE    (0x40)
#define DEVICE_ID_MEM_DENSITY (0x19)

// Command code for SPI protocol
#define SPI_COMMAND_NORMAL_READ                  (0x13)
#define SPI_COMMAND_PROGRAM_PAGE                 (0x12)
#define SPI_COMMAND_BLOCK_ERASE                  (0xDC)
#define SPI_COMMAND_CHIP_ERASE                   (0x60)
#define SPI_COMMAND_WRITE_ENABLE                 (0x06)
#define SPI_COMMAND_READ_DEVICE_ID               (0x9F)
#define SPI_COMMAND_READ_STATUS_REGISTER         (0x05)
#define SPI_COMMAND_READ_CONFIG_REGISTER         (0x15)
#define SPI_COMMAND_READ_CONFIG_REGISTER2        (0x71)
#define SPI_COMMAND_WRITE_CONFIG_REGISTER2       (0x72)
#define SPI_COMMAND_READ_SECURITY_REGISTER       (0x2B)

// Configuration register volatile address
#define CONFIG_REG2_ADDRESS1 (0x00000000)  // Specify protocol mode

// Status register field
#define STATUS_REG_WIP_MSK (0x01)
#define STATUS_REG_WEL_MSK (0x02)

// Configuration register volatile field
#define CONFIG_REG2_SOPI_DOPI_MSK   (0x03)

// Security register field
#define SECURITY_REG_ERASE_FAIL_MSK   (0x40)
#define SECURITY_REG_PROGRAM_FAIL_MSK (0x20)

#endif  /* MX25LW51245G_H_ */
