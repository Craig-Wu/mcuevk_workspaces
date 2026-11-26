#ifndef __NOR_FLASH_H
#define __NOR_FLASH_H

#include <stdint.h>

#define NORFLASH_MAP_START_ADDR	0x80000000
#define NORFLASH_MAP_END_ADDR	0x90000000
#define NORFLASH_SIZE			(1024 * 1024 * 32)
#define NORFLASH_SECTOR_SIZE	(1024 * 4)
#define NORFLASH_BLOCK_SIZE		(1024 * 64)

#ifdef __cplusplus
extern "C" {
#endif

uint32_t NorFlash_EraseChip(void);
uint32_t NorFlash_EraseSector(uint32_t sector);
uint32_t NorFlash_GetID(uint8_t *manufacturer_id, uint8_t *device_id, uint64_t *unique_id);
uint32_t NorFlash_Init(void);
uint32_t NorFlash_Program(uint32_t address, void const *data, uint32_t length);
uint32_t NorFlash_SetWriteEnable(void);
uint32_t NorFlash_WaitOperation(uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif
