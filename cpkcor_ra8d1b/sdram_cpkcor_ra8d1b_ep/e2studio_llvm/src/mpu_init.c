/*
 * mpu_init.c
 *
 *  Created on: Feb 16, 2024
 *      Author: a5125834
 */
#include "hal_data.h"
#include "core_cm85.h"


#define Attr0               (0b01110111) //Normal memory, Write-Back Transient, Read allocate, Write allocate
#define Attr1               (0b00100010) //Normal memory, Write-Through transient, Read allocate
#define Attr2               (0b00000000) //Device-nGnRnE

#define Attr3               (0b11111111) //Normal memory, Write-Back non-Transient, Read allocate, Write allocate
#define Attr4               (0b10101010) //Normal memory, Write-Through non-transient, Read allocate

void mpu_direct_config(void)
{

    MPU->CTRL = 0x0;
    __DSB();
    __ISB();

    // Enable the MPU
    // change Attr0 -> Attrx to change cache write-back or write-though
    MPU->MAIR1 = ((Attr1  &  MPU_MAIR1_Attr4_Msk)   |
                (Attr1 << MPU_MAIR1_Attr5_Pos));

    // Region 4 SRAM
    MPU->RNR = 4;
    // Region 4 Base address
    //MPU->RBAR = ((MPU_SRAM_BASE_SC & MPU_RBAR_ADDR_Msk) |
    MPU->RBAR = ((0x22000000) |
               (0b00 << MPU_RBAR_SH_Pos)              |  // non-Shareability   - Non-shareable
               (0b01 << MPU_RBAR_AP_Pos)              |  // Access permissions - Read/write by any privilege level
               (0b0  << MPU_RBAR_XN_Pos));               // Execute never      - Execution only permitted if read permitted
    // Region 4 Limit register
    MPU->RLAR = ((0x220DFFFF & MPU_RLAR_LIMIT_Msk) | // Size
               (0b100 << MPU_RLAR_AttrIndx_Pos)         | // AttrIndex - 4
               (0b1   << MPU_RLAR_EN_Pos));               // Enable    - On

    // Region 5 SDRAM
    MPU->RNR = 5;
    // Region 5 Base address
    MPU->RBAR = ((0x68000000) |
               (0b00 << MPU_RBAR_SH_Pos)              |  // non-Shareability   - Non-shareable
               (0b01 << MPU_RBAR_AP_Pos)              |  // Access permissions - Read/write by any privilege level
               (0b0  << MPU_RBAR_XN_Pos));               // Execute never      - Execution only permitted if read permitted
    // Region 5 Limit register
    MPU->RLAR = ((0x6FFFFFFF & MPU_RLAR_LIMIT_Msk) | // Size
               (0b101 << MPU_RLAR_AttrIndx_Pos)         | // AttrIndex - 5
               (0b1   << MPU_RLAR_EN_Pos));               // Enable    - On

    MPU->CTRL = 0x5;
    __DSB();
    __ISB();
}
