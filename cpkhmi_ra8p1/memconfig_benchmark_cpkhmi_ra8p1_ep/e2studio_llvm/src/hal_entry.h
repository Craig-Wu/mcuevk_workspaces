#ifndef __HAL_ENTRY_H
#define __HAL_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#if __CODE_AREA == 1
#define CODE_AREA		__attribute__((section(".itcm_code_from_flash")))
#elif __CODE_AREA == 2
#define CODE_AREA		__attribute__((section(".ram_code_from_flash")))
#elif __CODE_AREA == 3
#define CODE_AREA		__attribute__((section(".sdram_code_from_flash")))
#elif __CODE_AREA == 4
#define CODE_AREA		__attribute__((section(".ospi0_cs0_code_from_flash")))
#elif __CODE_AREA == 5
#define CODE_AREA		__attribute__((section(".ospi1_cs0_code_from_flash")))
#else
#define CODE_AREA
#endif

#if __DATA_AREA == 1
#define DATA_AREA_BSS	__attribute__((section(".dtcm_noinit")))
#define DATA_AREA_DATA	__attribute__((section(".dtcm_from_flash")))
#define DATA_AREA_ZERO	__attribute__((section(".dtcm")))
#elif __DATA_AREA == 2
#define DATA_AREA_BSS	__attribute__((section(".sdram_noinit")))
#define DATA_AREA_DATA	__attribute__((section(".sdram_from_flash")))
#define DATA_AREA_ZERO	__attribute__((section(".sdram")))
#else
#define DATA_AREA_BSS
#define DATA_AREA_DATA
#define DATA_AREA_ZERO
#endif

#ifdef __cplusplus
}
#endif

#endif
