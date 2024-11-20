/*
* Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include "hal_data.h"
#include "board_sdram.h"
#include <stdio.h>
FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

#define DCACHE_Enable   0//0
#define DTCM_Used       0


//extern void mpu_direct_config(void);
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


volatile uint32_t DWT_pre_count=0, DWT_post_count=0, time_sdram_access=0;
volatile uint32_t DWT_delta=0;

#define SDRAM_EXAMPLE_DATALEN    4*1024

volatile uint32_t SRAM_write_buff_Cache[SDRAM_EXAMPLE_DATALEN];// BSP_PLACE_IN_SECTION(".nocache");
volatile uint32_t SRAM_read_buff_Cache[SDRAM_EXAMPLE_DATALEN];// BSP_PLACE_IN_SECTION(".nocache");
volatile uint32_t SRAM_write_buff_Nocache[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".nocache");
volatile uint32_t SRAM_read_buff_Nocache[SDRAM_EXAMPLE_DATALEN]  BSP_PLACE_IN_SECTION(".nocache");

volatile uint32_t dtcm_write_buffer[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".dtcm_data");
volatile uint32_t dtcm_read_buffer[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".dtcm_data");
//uint32_t dtcm_read_buff[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".dtcm_data");
//uint32_t sdram_read_buff[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".sdram");
//uint32_t sdram_write_buff[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".sdram");

volatile uint32_t sdram_cache[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".sdram");
volatile uint32_t sdram_nocache[SDRAM_EXAMPLE_DATALEN] BSP_PLACE_IN_SECTION(".nocache_sdram");

/* SRAM<--->SDRAM 读写测试，不开cache  */
void SDRAMReadWrite32Bit_SRAM_NonCache(void) ;
/* SRAM<--->SDRAM 读写测试，开cache  */
void SDRAMReadWrite32Bit_SRAM_Cache(void) ;
/* DTCM<--->SDRAM 读写测试，不开cache  */
void SDRAMReadWrite32Bit_DTCM_NonCache(void);
/* DTCM<--->SDRAM 读写测试，开cache  */
void SDRAMReadWrite32Bit_DTCM_Cache(void);

/*******************************************************************************
 *
 ******************************************************************************/
#define EXAMPLE_SDRAM_START_ADDRESS (0x68000000U)
#define EXAMPLE_DTCM_START_ADDRESS  (0x20000000U)
#define EXAMPLE_SRAM_START_ADDRESS  (0x220B0000U)



