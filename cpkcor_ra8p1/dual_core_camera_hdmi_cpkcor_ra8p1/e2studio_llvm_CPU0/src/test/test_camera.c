#include "test.h"

#if TEST_EN_CAMERA

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>

#include "hal_data.h"
#include "camera/camera.h"
#include "camera/ov5640.h"
#include "perf_counter/perf_counter.h"
#include "utils/log.h"

/*================================== MACROS =======================================================*/

#ifndef TEST_CAMERA_EN_PERFORMANCE
#define TEST_CAMERA_EN_PERFORMANCE			1
#endif

#ifndef TEST_CAMERA_EN_PRESURE_REFINED
#define TEST_CAMERA_EN_PRESURE_REFINED		1
#endif

#ifndef TEST_CAMERA_EN_SINGLE_CAPTURE
#define TEST_CAMERA_EN_SINGLE_CAPTURE		0
#endif

#ifndef TEST_CAMERA_DVP_CACHE_HSIZE
#define TEST_CAMERA_DVP_CACHE_HSIZE			1920
#endif

#ifndef TEST_CAMERA_DVP_CACHE_VSIZE
#define TEST_CAMERA_DVP_CACHE_VSIZE			1080
#endif

/**
 * 1: DVP Cache in SRAM
 * 2: DVP Cache in SDRAM
 * 3: DVP Cache in HyperRAM(Not available in CPKHMI) */
#ifndef TEST_CAMERA_DVP_CACHE_POS
#define TEST_CAMERA_DVP_CACHE_POS			2
#endif

#if TEST_CAMERA_EN_PERFORMANCE
#ifndef PERFORMANCE_TIME_S
#define PERFORMANCE_TIME_S					20
#endif
#endif /* #if TEST_CAMERA_EN_PERFORMANCE */

#if TEST_CAMERA_EN_PRESURE_REFINED
#ifndef CAMERA_PRESURE_REFINED_SIZE
#define CAMERA_PRESURE_REFINED_SIZE			1024
#endif
#endif

#define DVP_CACHE_IN_SRAM					1
#define DVP_CACHE_IN_SDRAM					2
#define DVP_CACHE_IN_HYPER_RAM				3

#define TEST_CAMERA_DVP_CACHE_SIZE			(TEST_CAMERA_DVP_CACHE_HSIZE * TEST_CAMERA_DVP_CACHE_VSIZE * 2)
#define UNLIKE_RETURN(v, t, msg, ...)   	if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }

#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SRAM
#undef TEST_CAMERA_EN_PRESURE_REFINED
#define TEST_CAMERA_EN_PRESURE_REFINED		0
#endif

#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SDRAM
#include "sdram.h"
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

#if TEST_CAMERA_EN_PRESURE_REFINED
static float s_speed_sum_r[CAMERA_PRESURE_REFINED_SIZE];
static float s_speed_sum_w[CAMERA_PRESURE_REFINED_SIZE];
#endif

#if TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SRAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8)));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_SDRAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".sdram_noinit_nocache")));
#elif TEST_CAMERA_DVP_CACHE_POS == DVP_CACHE_IN_HYPER_RAM
static uint8_t s_dvp_image[TEST_CAMERA_DVP_CACHE_SIZE] __attribute__((aligned(8))) __attribute__((section(".ospi1_cs0_noinit_nocache")));
#endif

/*================================== Private Functions Prototypes =================================*/

#if TEST_CAMERA_EN_PERFORMANCE
static bool checkDVPHasError(CameraDVPErrorCountType *err_cnt);
static bool checkMIPIHasError(CameraMIPIErrrorCountType *err_cnt);
static uint32_t testCameraPerformance(Camera *camera);
static uint32_t testCameraPerformance2(Camera *camera1, const CameraConfig *cfg1, Camera *camera2, const CameraConfig *cfg2);
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
static uint32_t testCameraPresureRefined(Camera *camera);
static uint32_t testCameraPresureRefined2(Camera *camera1, Camera *camera2, uint8_t which);
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
uint32_t testCameraSingleCapture(Camera *camera);
#endif

/*================================== Public Functions =============================================*/

