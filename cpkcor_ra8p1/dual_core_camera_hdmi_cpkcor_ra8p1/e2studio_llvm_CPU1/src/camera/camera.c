/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include "camera.h"
#include "hal_data.h"
#include "ov5640.h"
#include "ov7725.h"

/*================================== MACROS =======================================================*/

#ifndef __CAMERA_DEBUG
#define __CAMERA_DEBUG	1
#endif

#define IS_INVALID_POINT(p)				if (p == NULL) { return FSP_ERR_INVALID_POINTER; }
#define IS_INVALID_SUPPORT(s)			if (s == NULL) { return FSP_ERR_UNSUPPORTED; }

#if __CAMERA_DEBUG
#include "utils/log.h"
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define CAMERA_LOGD(msg, ...)			LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define CAMERA_LOGI(msg, ...)			LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define CAMERA_LOGW(msg, ...)			LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define CAMERA_LOGE(msg, ...)			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { return v; }
#define CAMERA_LOGD(msg, ...)
#define CAMERA_LOGI(msg, ...)
#define CAMERA_LOGW(msg, ...)
#define CAMERA_LOGE(msg, ...)
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/
/*================================== Private Functions Prototypes =================================*/
/*================================== Public Functions =============================================*/

uint32_t CameraGetBufferNum(Camera *camera, uint8_t *num)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(num);
	IS_INVALID_SUPPORT(camera->getBufferNum);

	err = camera->getBufferNum(camera->interface, num);

	return err;
}

uint32_t CameraGetCalFPS(Camera *camera, float *fps)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(fps);
	IS_INVALID_SUPPORT(camera->getCalFPS);

	err = camera->getCalFPS(camera->interface, fps);

	return err;
}

uint32_t CameraGetCaptureCnt(Camera *camera, uint32_t *cnt)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(cnt);
	IS_INVALID_SUPPORT(camera->getCaptureCnt);

	err = camera->getCaptureCnt(camera->interface, cnt);

	return err;
}

uint32_t CameraGetDVPErrorCnt(Camera *camera, CameraDVPErrorCountType *err_cnt)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(err_cnt);
	IS_INVALID_SUPPORT(camera->getDVPErrorCount);

	if (camera->interface != CAMERA_INTERFACE_DVP) {
		return FSP_ERR_ASSERTION;
	}

	err = camera->getDVPErrorCount(err_cnt);

	return err;
}

uint32_t CameraGetMIPIErrorCnt(Camera *camera, CameraMIPIErrrorCountType *err_cnt)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(err_cnt);
	IS_INVALID_SUPPORT(camera->getMIPIErrorCount);

	if (camera->interface != CAMERA_INTERFACE_MIPI_CSI) {
		return FSP_ERR_ASSERTION;
	}

	err = camera->getMIPIErrorCount(err_cnt);

	return err;
}

uint32_t CameraGetResolution(Camera *camera, uint16_t *length, uint16_t *width, CameraResolutionEnum *res)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->getResolution);

	err = camera->getResolution(camera->interface, length, width, res);

	return err;
}

uint32_t CameraInit(Camera *camera, const CameraConfig *config)
{
	uint16_t default_length;
	uint16_t default_width;
	uint32_t i, j;

	uint32_t err = FSP_ERR_UNSUPPORTED;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(config);

	camera->model = config->model;
	camera->interface = config->interface;
	camera->length = config->length;
	camera->width = config->width;
	camera->peripheral_inst = config->peripheral_inst;

#if CAMERA_EN_DVP
	if (camera->model == CAMERA_MODEL_OV7725) {
		if (camera->interface == CAMERA_INTERFACE_MIPI_CSI) {
			return err;
		}
		camera->bufferIsIdle = NULL;
		camera->getBufferNum = NULL;
		camera->getCalFPS = NULL;
		camera->getCaptureCnt = OV7725_GetCaptureCnt;
		camera->getDVPErrorCount = OV7725_GetErrorCnt;
		camera->getMIPIErrorCount = NULL;
		camera->getResolution = OV7725_GetResolution;
		camera->init = OV7725_Init;
		camera->isCaptureDone = OV7725_IsImageReady;
		camera->isCaptureStart = OV7725_IsCaptureStart;
		camera->isPowerDown = OV7725_IsPowerDown;
		camera->powerDown = OV7725_PowerDown;
		camera->powerOff = NULL;
		camera->setFPS = OV7725_SetFPS;
		camera->setOutputSize = OV7725_SetOutputSize;
		camera->startCapture = OV7725_StartCapture;
		camera->stopCapture = OV7725_StopCapture;
	}
#endif
	if (camera->model == CAMERA_MODEL_OV5640) {
		camera->bufferIsIdle = OV5640_BufferIsIdle;
		camera->getBufferNum = OV5640_GetBufferNum;
		camera->getCalFPS = OV5640_GetCalFPS;
		camera->getCaptureCnt = OV5640_GetCaptueCnt;
		camera->getDVPErrorCount = OV5640_GetDVPErrorCnt;
		camera->getMIPIErrorCount = OV5640_GetMIPIErrorCnt;
		camera->getResolution = OV5640_GetResolution;
		camera->init = OV5640_Init;
		camera->isCaptureDone = OV5640_IsImageReady;
		camera->isCaptureStart = OV5640_IsCaptureStart;
		camera->isPowerDown = OV5640_IsPowerDown;
		camera->powerDown = OV5640_PowerDown;
		camera->powerOff = OV5640_PowerOff;
		camera->setFPS = OV5640_SetFPS;
		camera->setOutputSize = OV5640_SetOutputSize;
		camera->startCapture = OV5640_StartCapture;
		camera->stopCapture = OV5640_StopCapture;
	}
	else {
		return err;
	}

	__DSB();
	__ISB();
	camera->image = NULL;
	camera->image_ready = NULL;
	err = camera->init(camera->interface, &camera->rotation_buffer);
	UNLIKE_RETURN(err, 0, "init failed: %" PRIu32, err);
	err = CameraGetResolution(camera, &default_length, &default_width, NULL);
	if ((err == FSP_ERR_UNSUPPORTED) || (camera->length != default_length) || (camera->width != default_width)) {
		CAMERA_LOGI("Reset output size: %" PRIu32 "x%" PRIu32, camera->length, camera->width);
		err = camera->setOutputSize(camera->interface, camera->length, camera->width);
		UNLIKE_RETURN(err, 0, "setOutputSize failed: %" PRIu32, err);
	}

	if (camera->interface == CAMERA_INTERFACE_DVP) {
		camera->rotation_buffer.buffer[0] = config->buffer_1;
		camera->rotation_buffer.buffer[1] = config->buffer_2;
		camera->rotation_buffer.buffer[2] = config->buffer_3;

		for (i = 0; i < 3; i++) {
			for (j = i + 1; j < 3; j++) {
				if ((camera->rotation_buffer.buffer[j] != NULL) && (camera->rotation_buffer.buffer[i] == NULL)) {
					camera->rotation_buffer.buffer[i] = camera->rotation_buffer.buffer[j];
					camera->rotation_buffer.buffer[j] = NULL;
					break;
				}
				if ((camera->rotation_buffer.buffer[j] == camera->rotation_buffer.buffer[i]) && (camera->rotation_buffer.buffer[i] != NULL)) {
					camera->rotation_buffer.buffer[j] = NULL;
				}
			}
		}

		camera->rotation_buffer.length = 0;
		for (i = 0; i < 3; i++) {
			if (camera->rotation_buffer.buffer[i] != NULL) {
				camera->rotation_buffer.length++;
			}
			else {
				break;
			}
		}
		CAMERA_LOGD("rotation_buffer.length: %" PRIu32, camera->rotation_buffer.length);
	}

	return err;
}