/* SRAM<--->SDRAM 写测试，SRAM cache<-->SDRAM cache, SRAM cache<-->SDRAM Non cache, SRAM Non cache<-->SDRAM Non cache  */
void SDRAMWrite32Bit_test(void)
{
       uint32_t index;
       uint32_t datalen = SDRAM_EXAMPLE_DATALEN ;
//       uint32_t *sdram  = (uint32_t *)sdram_nocache;//EXAMPLE_SDRAM_START_ADDRESS; /* SDRAM start address. */
//       uint32_t *sdram  = (uint32_t *)EXAMPLE_SDRAM_START_ADDRESS; /* SDRAM start address. */

//       uint32_t *dtcm_buffer  = (uint32_t *)EXAMPLE_DTCM_START_ADDRESS;
//       uint32_t *write_buff = (uint32_t *)EXAMPLE_SRAM_START_ADDRESS;
       printf("##############################################################\r\n");
       printf("##############################################################\r\n");
       printf("##############################################################\r\n");
       printf("**********Write to SDRAM! **********\r\n");
       printf("**********SRAM cacheable SDRAM cacheable Start! **********\r\n");

       memset((uint32_t *)SRAM_write_buff_Cache, 0 ,datalen);
       memset((uint32_t *)sdram_cache, 0 ,datalen);
       memset((uint32_t *)SRAM_write_buff_Nocache, 0 ,datalen);
       memset((uint32_t *)sdram_nocache, 0 ,datalen);
       for(uint16_t i=0; i<datalen; i++)
       {
           SRAM_write_buff_Cache[i] = i;
           SRAM_write_buff_Nocache[i] = i;
       }
       for (index = 0; index < datalen; index++)
       {
#if DTCM_Used
        dtcm_buffer[index]  = SRAM_write_buff[index];       //index;
//        sdram_write_buff[index] = write_buff[index];
#else

#endif
       }

       DWT_init();
       DWT_clean_count();
       DWT_pre_count = DWT_get_count();

       for (index = 0; index < datalen; index++)
       {
#if DTCM_Used
        sdram[index]  = dtcm_buffer[index];  //source 为 DTCM
//        sdram_read_buff[index] = sdram_write_buff[index];
#else
        sdram_cache[index]  = SRAM_write_buff_Cache[index];  //source 为 SRAM
#endif
       }

       DWT_post_count = DWT_get_count();
       DWT_delta = DWT_post_count - DWT_pre_count;
       if(DWT_delta==0)
       {
           printf("DWT count error! \r\n");
       }
       time_sdram_access = DWT_count_to_us(DWT_delta);

       printf("sdram write DWT count:%d \r\n", DWT_delta);
       printf("sdram write time:%dus \r\n", time_sdram_access);
       printf("sram cache sdram cache write speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
       if(time_sdram_access==0)
       {
           while(1){;}
       }

       printf("**********SRAM cacheable SDRAM cacheable End! **********\r\n");

       R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);


       printf("**********SRAM cacheable SDRAM Non cacheable Start! **********\r\n");
       DWT_init();
       DWT_clean_count();
       DWT_pre_count = DWT_get_count();

       for (index = 0; index < datalen; index++)
       {
#if DTCM_Used
        sdram[index]  = dtcm_buffer[index];  //source 为 DTCM
//        sdram_read_buff[index] = sdram_write_buff[index];
#else
        sdram_nocache[index]  = SRAM_write_buff_Cache[index];  //source 为 SRAM
#endif
       }

       DWT_post_count = DWT_get_count();
       DWT_delta = DWT_post_count - DWT_pre_count;
       if(DWT_delta==0)
       {
           printf("DWT count error! \r\n");
       }
       time_sdram_access = DWT_count_to_us(DWT_delta);

       printf("sdram write DWT count:%d \r\n", DWT_delta);
       printf("sdram write time:%dus \r\n", time_sdram_access);
       printf("sram cache sdram non cache write speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
       if(time_sdram_access==0)
       {
           while(1){;}
       }
//       printf("SDRAM write %d bytes data finished! \r\n" , SDRAM_EXAMPLE_DATALEN*4);
       printf("**********SRAM cacheable SDRAM Non cacheable End! **********\r\n");

       R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);


       printf("**********SRAM Non acheable SDRAM Non cacheable Start! **********\r\n");
       DWT_init();
       DWT_clean_count();
       DWT_pre_count = DWT_get_count();

       for (index = 0; index < datalen; index++)
       {
#if DTCM_Used
        sdram[index]  = dtcm_buffer[index];  //source 为 DTCM
//        sdram_read_buff[index] = sdram_write_buff[index];
#else
        sdram_nocache[index]  = SRAM_write_buff_Nocache[index];  //source 为 SRAM
#endif
       }

       DWT_post_count = DWT_get_count();
       DWT_delta = DWT_post_count - DWT_pre_count;
       if(DWT_delta==0)
       {
           printf("DWT count error! \r\n");
       }
       time_sdram_access = DWT_count_to_us(DWT_delta);

       printf("sdram write DWT count:%d \r\n", DWT_delta);
       printf("sdram write time:%dus \r\n", time_sdram_access);
       printf("sdram non cache write speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
       if(time_sdram_access==0)
       {
           while(1){;}
       }
       printf("SDRAM write %d bytes data finished! \r\n" , SDRAM_EXAMPLE_DATALEN*4);
       printf("**********SRAM Non cacheable SDRAM Non cacheable End! **********\r\n");

       R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);


       printf("##############################################################\r\n");
       printf("##############################################################\r\n");
       printf("##############################################################\r\n");
       printf("**********Read from SDRAM! **********\r\n");
       printf("**********SRAM cacheable SDRAM cacheable read Start! **********\r\n");
        DWT_init();
        DWT_clean_count();
        DWT_pre_count = DWT_get_count();

        for (index = 0; index < datalen; index++)
        {
            SRAM_write_buff_Cache[index] = sdram_cache[index];  //读SDRAM
        }

        DWT_post_count = DWT_get_count();
        DWT_delta = DWT_post_count - DWT_pre_count;
        if(DWT_delta==0)
        {
         printf("DWT count error! \r\n");
        }
        time_sdram_access = DWT_count_to_us(DWT_delta);

        printf("sdram read DWT count:%d \r\n", DWT_delta);
        printf("sdram read time:%dus \r\n", time_sdram_access);
        printf("sram cache sdram cache read speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
        if(time_sdram_access==0)
        {
         while(1){;}
        }
        //       printf("SDRAM write %d bytes data finished! \r\n" , SDRAM_EXAMPLE_DATALEN*4);
        printf("**********SRAM cacheable SDRAM cacheable read End! **********\r\n");

        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);


        printf("**********SRAM cacheable SDRAM non cacheable read Start! **********\r\n");
        DWT_init();
        DWT_clean_count();
        DWT_pre_count = DWT_get_count();

        for (index = 0; index < datalen; index++)
        {
            SRAM_write_buff_Nocache[index] = sdram_cache[index];  //读SDRAM
        }

        DWT_post_count = DWT_get_count();
        DWT_delta = DWT_post_count - DWT_pre_count;
        if(DWT_delta==0)
        {
         printf("DWT count error! \r\n");
        }
        time_sdram_access = DWT_count_to_us(DWT_delta);

        printf("sdram read DWT count:%d \r\n", DWT_delta);
        printf("sdram read time:%dus \r\n", time_sdram_access);
        printf("sram cache sdram non read write speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
        if(time_sdram_access==0)
        {
         while(1){;}
        }
        //       printf("SDRAM write %d bytes data finished! \r\n" , SDRAM_EXAMPLE_DATALEN*4);
        printf("**********SRAM cacheable SDRAM non cacheable read End! **********\r\n");

        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);


        printf("**********SRAM non cacheable SDRAM non cacheable read Start! **********\r\n");
        DWT_init();
        DWT_clean_count();
        DWT_pre_count = DWT_get_count();

        for (index = 0; index < datalen; index++)
        {
            SRAM_write_buff_Nocache[index] = sdram_nocache[index];  //读SDRAM
        }

        DWT_post_count = DWT_get_count();
        DWT_delta = DWT_post_count - DWT_pre_count;
        if(DWT_delta==0)
        {
         printf("DWT count error! \r\n");
        }
        time_sdram_access = DWT_count_to_us(DWT_delta);

        printf("sdram read DWT count:%d \r\n", DWT_delta);
        printf("sdram read time:%dus \r\n", time_sdram_access);
        printf("sram non cache sdram non read write speed:%dMB/s \r\n", SDRAM_EXAMPLE_DATALEN*4 / time_sdram_access);
        if(time_sdram_access==0)
        {
         while(1){;}
        }
        //       printf("SDRAM write %d bytes data finished! \r\n" , SDRAM_EXAMPLE_DATALEN*4);
        printf("**********SRAM non cacheable SDRAM non cacheable read End! **********\r\n");

        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);





}




/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    /* TODO: add your own code here */
    fsp_err_t err;
    err = R_SCI_B_UART_Open(&g_uart3_ctrl, &g_uart3_cfg);
    if(FSP_SUCCESS != err)
    {
        while(1);
    }
    printf("SDRAM read/write test start!\r\n");
    /* Initialize SDRAM. */
    bsp_sdram_init();          //See BSP_PRV_SDRAM_BUS_WIDTH to define bus width of SDRAM

    SDRAMWrite32Bit_test();

#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart(bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        IOPORT_CFG_OPEN (&IOPORT_CFG_CTRL, &IOPORT_CFG_NAME);
    }
}

#if BSP_TZ_SECURE_BUILD

FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
FSP_CPP_FOOTER

#endif