uint32_t TestCamera(void *camera)
{
	(void)camera;

#if TEST_CAMERA_EN_PERFORMANCE
	testCameraPerformance(camera);
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
	testCameraPresureRefined(camera);
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
	testCameraSingleCapture(camera);
#endif

	return 0;
}

uint32_t TestCamera2(void *camera1, void *camera_cfg1, void *camera2, void *camera_cfg2)
{
	(void)camera1;
	(void)camera2;

#if TEST_CAMERA_EN_PERFORMANCE
	testCameraPerformance2(camera1, camera_cfg1, camera2, camera_cfg2);
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
	testCameraPresureRefined2(camera1, camera2, 1);
#endif

	return 0;
}

/*================================== Private Functions ============================================*/

#if TEST_CAMERA_EN_PERFORMANCE

static bool checkDVPHasError(CameraDVPErrorCountType *err_cnt)
{
	bool has_error = false;

	if (err_cnt->firewall) {
		LOG_W(__FUNCTION__, "firewall: %" PRIu32, err_cnt->firewall);
		has_error = true;
	}
	if (err_cnt->hd_mismatch) {
		LOG_W(__FUNCTION__, "hd_mismatch: %" PRIu32, err_cnt->hd_mismatch);
		has_error = true;
	}
	if (err_cnt->hd_missing) {
		LOG_W(__FUNCTION__, "hd_missing: %" PRIu32, err_cnt->hd_missing);
		// has_error = true;
	}
	if (err_cnt->overflow) {
		LOG_W(__FUNCTION__, "overflow: %" PRIu32, err_cnt->overflow);
		has_error = true;
	}
	if (err_cnt->vd_error) {
		LOG_W(__FUNCTION__, "vd_error: %" PRIu32, err_cnt->vd_error);
		has_error = true;
	}
	if (err_cnt->vd_mismatch) {
		LOG_W(__FUNCTION__, "vd_mismatch: %" PRIu32, err_cnt->vd_mismatch);
		has_error = true;
	}
	if (err_cnt->vd_missing) {
		LOG_W(__FUNCTION__, "vd_missing: %" PRIu32, err_cnt->vd_missing);
		has_error = true;
	}

	return has_error;
}

static bool checkMIPIHasError(CameraMIPIErrrorCountType *err_cnt)
{
	bool has_error = false;

	if (err_cnt->axi_err) {
		LOG_W(__FUNCTION__, "axi_error: %" PRIu32, err_cnt->axi_err);
		has_error = true;
	}
	if (err_cnt->fifo_overflow) {
		LOG_W(__FUNCTION__, "fifo_overflow: %" PRIu32, err_cnt->fifo_overflow);
		has_error = true;
	}
	if (err_cnt->preclip_h_err) {
		LOG_W(__FUNCTION__, "preclip_h_err: %" PRIu32, err_cnt->preclip_h_err);
		has_error = true;
	}
	if (err_cnt->preclip_v_err) {
		LOG_W(__FUNCTION__, "preclip_v_err: %" PRIu32, err_cnt->preclip_v_err);
		has_error = true;
	}
	if (err_cnt->resp_overflow) {
		LOG_W(__FUNCTION__, "resp_overflow: %" PRIu32, err_cnt->resp_overflow);
		has_error = true;
	}

	return has_error;
}

