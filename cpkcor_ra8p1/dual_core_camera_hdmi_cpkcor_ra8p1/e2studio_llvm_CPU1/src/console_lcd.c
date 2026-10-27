#include "console.h"

#if CONSOLE_CFG_USE_LCD

#include "arm_2d_disp_adapter_0.h"
#include "arm_2d_helper_scene.h"
#include "arm_2d_types.h"
#include "console_box.h"

#include "lcd/picture.h"

#ifndef CONSOLE_LCD_BUFFER_SIZE
#define CONSOLE_LCD_BUFFER_SIZE			1024
#endif

#ifndef CONSOLE_LCD_INPUT_SIZE
#define CONSOLE_LCD_INPUT_SIZE			512
#endif

static arm_2d_scene_t s_console_scene;
static console_box_t s_console_box;
static uint8_t s_input_buffer[CONSOLE_LCD_INPUT_SIZE];
static uint8_t s_console_buffer[CONSOLE_LCD_BUFFER_SIZE];
/* console 宽度应是使用字体的宽度的整数倍，否则在右侧边界显示不全 */
static uint32_t s_console_width = 512;
static uint32_t s_console_height = 512;

static void afterSwitch(arm_2d_scene_t *ptThis);
static void fnDepose(arm_2d_scene_t *ptThis);
static arm_fsm_rt_t fnScene(void *pTarget, const arm_2d_tile_t *ptTile, bool bIsNewFrame);
static void onFrameCPL(arm_2d_scene_t *ptThis);
static void onFrameStart(arm_2d_scene_t *ptThis);
static void onBGComplete(arm_2d_scene_t *ptThis);
static void onBGStart(arm_2d_scene_t *ptThis);
static void onLoad(arm_2d_scene_t *ptThis);

void CONSOLE_LCD_Init(void)
{
	console_box_cfg_t cfg = {0};
	
	memset(&s_console_scene, 0, sizeof(s_console_scene));
	
	s_console_scene.tCanvas = (arm_2d_colour_t){GLCD_COLOR_WHITE};
	s_console_scene.fnScene = fnScene;
	s_console_scene.fnOnLoad = onLoad;
	s_console_scene.fnAfterSwitch = afterSwitch;
	s_console_scene.fnOnBGStart = onBGStart;
	s_console_scene.fnOnBGComplete = onBGComplete;
	s_console_scene.fnOnFrameStart = onFrameStart;
	s_console_scene.fnOnFrameCPL = onFrameCPL;
	s_console_scene.fnDepose = fnDepose;
	s_console_scene.bUseDirtyRegionHelper = false;
	
	cfg.tBoxSize = (arm_2d_size_t){s_console_width, s_console_height};
	cfg.pchConsoleBuffer = s_console_buffer;
	cfg.hwConsoleBufferSize = sizeof(s_console_buffer);
	cfg.pchInputBuffer = s_input_buffer;
	cfg.hwInputBufferSize = sizeof(s_input_buffer);
	cfg.ptFont = &ARM_2D_FONT_16x24.use_as__arm_2d_font_t;
	cfg.tColor = GLCD_COLOR_GREEN;
	cfg.bUseDirtyRegion = true;
	
	console_box_init(&s_console_box, &s_console_scene, &cfg);
	arm_2d_scene_player_append_scenes(&DISP0_ADAPTER, &s_console_scene, 1);
}

void CONSOLE_LCD_Putchar(char ch)
{
	if (ch == '\n') {
		console_box_putchar(&s_console_box, '\r');
	}
	console_box_putchar(&s_console_box, (uint8_t)ch);
}

static void afterSwitch(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
}

static void fnDepose(arm_2d_scene_t *ptThis)
{
	console_box_depose(&s_console_box);
	ptThis->ptPlayer = NULL;
}

static arm_fsm_rt_t fnScene(void *pTarget, const arm_2d_tile_t *ptTile, bool bIsNewFrame)
{
	arm_2d_region_t console_region;
	arm_2d_region_t picture_region;

	arm_2d_tile_t picture_tile = {0};

	picture_region.tLocation.iX = 0;
	picture_region.tLocation.iY = 0;
	picture_region.tSize.iWidth = g_picture_520x720_01.width;
	picture_region.tSize.iHeight = g_picture_520x720_01.height;

	picture_tile.tRegion.tSize.iWidth = (int16_t)g_picture_520x720_01.width;
	picture_tile.tRegion.tSize.iHeight = (int16_t)g_picture_520x720_01.height;
	picture_tile.tInfo.bIsRoot = true;
	picture_tile.tInfo.bHasEnforcedColour = true;
	picture_tile.tInfo.tColourInfo.chScheme = ARM_2D_COLOUR_RGB565;
	picture_tile.phwBuffer = (uint16_t *)g_picture_520x720_01.pixel_data;

	arm_2d_canvas(ptTile, __top_canvas) {
		arm_2d_rgb16_tile_copy_only(&picture_tile, ptTile, &picture_region);
		arm_2d_op_wait_async(NULL);

		console_region.tLocation.iX = (__top_canvas.tSize.iWidth - s_console_width) / 2;
		console_region.tLocation.iY = 144;
		console_region.tSize.iWidth = s_console_width;
		console_region.tSize.iHeight = s_console_height;
		arm_2d_fill_colour_with_opacity(ptTile, &console_region, (__arm_2d_color_t){GLCD_COLOR_BLACK}, 128);
		arm_2d_op_wait_async(NULL);
		console_box_show(&s_console_box, ptTile, &console_region, bIsNewFrame, 255);

		arm_lcd_text_set_target_framebuffer((arm_2d_tile_t *)ptTile);
		arm_lcd_text_set_font(&ARM_2D_FONT_16x24.use_as__arm_2d_font_t);
		arm_lcd_text_set_draw_region(NULL);
		arm_lcd_text_set_colour(GLCD_COLOR_GREEN, GLCD_COLOR_WHITE);
		arm_lcd_text_location(5, 0);
		arm_lcd_puts("ARM2D Console");
	}
	arm_2d_op_wait_async(NULL);

	return arm_fsm_rt_cpl;
}

static void onFrameCPL(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
}

static void onFrameStart(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
	
	console_box_on_frame_start(&s_console_box);
}

static void onBGComplete(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
}

static void onBGStart(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
}

static void onLoad(arm_2d_scene_t *ptThis)
{
	(void)ptThis;
}

#endif
