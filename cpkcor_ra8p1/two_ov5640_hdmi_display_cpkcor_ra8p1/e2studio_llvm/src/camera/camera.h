#ifndef __CAMERA_H
#define __CAMERA_H

#include <stdint.h>
#include "camera_def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CameraConfig {
	CameraModelEnum model;
	CameraInterfaceEnum interface;
	uint16_t length;
	uint16_t width;
	const void *peripheral_inst;

	/* CEU 写入缓冲 */
	uint8_t *buffer_1;
	uint8_t *buffer_2;
	uint8_t *buffer_3;
} CameraConfig;

typedef struct Camera {
	CameraModelEnum model;
	CameraInterfaceEnum interface;
	uint16_t length;
	uint16_t width;
	const void *peripheral_inst;

	uint8_t *image; /* Deprecated, don't use */
	uint8_t *image_ready;
	CameraRotationBufferType rotation_buffer;
	
	uint32_t (*bufferIsIdle)(CameraInterfaceEnum interface, uint8_t *buf, bool *idle);
	uint32_t (*getBufferNum)(CameraInterfaceEnum interface, uint8_t *num);
	uint32_t (*getCalFPS)(CameraInterfaceEnum interface, float *fps);
	uint32_t (*getCaptureCnt)(CameraInterfaceEnum interface, uint32_t *cnt);
	uint32_t (*getDVPErrorCount)(CameraDVPErrorCountType *err_cnt);
	uint32_t (*getMIPIErrorCount)(CameraMIPIErrrorCountType *err_cnt);
	uint32_t (*getResolution)(CameraInterfaceEnum interface, uint16_t *length, uint16_t *width, CameraResolutionEnum *res);
	uint32_t (*init)(CameraInterfaceEnum interface, CameraRotationBufferType *rb);
	uint32_t (*isCaptureDone)(CameraInterfaceEnum interface, uint8_t **image);
	uint32_t (*isCaptureStart)(CameraInterfaceEnum interface, bool *result);
	uint32_t (*isPowerDown)(CameraInterfaceEnum interface);
	uint32_t (*powerDown)(CameraInterfaceEnum interface, uint8_t enable);
	uint32_t (*powerOff)(CameraInterfaceEnum interface, bool enable);
	uint32_t (*setFPS)(CameraInterfaceEnum interface, uint16_t target, uint16_t *real, bool auto_decrese);
	uint32_t (*setOutputSize)(CameraInterfaceEnum interface, uint16_t length, uint16_t width);
	uint32_t (*startCapture)(const void *peri, CameraInterfaceEnum interface, uint8_t *buffer);
	uint32_t (*stopCapture)(const void *peri, CameraInterfaceEnum interface);
} Camera;

uint32_t CameraGetBufferNum(Camera *camera, uint8_t *num);
uint32_t CameraGetCalFPS(Camera *camera, float *fps);
uint32_t CameraGetCaptureCnt(Camera *camera, uint32_t *cnt);
uint32_t CameraGetDVPErrorCnt(Camera *camera, CameraDVPErrorCountType *err_cnt);
uint32_t CameraGetMIPIErrorCnt(Camera *camera, CameraMIPIErrrorCountType *err_cnt);
uint32_t CameraGetResolution(Camera *camera, uint16_t *length, uint16_t *width, CameraResolutionEnum *res);
uint32_t CameraInit(Camera *camera, const CameraConfig *config);
uint32_t CameraIsCaptureDone(Camera *camera);
uint32_t CameraIsCaptureStart(Camera *camera, bool *result);
uint32_t CameraIsPowerDown(Camera *camera, uint8_t *result);
uint32_t CameraPowerDown(Camera *camera, uint8_t enable);
uint32_t CameraPowerOff(Camera *camera, bool enable);
uint32_t CameraReadyImageIsIdle(Camera *camera, bool *result);
uint32_t CameraSetFPS(Camera *camera, uint16_t target, uint16_t *real, bool auto_decrese);
uint32_t CameraSetOutputSize(Camera *camera, uint16_t length, uint16_t width);
uint32_t CameraStartCapture(Camera *camera, uint8_t *image_buffer);
uint32_t CameraStopCapture(Camera *camera);

#ifdef __cplusplus
}
#endif

#endif