static uint32_t testCameraPerformance(Camera *camera)
{
	bool is_capture_start;
	uint32_t i;
	uint32_t ret;
	uint32_t cnt1, cnt2;
	int64_t time_start, time_end;
	float time_s, fps;
	CameraDVPErrorCountType dvp_error_cnt;
	CameraMIPIErrrorCountType mipi_error_cnt;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PERFORMANCE");

	ret = CameraIsCaptureStart(camera, &is_capture_start);
	UNLIKE_RETURN(ret, 0, "IsCaptureStart failed: %" PRIu32, ret);
	if (is_capture_start) {
		LOG_I(__FUNCTION__, "CEU already start, try to close");
		ret = CameraStopCapture(camera);
		UNLIKE_RETURN(ret, 0, "StopCapture failed: %" PRIu32, ret);
		memset(s_dvp_image, 0, sizeof(s_dvp_image));
	}

	if (camera->model == CAMERA_MODEL_OV5640) {
		// OV5640_SetScale(false);
		// OV5640_SetSubsample(1, 1);
		// OV5640_SetWindowSize(640, 480, true);
		// ret = CameraSetOutputSize(camera, camera->length, camera->width);
		// UNLIKE_RETURN(ret, 0, "SetOutputSize failed: %" PRIu32, ret);
	}
	ret = CameraSetFPS(camera, 55, NULL, false);
	UNLIKE_RETURN(ret, 0, "SetFPS failed: %" PRIu32, ret);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
	ret = CameraStartCapture(camera, s_dvp_image);
	UNLIKE_RETURN(ret, 0, "StartCapture failed: %" PRIu32, ret);
	R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
	ret = CameraGetCalFPS(camera, &fps);
	if (ret == 0) {
		printf("Theory FPS: %.3f\r\n", fps);
	}

	ret = CameraGetCaptureCnt(camera, &cnt1);
	UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
		ret = CameraGetCaptureCnt(camera, &cnt1);
		UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);
		if (camera->interface == CAMERA_INTERFACE_DVP) {
			CameraGetDVPErrorCnt(camera, &dvp_error_cnt);
			if (checkDVPHasError(&dvp_error_cnt)) {
				LOG_E(__FUNCTION__, "Exit because DVP error");
				return 1;
			}
		}
		else {
			CameraGetMIPIErrorCnt(camera, &mipi_error_cnt);
			if (checkMIPIHasError(&mipi_error_cnt)) {
				LOG_E(__FUNCTION__, "Exit because MIPI error");
				return 1;
			}
		}
	}

	time_start = get_system_ms();
	for (i = 0; i < PERFORMANCE_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();
	ret = CameraGetCaptureCnt(camera, &cnt2);
	UNLIKE_RETURN(ret, 0, "GetCaptureCnt failed: %" PRIu32, ret);

	time_s = (float)(time_end - time_start) / 1000.0f;
	fps = (float)(cnt2 - cnt1) / time_s;
	printf("Actual running time: %.3fs\r\n", time_s);
	printf("Total get frame    : %" PRIu32 "\r\n", cnt2 - cnt1);
	printf("Calculate FPS      : %.3f\r\n", fps);
	puts("======== Error Count ========");
	if (camera->interface == CAMERA_INTERFACE_DVP) {
		CameraGetDVPErrorCnt(camera, &dvp_error_cnt);
		printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt.firewall);
		printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.hd_mismatch);
		printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt.hd_missing);
		printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt.overflow);
		printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt.vd_error);
		printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.vd_mismatch);
		printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt.vd_missing);
	}
	else {
		CameraGetMIPIErrorCnt(camera, &mipi_error_cnt);
		printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt.fifo_overflow);
		printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_h_err);
		printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_v_err);
		printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt.resp_overflow);
		printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt.axi_err);
	}

	return 0;
}

