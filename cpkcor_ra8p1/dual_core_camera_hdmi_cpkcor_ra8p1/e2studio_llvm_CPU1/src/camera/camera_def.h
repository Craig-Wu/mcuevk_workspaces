#ifndef __CAMERA_DEF_H
#define __CAMERA_DEF_H

#ifndef CAMERA_EN_DVP
#define CAMERA_EN_DVP	1
#endif

#ifndef CAMERA_EN_MIPI
#define CAMERA_EN_MIPI	1
#endif

/* RA8P1 手册规定 CEU VIO_CLK 不能大于 PCLKA。这个值影响在使用 DVP 接口时 setFPS 可达到的最大值
 * 实际能达到的最大值还与 PCB 布线有关，若遇到频率虽然接近上限但也依然不能运行，可以考虑把这个值改小 */
#ifndef CAMERA_DVP_FREQ_LIMIT
#define CAMERA_DVP_FREQ_LIMIT	125000000
#endif

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CameraModel {
	CAMERA_MODEL_OV5640,
	CAMERA_MODEL_OV7725
} CameraModelEnum;

typedef enum CameraInterface {
	CAMERA_INTERFACE_DVP,
	CAMERA_INTERFACE_MIPI_CSI
} CameraInterfaceEnum;

typedef enum CameraPixelFormat {
	CAMERA_PIXEL_GRB422,
	CAMERA_PIXEL_JPEG,
	CAMARA_PIXEL_RGB565,
	CAMERA_PIXEL_YUV420,
	CAMERA_PIXEL_YUV422,
} CameraPixelFormatEnum;

typedef enum CameraResolution {
	CAMERA_RESOLUTION_CUSTOM,
	CAMERA_RESOLUTION_QQVGA,		/* 160x120 */
	CAMERA_RESOLUTION_QVGA,			/* 320x240 */
	CAMERA_RESOLUTION_VGA,			/* 640x480 */
	CAMERA_RESOLUTION_SVGA,			/* 800x600 */
	CAMERA_RESOLUTION_XGA,			/* 1024x768 */
	CAMERA_RESOLUTION_720P,			/* 1280x720 */
	CAMERA_RESOLUTION_WXGA,			/* 1280x800 */
	CAMERA_RESOLUTION_WXGA_PLUS,	/* 1280x854 */
	CAMERA_RESOLUTION_SXGA,			/* 1280x960 */
	CAMERA_RESOLUTION_WSXGA_PLUS,	/* 1680x1050 */
	CAMERA_RESOLUTION_1080P,		/* 1920x1080 */
	CAMERA_RESOLUTION_1200P,		/* 1920x1200 */
	CAMERA_RESOLUTION_5M,			/* 2592x1944 */
} CameraResolutionEnum;

typedef struct CameraBufferStatus {
	uint32_t idle : 1;
	uint32_t : 31;
	uint8_t *buf;
} CameraBufferStatusType;

typedef struct CameraRotationBuffer {
	uint8_t length;
	uint8_t index;
	uint8_t *buffer[3];
} CameraRotationBufferType;

typedef struct CameraDVPErrorCount {
	uint32_t firewall;
	uint32_t hd_mismatch;
	uint32_t hd_missing;
	uint32_t overflow;
	uint32_t vd_error;
	uint32_t vd_mismatch;
	uint32_t vd_missing;
} CameraDVPErrorCountType;

typedef struct CameraMIPIErrorCount {
	uint32_t fifo_overflow;
	uint32_t preclip_h_err;
	uint32_t preclip_v_err;
	uint32_t resp_overflow;
	uint32_t axi_err;
} CameraMIPIErrrorCountType;

#ifdef __cplusplus
}
#endif

#endif