uint32_t CameraIsCaptureDone(Camera *camera)
{
	uint32_t ret;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->isCaptureDone);

	camera->image_ready = NULL;
	ret = camera->isCaptureDone(camera->interface, &camera->image_ready);
	/* 外设本身没有接收缓冲，使用的是 CameraStartCapture() 传递的接收缓冲 */
	if (ret && (camera->image_ready == NULL)) {
		camera->image_ready = camera->image;
	}

	return ret;
}

uint32_t CameraIsCaptureStart(Camera *camera, bool *result)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(result);
	IS_INVALID_SUPPORT(camera->isCaptureStart);

	err = camera->isCaptureStart(camera->interface, result);

	return err;
}

uint32_t CameraIsPowerDown(Camera *camera, uint8_t *result)
{
	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(result);
	IS_INVALID_SUPPORT(camera->isPowerDown);

	if (camera->isPowerDown(camera->interface)) {
		*result = 1;
	}
	else {
		*result = 0;
	}

	return 0;
}

uint32_t CameraPowerDown(Camera *camera, uint8_t enable)
{
	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->powerDown);

	if (camera->isPowerDown != NULL) {
		if (camera->isPowerDown(camera->interface) != enable) {
			camera->powerDown(camera->interface, enable);
		}
	}
	else {
		camera->powerDown(camera->interface, enable);
	}

	return 0;
}

uint32_t CameraPowerOff(Camera *camera, bool enable)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->powerOff);

	err = camera->powerOff(camera->interface, enable);

	return err;
}

uint32_t CameraReadyImageIsIdle(Camera *camera, bool *result)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_POINT(result);
	IS_INVALID_POINT(camera->image_ready);
	IS_INVALID_SUPPORT(camera->bufferIsIdle);

	err = camera->bufferIsIdle(camera->interface, camera->image_ready, result);

	return err;
}

uint32_t CameraSetFPS(Camera *camera, uint16_t target, uint16_t *real, bool auto_decrese)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->setFPS);

	err = camera->setFPS(camera->interface, target, real, auto_decrese);

	return err;
}

uint32_t CameraSetOutputSize(Camera *camera, uint16_t length, uint16_t width)
{
	uint32_t err = FSP_ERR_UNSUPPORTED;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->setOutputSize);

	err = camera->setOutputSize(camera->interface, length, width);
	UNLIKE_RETURN(err, 0, "setOutputSize failed: %" PRIu32, err);
	camera->length = length;
	camera->width = width;

	return err;
}

uint32_t CameraStartCapture(Camera *camera, uint8_t *image_buffer)
{
	uint32_t err = FSP_ERR_UNSUPPORTED;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->startCapture);

	err = camera->startCapture(camera->peripheral_inst, camera->interface, image_buffer);
	UNLIKE_RETURN(err, 0, "startCapture failed: %" PRIu32, err);
	camera->image = image_buffer;

	return err;
}

uint32_t CameraStopCapture(Camera *camera)
{
	uint32_t err;

	IS_INVALID_POINT(camera);
	IS_INVALID_SUPPORT(camera->stopCapture);

	err = camera->stopCapture(camera->peripheral_inst, camera->interface);
	UNLIKE_RETURN(err, 0, "stopCapture failed: %" PRIu32, err);

	return err;
}

/*================================== Private Functions ============================================*/