static uint32_t testCameraPerformance2(Camera *camera1, const CameraConfig *cfg1, Camera *camera2, const CameraConfig *cfg2)
{
	uint32_t i, err;
	uint32_t cnt1_start, cnt1_end;
	uint32_t cnt2_start, cnt2_end;
	int64_t time_start, time_end;
	CameraDVPErrorCountType dvp_error_cnt;
	CameraMIPIErrrorCountType mipi_error_cnt;
	float fps1, fps2, time_s;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PERFORMANCE");

	if ((cfg1->interface != CAMERA_INTERFACE_DVP) && (cfg2->interface != CAMERA_INTERFACE_MIPI_CSI)) {
		LOG_E(__FUNCTION__, "camera1 must using DVP and camera2 must using MIPI");
		return 1;
	}

#if 0
	bool capture_start;

	/* 确保两个相机都停止捕获 */
	err = CameraIsCaptureStart(camera1, &capture_start);
	UNLIKE_RETURN(err, 0, "Camera1 IsCaptureStart failed: %" PRIu32, err);
	if (capture_start) {
		LOG_I(__FUNCTION__, "Camera1 already start, try to close");
		err = CameraStopCapture(camera1);
		UNLIKE_RETURN(err, 0, "Camera1 StopCapture failed: %" PRIu32, err);
		if (camera1->interface == CAMERA_INTERFACE_DVP) {
			memset(s_dvp_image, 0, sizeof(s_dvp_image));
		}
	}
	err = CameraIsCaptureStart(camera2, &capture_start);
	UNLIKE_RETURN(err, 0, "Camera2 IsCaptureStart failed: %" PRIu32, err);
	if (capture_start) {
		LOG_I(__FUNCTION__, "Camera2 already start, try to close");
		err = CameraStopCapture(camera2);
		UNLIKE_RETURN(err, 0, "Camera2 StopCapture failed: %" PRIu32, err);
		if (camera2->interface == CAMERA_INTERFACE_DVP) {
			memset(s_dvp_image, 0, sizeof(s_dvp_image));
		}
	}
#endif

	LOG_D(__FUNCTION__, "camera1 resolution: %" PRIu32 "x%" PRIu32, cfg1->length, cfg1->width);
	LOG_D(__FUNCTION__, "camera2 resolution: %" PRIu32 "x%" PRIu32, cfg2->length, cfg2->width);

	/* 两个相机都重新指定帧率，由于 MIPI 接口的 OV5640 Power down 功能暂时异常，因此这里使用掉电重配置 */
	CameraPowerOff(camera1, true);
	CameraPowerOff(camera2, true);
	LOG_D(__FUNCTION__, "camera1 re-init");
	err = CameraInit(camera1, cfg1);
	UNLIKE_RETURN(err, 0, "camera1 init failed: %" PRIu32, err);
	err = CameraSetFPS(camera1, 60, NULL, false);
	UNLIKE_RETURN(err, 0, "camera1 set FPS failed: %" PRIu32, err);
	CameraPowerDown(camera1, 1);
	LOG_D(__FUNCTION__, "camera1 re-init done, and enter power down");

	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	LOG_D(__FUNCTION__, "camera2 re-init");
	err = CameraInit(camera2, cfg2);
	UNLIKE_RETURN(err, 0, "camera2 init failed: %" PRIu32, err);
	err = CameraSetFPS(camera2, 60, NULL, false);
	UNLIKE_RETURN(err, 0, "camera2 set FPS failed: %" PRIu32, err);
	R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
	LOG_D(__FUNCTION__, "camera2 re-init done, and ready to capture");

	CameraPowerDown(camera1, 0);
	err = CameraStartCapture(camera1, s_dvp_image);
	UNLIKE_RETURN(err, 0, "camera1 start capture failed: %" PRIu32, err);
	err = CameraStartCapture(camera2, NULL);
	UNLIKE_RETURN(err, 0, "camera2 start capture failed: %" PRIu32, err);

	CameraGetCaptureCnt(camera1, &cnt1_start);
	CameraGetCaptureCnt(camera2, &cnt2_start);
	time_start = get_system_ms();
	for (i = 0; i < PERFORMANCE_TIME_S; i++) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
	}
	time_end = get_system_ms();

	CameraGetCaptureCnt(camera1, &cnt1_end);
	CameraGetCaptureCnt(camera2, &cnt2_end);
	time_s = (float)(time_end - time_start) / 1000.0f;
	fps1 = (float)(cnt1_end - cnt1_start) / time_s;
	fps2 = (float)(cnt2_end - cnt2_start) / time_s;
	printf("Actual running time  : %.3f\r\n", time_s);
	printf("Camera1 get frame    : %" PRIu32 "\r\n", cnt1_end - cnt1_start);
	printf("Camera1 calculate FPS: %.3f\r\n", fps1);
	printf("Camera2 get frame    : %" PRIu32 "\r\n", cnt2_end - cnt2_start);
	printf("Camera2 calculate FPS: %.3f\r\n", fps2);

	if (camera1->interface == CAMERA_INTERFACE_DVP) {
		CameraGetDVPErrorCnt(camera1, &dvp_error_cnt);
		CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt);
	}
	else {
		CameraGetDVPErrorCnt(camera2, &dvp_error_cnt);
		CameraGetMIPIErrorCnt(camera1, &mipi_error_cnt);
	}
	puts("======== DVP Error Count ========");
	printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt.firewall);
	printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.hd_mismatch);
	printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt.hd_missing);
	printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt.overflow);
	printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt.vd_error);
	printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt.vd_mismatch);
	printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt.vd_missing);
	puts("======== MIPI Error Count ========");
	printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt.fifo_overflow);
	printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_h_err);
	printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt.preclip_v_err);
	printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt.resp_overflow);
	printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt.axi_err);

	return 0;
}
#endif

