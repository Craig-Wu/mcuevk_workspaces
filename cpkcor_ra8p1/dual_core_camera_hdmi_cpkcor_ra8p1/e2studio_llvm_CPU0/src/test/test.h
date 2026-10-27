#ifndef __TEST_H
#define __TEST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TEST_EN_AUDIO
#define TEST_EN_AUDIO		0
#endif

#ifndef TEST_EN_CAMERA
#define TEST_EN_CAMERA		0
#endif

#ifndef TEST_EN_HYPER_RAM
#define TEST_EN_HYPER_RAM	0
#endif

#ifndef TEST_EN_LCD
#define TEST_EN_LCD			0
#endif

#ifndef TEST_EN_NOR_FLASH
#define TEST_EN_NOR_FLASH	0
#endif

#ifndef TEST_EN_PDM
#define TEST_EN_PDM			0
#endif

#ifndef TEST_EN_SDRAM
#define TEST_EN_SDRAM		0
#endif

#if TEST_EN_AUDIO
uint32_t TestAudio(void);
#endif

#if TEST_EN_CAMERA
uint32_t TestCamera(void *camera);
uint32_t TestCamera2(void *camera1, void *camera_cfg1, void *camera2, void *camera_cfg2);
#endif

#if TEST_EN_HYPER_RAM
uint32_t TestHyperRAM(uint32_t start_addr, uint32_t size);
#endif

#if TEST_EN_LCD
uint32_t TestLCD(void *lcd_device);
#endif

#if TEST_EN_NOR_FLASH
uint32_t TestNorFlash(uint32_t start_addr, uint32_t size);
#endif

#if TEST_EN_SDRAM
#define TEST_SDRAM_SPEED_COUNT	6

typedef enum {
	TEST_SDRAM_WIDTH_8BIT,
	TEST_SDRAM_WIDTH_16BIT,
	TEST_SDRAM_WIDTH_32BIT,
	TEST_SDRAM_WIDTH_64BIT
} TestSDRAM_WidthEnum;

typedef enum {
	TEST_SDRAM_DIR_DTCM_TO_SDRAM,
	TEST_SDRAM_DIR_SRAM_TO_SDRAM,
	TEST_SDRAM_DIR_SDRAM_TO_DTCM,
	TEST_SDRAM_DIR_SDRAM_TO_SRAM,

	TEST_SDRAM_DIR_DTCM_TO_SDRAM_NC,
	TEST_SDRAM_DIR_SRAM_TO_SDRAM_NC,
	TEST_SDRAM_DIR_SRAM_NC_TO_SDRAM,
	TEST_SDRAM_DIR_SRAM_NC_TO_SDRAM_NC,
	TEST_SDRAM_DIR_SDRAM_NC_TO_DTCM,
	TEST_SDRAM_DIR_SDRAM_TO_SRAM_NC,
	TEST_SDRAM_DIR_SDRAM_NC_TO_SRAM,
	TEST_SDRAM_DIR_SDRAM_NC_TO_SRAM_NC
} TestSDRAM_DirEnum;

uint32_t TestSDRAM(uint32_t start_addr, uint32_t size, bool speed_write, bool speed_read);
void TestSDRAM_Speed(float *speed, TestSDRAM_WidthEnum width, TestSDRAM_DirEnum dir);
void TestSDRAM_SpeedRead(float speed[TEST_SDRAM_SPEED_COUNT], TestSDRAM_WidthEnum width);
void TestSDRAM_SpeedWrite(float speed[TEST_SDRAM_SPEED_COUNT], TestSDRAM_WidthEnum width);
#endif

#ifdef __cplusplus
}
#endif

#endif
