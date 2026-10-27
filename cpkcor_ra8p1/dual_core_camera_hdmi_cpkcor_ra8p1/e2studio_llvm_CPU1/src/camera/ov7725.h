#ifndef __OV7725_H
#define __OV7725_H

#include <stdbool.h>
#include <stdint.h>
#include "camera_def.h"

#if CAMERA_EN_DVP

#define OV7725_IIC_ADDR		0x21

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OV7725_GetCaptureCnt(CameraInterfaceEnum interface, uint32_t *cnt);
uint32_t OV7725_GetErrorCnt(CameraDVPErrorCountType *err_cnt);
uint32_t OV7725_GetID(uint16_t *id);
uint32_t OV7725_GetResolution(CameraInterfaceEnum interface, uint16_t *length, uint16_t *width, CameraResolutionEnum *res);
uint32_t OV7725_Init(CameraInterfaceEnum interface, CameraRotationBufferType *rb);
uint32_t OV7725_IsCaptureStart(CameraInterfaceEnum interface, bool *result);
uint32_t OV7725_IsImageReady(CameraInterfaceEnum interface, uint8_t **p_image);
uint32_t OV7725_IsPowerDown(CameraInterfaceEnum interface);
uint32_t OV7725_PowerDown(CameraInterfaceEnum interface, uint8_t enable);
uint32_t OV7725_SetFPS(CameraInterfaceEnum interface, uint16_t target, uint16_t *real, bool auto_decrese);
uint32_t OV7725_SetOutputSize(CameraInterfaceEnum interface, uint16_t length, uint16_t width);
uint32_t OV7725_SetOutputSizePreset(CameraResolutionEnum res);
uint32_t OV7725_StartCapture(const void *peri, CameraInterfaceEnum interface, uint8_t *image_buffer);
uint32_t OV7725_StopCapture(const void *peri, CameraInterfaceEnum interface);

#ifdef __cplusplus
}
#endif

#endif /* #if CAMERA_EN_DVP */

#endif