#if TEST_CAMERA_EN_PRESURE_REFINED
static uint32_t testCameraPresureRefined(Camera *camera)
{
	bool is_capture_start;
	uint32_t i, ret;
	uint32_t cnt1, cnt2;
	int64_t time_start, time_end;

	uint32_t cnt_w = 0;
	uint32_t cnt_r = 0;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PRESURE_REFINED");

	ret = CameraIsCaptureStart(camera, &is_capture_start);
	UNLIKE_RETURN(ret, 0, "IsCaptureStart failed: %" PRIu32, ret);
	if (is_capture_start == false) {
		LOG_I(__FUNCTION__, "CEU already start, try to start");
		ret = CameraStartCapture(camera, s_dvp_image);
		UNLIKE_RETURN(ret, 0, "StartCapture failed: %" PRIu32, ret);
	}

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	CameraGetCaptureCnt(camera, &cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(camera, &cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_w[cnt_w], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
		cnt_w++;
		if (cnt_w >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_end = get_system_ms();
	printf("============ Write speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_w);
	printf("SRAM ==> SDRAM, 16bit width\r\n");
	for (i = 0; i < cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
		if ((i % 10) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetCaptureCnt(camera, &cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_r[cnt_r], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
		cnt_r++;
		if (cnt_r >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(camera, &cnt1);
	}
	time_end = get_system_ms();
	printf("============ Read speed summury  ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_r);
	printf("SDRAM ==> SRAM, 16bit width\r\n");
	for (i = 0; i < cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
		if ((i % 5) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	return 0;
}

/**
 * @brief	测试在 CEU 和 VIN 都启动时的 SDRAM 读写速度
 * @note	务必确保进入此函数前相机已开启捕获，与单个相机的 testCameraPresureRefined() 不同，此函数不会主动启动捕获
 * @param	camera1 相机 1 实例
 * @param	camera2 相机 2 实例
 * @param	which	以哪个相机实例的帧为基准，1 为相机 1，其它为相机 2
 * @return	总是返回 0
 */
static uint32_t testCameraPresureRefined2(Camera *camera1, Camera *camera2, uint8_t which)
{
	uint32_t i;
	volatile uint32_t cnt1, cnt2;
	int64_t time_start, time_end;
	CameraDVPErrorCountType dvp_error_cnt_old;
	CameraDVPErrorCountType dvp_error_cnt_new;
	CameraMIPIErrrorCountType mipi_error_cnt_old;
	CameraMIPIErrrorCountType mipi_error_cnt_new;

	uint32_t cnt_w = 0;
	uint32_t cnt_r = 0;
	Camera *cam = NULL;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_PRESURE_REFINED");

	if ((camera1->interface != CAMERA_INTERFACE_DVP) && (camera2->interface != CAMERA_INTERFACE_MIPI_CSI)) {
		LOG_E(__FUNCTION__, "camera1 must using DVP and camera2 must using MIPI");
		return FSP_ERR_ASSERTION;
	}

	memset(s_speed_sum_r, 0, sizeof(s_speed_sum_r));
	memset(s_speed_sum_w, 0, sizeof(s_speed_sum_w));

	if (which == 1) {
		cam = camera1;
	}
	else {
		cam = camera2;
	}

	CameraGetDVPErrorCnt(camera1, &dvp_error_cnt_old);
	CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt_old);

	CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_w[cnt_w], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SRAM_TO_SDRAM);
		cnt_w++;
		if (cnt_w >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}
	time_end = get_system_ms();

	printf("============ Write speed summury ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_w);
	printf("SRAM ==> SDRAM, 16bit width\r\n");
	for (i = 0; i < cnt_w; i++) {
		printf("%.2f\r\n", s_speed_sum_w[i]);
		if ((i % 10) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}

	time_start = get_system_ms();
	cnt2 = cnt1;
	while (cnt1 == cnt2) {
		TestSDRAM_Speed(&s_speed_sum_r[cnt_r], TEST_SDRAM_WIDTH_16BIT, TEST_SDRAM_DIR_SDRAM_TO_SRAM);
		cnt_r++;
		if (cnt_r >= CAMERA_PRESURE_REFINED_SIZE) {
			LOG_W(__FUNCTION__, "CAMERA_PRESURE_REFINED_SIZE is too small, increase it and rerun");
			break;
		}
		CameraGetCaptureCnt(cam, (uint32_t *)&cnt1);
	}
	time_end = get_system_ms();

	printf("============ Read speed summury  ============\r\n");
	printf("Total running time: %" PRIu32 "ms\r\n", (uint32_t)(time_end - time_start));
	printf("Total running count: %" PRIu32 "\r\n", cnt_r);
	printf("SDRAM ==> SRAM, 16bit width\r\n");
	for (i = 0; i < cnt_r; i++) {
		printf("%.2f\r\n", s_speed_sum_r[i]);
		if ((i % 5) == 0) {
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		}
	}

	CameraGetDVPErrorCnt(camera1, &dvp_error_cnt_new);
	CameraGetMIPIErrorCnt(camera2, &mipi_error_cnt_new);
	printf("============ New CEU error ============\r\n");
	printf("firewall   : %" PRIu32 "\r\n", dvp_error_cnt_new.firewall - dvp_error_cnt_old.firewall);
	printf("hd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt_new.hd_mismatch - dvp_error_cnt_old.hd_mismatch);
	printf("hd_missing : %" PRIu32 "\r\n", dvp_error_cnt_new.hd_missing - dvp_error_cnt_old.hd_missing);
	printf("overflow   : %" PRIu32 "\r\n", dvp_error_cnt_new.overflow - dvp_error_cnt_old.overflow);
	printf("vd_error   : %" PRIu32 "\r\n", dvp_error_cnt_new.vd_error - dvp_error_cnt_old.vd_error);
	printf("vd_mismatch: %" PRIu32 "\r\n", dvp_error_cnt_new.vd_mismatch - dvp_error_cnt_old.vd_mismatch);
	printf("vd_missing : %" PRIu32 "\r\n", dvp_error_cnt_new.vd_missing - dvp_error_cnt_old.vd_missing);
	printf("============ New VIN error ============\r\n");
	printf("fifo_overflow: %" PRIu32 "\r\n", mipi_error_cnt_new.fifo_overflow - mipi_error_cnt_old.fifo_overflow);
	printf("preclip_h_err: %" PRIu32 "\r\n", mipi_error_cnt_new.preclip_h_err - mipi_error_cnt_old.preclip_h_err);
	printf("preclip_v_err: %" PRIu32 "\r\n", mipi_error_cnt_new.preclip_v_err - mipi_error_cnt_old.preclip_v_err);
	printf("resp_overflow: %" PRIu32 "\r\n", mipi_error_cnt_new.resp_overflow - mipi_error_cnt_old.resp_overflow);
	printf("axi_err      : %" PRIu32 "\r\n", mipi_error_cnt_new.axi_err - mipi_error_cnt_old.axi_err);

	return 0;
}
#endif

#if TEST_CAMERA_EN_SINGLE_CAPTURE
uint32_t testCameraSingleCapture(Camera *camera)
{
	uint32_t ret;

	uint8_t cnt = 0;

	LOG_I(__FUNCTION__, "TEST_CAMERA_EN_SINGLE_CAPTURE");

	ret = CameraStartCapture(camera, s_dvp_image);
	UNLIKE_RETURN(ret, 0, "StartCapture failed: %" PRIu32, ret);

	while (cnt < 1) {
		ret = CameraIsCaptureDone(camera);
		while (ret == 0) {
			R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
			ret = CameraIsCaptureDone(camera);
		}
		cnt++;
	}

	LOG_D(__FUNCTION__, "Capture done, check: 0x%" PRIX32, (uint32_t)s_dvp_image);
	__BKPT(0);

	return 0;
}
#endif

#endif /* #if TEST_EN_CAMERA */
