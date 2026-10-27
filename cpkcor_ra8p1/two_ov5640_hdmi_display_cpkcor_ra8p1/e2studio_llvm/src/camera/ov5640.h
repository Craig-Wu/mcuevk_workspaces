#ifndef __OV5640_H
#define __OV5640_H

#include <stdint.h>
#include "camera_def.h"

#define OV5640_FIRMWARE_ADDR		0x8000
#define OV5640_IIC_ADDR				0x3C
#define OV5640_MAX_LENGTH			2592
#define OV5640_MAX_WIDTH			1944

/* DVP 接口目前通过定时器给出 XCLK，如果定时器改了，这里也要改
 * MIPI 接口通过晶振给出 XCLK，固定为 24MHz */
#define OV5640_DVP_XCLK_HZ			12000000
#define OV5640_MIPI_XCLK_HZ			24000000

/**
 * selectDVPClock() 是否允许修改：
 * 	- 0x3037[3:0] pll_pre_div
 * 	- 0x3037[4]   pll_roor_div
 * 	- 0x3108[1:0] sclk_root_div
 * 0: 在搜索时钟时会同步遍历修改以上值
 * 1: 固定以上的设置，与 Linux 驱动一致 */
#ifndef OV5640_CLOCK_FIXED_DIV
#define OV5640_CLOCK_FIXED_DIV		1
#endif

/**
 * 设置 OV5640_SetOutputSize() 的处理方式
 * 0: 使用 OpenMV 的处理方式，使用完整读出窗口，开启 scale、binning、抽样，以及相同的居中处理算法
 * 1: 仅修改输出窗口大小、VTS、HTS，不修改读出窗口、scale、binning、抽样设置，窗口不居中 */
#ifndef OV5640_OUTPUT_SIZE_METHOD
#define OV5640_OUTPUT_SIZE_METHOD	0
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	EXPOSURE_LEVEL_0,
	EXPOSURE_LEVEL_1,
	EXPOSURE_LEVEL_2,
	EXPOSURE_LEVEL_3,
	EXPOSURE_LEVEL_4,
	EXPOSURE_LEVEL_5,
	EXPOSURE_LEVEL_6,
	EXPOSURE_LEVEL_7,
	EXPOSURE_LEVEL_8,
	EXPOSURE_LEVEL_9,
	EXPOSURE_LEVEL_10
} OV5640_ExposureLevelEnum;

uint32_t OV5640_BufferIsIdle(CameraInterfaceEnum interface, uint8_t *buf, bool *idle);
uint32_t OV5640_GetBufferNum(CameraInterfaceEnum interface, uint8_t *num);
uint32_t OV5640_GetCalFPS(CameraInterfaceEnum interface, float *fps);
uint32_t OV5640_GetCaptueCnt(CameraInterfaceEnum interface, uint32_t *cnt);
uint32_t OV5640_GetDVPErrorCnt(CameraDVPErrorCountType *err_cnt);
uint32_t OV5640_GetID(uint16_t *id);
uint32_t OV5640_GetMIPIErrorCnt(CameraMIPIErrrorCountType *err_cnt);
uint32_t OV5640_GetOutputResolution(uint16_t *length, uint16_t *width);
uint32_t OV5640_GetOutputSize(uint16_t *hts, uint16_t *vts);
uint32_t OV5640_GetResolution(CameraInterfaceEnum interface, uint16_t *length, uint16_t *width, CameraResolutionEnum *res);
uint32_t OV5640_GetSysclk(CameraInterfaceEnum interface, uint32_t *clk);
uint32_t OV5640_Init(CameraInterfaceEnum interface, CameraRotationBufferType *rb);
uint32_t OV5640_IsCaptureStart(CameraInterfaceEnum interface, bool *result);
uint32_t OV5640_IsImageReady(CameraInterfaceEnum interface, uint8_t **p_image);
uint32_t OV5640_IsPowerDown(CameraInterfaceEnum interface);
uint32_t OV5640_PowerDown(CameraInterfaceEnum interface, uint8_t enable);
uint32_t OV5640_PowerOff(CameraInterfaceEnum interface, bool enable);
uint32_t OV5640_SetAutoFocus(uint8_t enable);
uint32_t OV5640_SetBinning(bool en_vertical, bool en_horizon);
uint32_t OV5640_SetExposureLevel(OV5640_ExposureLevelEnum level);
uint32_t OV5640_SetFPS(CameraInterfaceEnum interface, uint16_t target, uint16_t *real, bool auto_decrese);
uint32_t OV5640_SetMIPIChannel(uint8_t channel);
uint32_t OV5640_SetOutputFormat(CameraPixelFormatEnum format);
uint32_t OV5640_SetOutputSize(CameraInterfaceEnum interface, uint16_t length, uint16_t width);
uint32_t OV5640_SetScale(bool enable);
uint32_t OV5640_SetScaleMode(bool manual);
uint32_t OV5640_SetSubsample(uint8_t horizon, uint8_t vertical);
uint32_t OV5640_SetWindowSize(uint16_t length, uint16_t width, bool center);
uint32_t OV5640_StartCapture(const void *peri, CameraInterfaceEnum interface, uint8_t *image_buffer);
uint32_t OV5640_StopCapture(const void *peri, CameraInterfaceEnum interface);

#ifdef __cplusplus
}
#endif

#endif
