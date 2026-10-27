#include "lcd_ili9881c.h"

#if LCD_EN_ILI9881C

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "hal_data.h"
#include "utils/util.h"

/*================================== MACROS =======================================================*/

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 1->Framebuffer->Number of framebuffers */
#ifndef ILI9881C_LAYER_1_CACHE_NUM
#define ILI9881C_LAYER_1_CACHE_NUM			1
#endif

/* For RGB and MIPI interface, must match FSP configuration.xml:
 * r_glcdc->Input->Graphics Layer 2->Framebuffer->Number of framebuffers.
 * Set to 0 only when Graphics Layer 2 is also disabled in FSP. */
#ifndef ILI9881C_LAYER_2_CACHE_NUM
#define ILI9881C_LAYER_2_CACHE_NUM			1
#endif

#define ILI9881C_PIN_BACKLIGHT				BSP_IO_PORT_00_PIN_12
#define ILI9881C_PIN_RESET					BSP_IO_PORT_00_PIN_13

#define MIPI_CMD_ID_DELAY					((mipi_cmd_id_t)0xFE)

#ifndef __ILI9881C_DEBUG
#define __ILI9881C_DEBUG	1
#endif

#if (ILI9881C_LAYER_1_CACHE_NUM != 1) && (ILI9881C_LAYER_1_CACHE_NUM != 2)
#error "ILI9881C_LAYER_1_CACHE_NUM must be 1 or 2"
#endif

#if (ILI9881C_LAYER_2_CACHE_NUM < 0) || (ILI9881C_LAYER_2_CACHE_NUM > 2)
#error "ILI9881C_LAYER_2_CACHE_NUM must be 0, 1 or 2"
#endif

#if __ILI9881C_DEBUG
#include "perf_counter/perf_counter.h"
#include "utils/log.h"
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define ILI9881C_LOGD(msg, ...)             LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define ILI9881C_LOGI(msg, ...)             LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define ILI9881C_LOGW(msg, ...)             LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define ILI9881C_LOGE(msg, ...)             LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define UNLIKE_RETURN(v, t, msg, ...)       if (v != t) { return v; }
#define ILI9881C_LOGD(msg, ...)
#define ILI9881C_LOGI(msg, ...)
#define ILI9881C_LOGW(msg, ...)
#define ILI9881C_LOGE(msg, ...)
#endif

/*================================== TYPES ========================================================*/

struct mipi_init_table {
	uint8_t size;
	uint8_t buffer[10];
	mipi_cmd_id_t cmd_id;
	mipi_dsi_cmd_flag_t flags;
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static ioport_instance_ctrl_t s_pin_ctrl;
static ioport_cfg_t s_pin_cfg;
static ioport_pin_cfg_t s_pin_cfg_data[2];
static display_runtime_cfg_t s_layer1_input_cfg;
static display_runtime_cfg_t s_layer2_input_cfg;

static const struct mipi_init_table sc_mipi_init_table_1[] = {
	{4, {0xFF, 0x98, 0x81, 0x03}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x01, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x02, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x03, 0x53}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x04, 0xD3}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x05, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x06, 0x0D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x07, 0x08}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x08, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x09, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0A, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0B, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0D, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0E, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x0F, 0x28}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x10, 0x28}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x11, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x12, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x13, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x14, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x15, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x16, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x17, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x18, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x19, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1A, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1B, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1D, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1E, 0x40}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x1F, 0x80}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x20, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x21, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x22, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x23, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x24, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x25, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x26, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x27, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x28, 0x33}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x29, 0x33}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2A, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2B, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2D, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2E, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x2F, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x30, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x31, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x32, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x33, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x34, 0x03}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x35, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x36, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x37, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x38, 0x96}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x39, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3A, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3B, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3D, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3E, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3F, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x40, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x41, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x42, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x43, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x44, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x50, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x51, 0x23}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x52, 0x45}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x53, 0x67}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x54, 0x89}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x55, 0xAB}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x56, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x57, 0x23}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x58, 0x45}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x59, 0x67}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5A, 0x89}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5B, 0xAB}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5C, 0xCD}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5D, 0xEF}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5E, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x5F, 0x08}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x60, 0x08}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x61, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x62, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x63, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x64, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x65, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x66, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x67, 0x02}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x68, 0x15}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x69, 0x15}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6A, 0x14}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6B, 0x14}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6C, 0x0D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6D, 0x0D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6E, 0x0C}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6F, 0x0C}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x70, 0x0F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x71, 0x0F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x72, 0x0E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x73, 0x0E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x74, 0x02}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x75, 0x08}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x76, 0x08}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x77, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x78, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x79, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7A, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7B, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7C, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7D, 0x02}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7E, 0x15}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x7F, 0x15}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x80, 0x14}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x81, 0x14}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x82, 0x0D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x83, 0x0D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x84, 0x0C}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x85, 0x0C}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x86, 0x0F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x87, 0x0F}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x88, 0x0E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x89, 0x0E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x8A, 0x02}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {4, {0xFF, 0x98, 0x81, 0x04}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6E, 0x2B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x6F, 0x37}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3A, 0x24}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x8D, 0x1A}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x87, 0xBA}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB2, 0xD1}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x88, 0x0B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x38, 0x01}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x39, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB5, 0x02}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x31, 0x25}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x3B, 0x98}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {4, {0xFF, 0x98, 0x81, 0x01}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x22, 0x0A}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x31, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x53, 0x3D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x55, 0x3D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x50, 0xB5}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x51, 0xAD}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x60, 0x06}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x62, 0x20}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB7, 0x03}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA0, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA1, 0x21}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA2, 0x35}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA3, 0x19}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA4, 0x1E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA5, 0x33}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA6, 0x27}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA7, 0x26}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA8, 0xAF}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xA9, 0x1B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAA, 0x27}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAB, 0x8D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAC, 0x1A}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAD, 0x1B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAE, 0x50}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xAF, 0x26}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB0, 0x2B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB1, 0x54}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB2, 0x5E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xB3, 0x23}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC0, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC1, 0x21}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC2, 0x35}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC3, 0x19}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC4, 0x1E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC5, 0x33}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC6, 0x27}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC7, 0x26}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC8, 0xAF}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xC9, 0x1B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCA, 0x27}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCB, 0x8D}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCC, 0x1A}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCD, 0x1B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCE, 0x50}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xCF, 0x26}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xD0, 0x2B}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xD1, 0x54}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xD2, 0x5E}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0xD3, 0x23}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {4, {0xFF, 0x98, 0x81, 0x00}, MIPI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
};

static const struct mipi_init_table sc_mipi_init_table_2[] = {
	{2, {0x11, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x29, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2, {0x35, 0x00}, MIPI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
};

static volatile bool s_mipi_cmd_tx_done;

/*================================== Private Functions Prototypes =================================*/

static uint32_t pinConfig(void);
static void glcdcCallback(display_callback_args_t *p_args);
static void mipiCallback(mipi_dsi_callback_args_t *p_args);

/*================================== Public Functions =============================================*/

/**
 * @brief	初始化 ILI9881C、GLCDC 和 MIPI DSI，绘图及提交使用 lcd.c 的默认实现。
 * @param	lcd LCD 实例，peri_1 为 GLCDC，peri_2 为 MIPI DSI。
 * @return	成功返回 FSP_SUCCESS，失败返回参数、配置或底层驱动错误码。
 */
uint32_t LCD_ILI9881C_Init(LCD_DeviceType *lcd)
{
	uint32_t err;
	uint32_t close_err;
	uint32_t frame_bytes;
	size_t i, init_table_len;
	mipi_dsi_cmd_t msg;

	if (lcd == NULL) {
		return FSP_ERR_ASSERTION;
	}
	if ((lcd->peri_model_1 != LCD_PERI_GLCDC) || (lcd->peri_model_2 != LCD_PERI_MIPI)) {
		return FSP_ERR_UNSUPPORTED;
	}
	if ((lcd->peri_1 == NULL) || (lcd->peri_2 == NULL)) {
		return FSP_ERR_ASSERTION;
	}

	const display_instance_t *p_glcdc = (const display_instance_t *)lcd->peri_1;
	const mipi_dsi_instance_t *p_mipi = (const mipi_dsi_instance_t *)lcd->peri_2;
	if ((p_glcdc->p_ctrl == NULL) || (p_glcdc->p_cfg == NULL) || (p_glcdc->p_api == NULL) ||
		(p_mipi->p_ctrl == NULL) || (p_mipi->p_cfg == NULL) || (p_mipi->p_api == NULL)) {
		return FSP_ERR_ASSERTION;
	}
	glcdc_instance_ctrl_t *p_glcdc_ctrl = (glcdc_instance_ctrl_t *)p_glcdc->p_ctrl;
	mipi_dsi_instance_ctrl_t *p_mipi_ctrl = (mipi_dsi_instance_ctrl_t *)p_mipi->p_ctrl;
	const display_input_cfg_t *p_input = &p_glcdc->p_cfg->input[0];

	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		return FSP_ERR_ASSERTION;
	}

	err = pinConfig();
	UNLIKE_RETURN(err, FSP_SUCCESS, "Pin config failed: %" PRIu32, err);
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(ILI9881C_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinWrite(ILI9881C_PIN_BACKLIGHT, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);

	if (p_glcdc_ctrl->state != DISPLAY_STATE_CLOSED) {
		ILI9881C_LOGI("GLCDC already open");
		err = p_glcdc->p_api->close(p_glcdc_ctrl);
		UNLIKE_RETURN(err, FSP_SUCCESS, "Close failed: %" PRIu32, err);
	}
	err = p_glcdc->p_api->open(p_glcdc->p_ctrl, p_glcdc->p_cfg);
	UNLIKE_RETURN(err, FSP_SUCCESS, "Open failed: %" PRIu32, err);
	p_glcdc_ctrl->p_callback = glcdcCallback;
	p_glcdc_ctrl->p_context = lcd;
	p_mipi_ctrl->p_callback = mipiCallback;
	p_mipi_ctrl->p_context = lcd;

	R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	msg.channel = 0;
	msg.p_rx_buffer = NULL;
	init_table_len = sizeof(sc_mipi_init_table_1) / sizeof(sc_mipi_init_table_1[0]);
	for (i = 0; i < init_table_len; i++) {
		msg.cmd_id = sc_mipi_init_table_1[i].cmd_id;
		msg.flags = sc_mipi_init_table_1[i].flags;
		msg.p_tx_buffer = sc_mipi_init_table_1[i].buffer;
		msg.tx_len = sc_mipi_init_table_1[i].size;
		s_mipi_cmd_tx_done = false;
		err = p_mipi->p_api->command(p_mipi->p_ctrl, &msg);
		UNLIKE_RETURN(err, 0, "MIPI Command Error: %" PRIu32, err);
		while (s_mipi_cmd_tx_done == false) {
		#ifdef __OPTIMIZE__
			__nop();
		#endif
		}
		R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	}
	R_BSP_SoftwareDelay(120, BSP_DELAY_UNITS_MILLISECONDS);
	init_table_len = sizeof(sc_mipi_init_table_2) / sizeof(sc_mipi_init_table_2[0]);
	for (i = 0; i < init_table_len; i++) {
		msg.cmd_id = sc_mipi_init_table_2[i].cmd_id;
		msg.flags = sc_mipi_init_table_2[i].flags;
		msg.p_tx_buffer = sc_mipi_init_table_2[i].buffer;
		msg.tx_len = sc_mipi_init_table_2[i].size;
		s_mipi_cmd_tx_done = false;
		err = p_mipi->p_api->command(p_mipi->p_ctrl, &msg);
		UNLIKE_RETURN(err, 0, "MIPI Command Error: %" PRIu32, err);
		while (s_mipi_cmd_tx_done == false) {
		#ifdef __OPTIMIZE__
			__nop();
		#endif
		}
		R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MILLISECONDS);
	}
	R_BSP_SoftwareDelay(120, BSP_DELAY_UNITS_MILLISECONDS);

	lcd->orientation = LCD_ORIENTATION_VERTICAL;
	lcd->vsync_cnt = 0;
	lcd->gr1_underflow_cnt = 0;
	lcd->gr2_underflow_cnt = 0;
	lcd->mipi_timing_err_cnt = 0;
	lcd->mipi_underflow_cnt = 0;
	lcd->mipi_overflow_cnt = 0;
	memset(&lcd->layer_1, 0, sizeof(lcd->layer_1));
	memset(&lcd->layer_2, 0, sizeof(lcd->layer_2));

	lcd->layer_1.index = 1;
	lcd->layer_1.x_length = p_input->hsize;
	lcd->layer_1.x_dummy = (uint16_t)(p_input->hstride - p_input->hsize);
	lcd->layer_1.y_length = p_input->vsize;
	lcd->layer_1.y_dummy = 0;

	switch (p_input->format) {
	case DISPLAY_IN_FORMAT_32BITS_ARGB8888:
		lcd->layer_1.bytes_per_pixel = 4;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB8888;
		break;
	case DISPLAY_IN_FORMAT_32BITS_RGB888:
		lcd->layer_1.bytes_per_pixel = 4;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_RGB888;
		break;
	case DISPLAY_IN_FORMAT_16BITS_RGB565:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_RGB565;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB1555:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB1555;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB4444:
		lcd->layer_1.bytes_per_pixel = 2;
		lcd->layer_1.color_format = LCD_COLOR_FORMAT_ARGB4444;
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		goto CLOSE_GLCDC;
	}

	frame_bytes = p_input->hstride * p_input->vsize * lcd->layer_1.bytes_per_pixel;
	lcd->layer_1.graphic_mem_1 = (uint8_t *)p_input->p_base;
	lcd->layer_1.mem1_read_only = false;
#if ILI9881C_LAYER_1_CACHE_NUM == 1
	lcd->layer_1.graphic_mem_2 = NULL;
	lcd->layer_1.mem2_read_only = true;
	lcd->layer_1.graphic_writeable = lcd->layer_1.graphic_mem_1;
#else
	lcd->layer_1.graphic_mem_2 = &lcd->layer_1.graphic_mem_1[frame_bytes];
	lcd->layer_1.mem2_read_only = false;
	lcd->layer_1.graphic_writeable = lcd->layer_1.graphic_mem_2;
#endif
	lcd->layer_1.graphic_displayed = lcd->layer_1.graphic_mem_1;
	lcd->layer_1.graphic_to_present = NULL;
	lcd->layer_1.graphic_submitted = NULL;
	lcd->layer_1.dirty_current.valid = false;
	lcd->layer_1.dirty_submitted.valid = false;
	lcd->layer_1.present_pending = false;
	memset(lcd->layer_1.graphic_mem_1, 0xFF, frame_bytes);
	if (lcd->layer_1.graphic_mem_2 != NULL) {
		memset(lcd->layer_1.graphic_mem_2, 0xFF, frame_bytes);
	}

	/* Layer 2 未配置时保留为空，并清除重复初始化前的状态。 */
	lcd->layer_2.index = 2;
	lcd->layer_2.mem1_read_only = true;
	lcd->layer_2.mem2_read_only = true;
#if ILI9881C_LAYER_2_CACHE_NUM
	p_input = &p_glcdc->p_cfg->input[1];
	if ((p_input->p_base == NULL) || (p_input->hsize == 0) || (p_input->vsize == 0) ||
		(p_input->hstride < p_input->hsize) || (p_input->hstride > UINT16_MAX)) {
		ILI9881C_LOGW("Layer 2 setting has error, ignore");
		goto START_GLCDC;
	}
	lcd->layer_2.x_length = p_input->hsize;
	lcd->layer_2.x_dummy = (uint16_t)(p_input->hstride - p_input->hsize);
	lcd->layer_2.y_length = p_input->vsize;
	lcd->layer_2.y_dummy = 0;

	switch (p_input->format) {
	case DISPLAY_IN_FORMAT_32BITS_ARGB8888:
		lcd->layer_2.bytes_per_pixel = 4;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB8888;
		break;
	case DISPLAY_IN_FORMAT_32BITS_RGB888:
		lcd->layer_2.bytes_per_pixel = 4;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_RGB888;
		break;
	case DISPLAY_IN_FORMAT_16BITS_RGB565:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_RGB565;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB1555:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB1555;
		break;
	case DISPLAY_IN_FORMAT_16BITS_ARGB4444:
		lcd->layer_2.bytes_per_pixel = 2;
		lcd->layer_2.color_format = LCD_COLOR_FORMAT_ARGB4444;
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		goto CLOSE_GLCDC;
	}

	frame_bytes = p_input->hstride * p_input->vsize * lcd->layer_2.bytes_per_pixel;
	lcd->layer_2.graphic_mem_1 = (uint8_t *)p_input->p_base;
	lcd->layer_2.mem1_read_only = false;
#if ILI9881C_LAYER_2_CACHE_NUM == 1
	lcd->layer_2.graphic_mem_2 = NULL;
	lcd->layer_2.mem2_read_only = true;
	lcd->layer_2.graphic_writeable = lcd->layer_2.graphic_mem_1;
#else
	lcd->layer_2.graphic_mem_2 = &lcd->layer_2.graphic_mem_1[frame_bytes];
	lcd->layer_2.mem2_read_only = false;
	lcd->layer_2.graphic_writeable = lcd->layer_2.graphic_mem_2;
#endif
	lcd->layer_2.graphic_displayed = lcd->layer_2.graphic_mem_1;
	lcd->layer_2.graphic_to_present = NULL;
	lcd->layer_2.graphic_submitted = NULL;
	lcd->layer_2.dirty_current.valid = false;
	lcd->layer_2.dirty_submitted.valid = false;
	lcd->layer_2.present_pending = false;
	memset(lcd->layer_2.graphic_mem_1, 0xFF, frame_bytes);
	if (lcd->layer_2.graphic_mem_2 != NULL) {
		memset(lcd->layer_2.graphic_mem_2, 0xFF, frame_bytes);
	}
#endif /* #if ILI9881C_LAYER_2_CACHE_NUM */

#if ILI9881C_LAYER_2_CACHE_NUM
START_GLCDC:
#endif
	lcd->layer_1.runtime_cfg = &s_layer1_input_cfg;
	s_layer1_input_cfg.input = p_glcdc->p_cfg->input[0];
	s_layer1_input_cfg.layer = p_glcdc->p_cfg->layer[0];
	s_layer1_input_cfg.input.p_base = (uint32_t *)lcd->layer_1.graphic_displayed;
	lcd->layer_2.runtime_cfg = &s_layer2_input_cfg;
	s_layer2_input_cfg.input = p_glcdc->p_cfg->input[1];
	s_layer2_input_cfg.layer = p_glcdc->p_cfg->layer[1];
	s_layer2_input_cfg.input.p_base = (uint32_t *)lcd->layer_2.graphic_displayed;

	__DSB();
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	err = p_glcdc->p_api->start(p_glcdc_ctrl);
	if (err != FSP_SUCCESS) {
		ILI9881C_LOGE("Start failed: %" PRIu32, err);
		goto CLOSE_GLCDC;
	}

	return err;

CLOSE_GLCDC:
	close_err = p_glcdc->p_api->close(p_glcdc_ctrl);
	if (close_err != FSP_SUCCESS) {
		ILI9881C_LOGE("Close after init failure failed: %" PRIu32, close_err);
	}
	return err;
}

/*================================== Private Functions ============================================*/

static uint32_t pinConfig(void)
{
	s_pin_cfg_data[0].pin = ILI9881C_PIN_BACKLIGHT;
	s_pin_cfg_data[0].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg_data[1].pin = ILI9881C_PIN_RESET;
	s_pin_cfg_data[1].pin_cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_HIGH;
	s_pin_cfg.number_of_pins = 2;
	s_pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&s_pin_cfg_data;
	s_pin_cfg.p_extend = NULL;

	uint32_t err = R_IOPORT_Open(&s_pin_ctrl, &s_pin_cfg);
	if (err == FSP_ERR_ALREADY_OPEN) {
		err = R_IOPORT_PinsCfg(&s_pin_ctrl, &s_pin_cfg);
	}
	return err;
}

static void glcdcCallback(display_callback_args_t *p_args)
{
	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case DISPLAY_EVENT_GR1_UNDERFLOW:
		lcd->gr1_underflow_cnt++;
		break;
	case DISPLAY_EVENT_GR2_UNDERFLOW:
		lcd->gr2_underflow_cnt++;
		break;
	case DISPLAY_EVENT_LINE_DETECTION:
		lcd->vsync_cnt++;
		/* 两层独立完成交接，一层尚未生效时仍检查另一层。 */
		for (uint8_t i = 0; i < 2; i++) {
			LCD_LayerType *layer = (i == 0) ? &lcd->layer_1 : &lcd->layer_2;
			if (layer->present_pending == false) {
				continue;
			}

			uint8_t *submitted = layer->graphic_submitted;
			/* LCD_MoveLayer() 不会动显存，所以 graphic_submitted 会为 NULL。 */
			if (submitted == NULL) {
				__DMB();
				layer->present_pending = false;
				continue;
			}
			/* 与 TFP410 相同，以对应层的 FLM2 地址确认提交目标。 */
			if (R_GLCDC->GR[i].FLM2 != (uintptr_t)submitted) {
				continue;
			}

			layer->graphic_displayed = submitted;
			if ((layer->graphic_mem_1 != NULL) && (layer->mem1_read_only == false) &&
				((layer->graphic_mem_1 != submitted) || (layer->graphic_mem_2 == NULL))) {
				layer->graphic_writeable = layer->graphic_mem_1;
			}
			else if ((layer->graphic_mem_2 != NULL) && (layer->mem2_read_only == false) &&
				((layer->graphic_mem_2 != submitted) || (layer->graphic_mem_1 == NULL))) {
				layer->graphic_writeable = layer->graphic_mem_2;
			}
			else {
				layer->graphic_writeable = NULL;
			}
			if ((layer->graphic_writeable == NULL) || (layer->graphic_writeable == submitted)) {
				layer->dirty_submitted.valid = false;
			}
			layer->graphic_submitted = NULL;
			display_runtime_cfg_t *p_runtime = (display_runtime_cfg_t *)layer->runtime_cfg;
			if (p_runtime != NULL) {
				p_runtime->input.p_base = (uint32_t *)layer->graphic_displayed;
			}

			__DMB();
			layer->present_pending = false;
		}
		break;
	case DISPLAY_EVENT_FRAME_END:
		break;
	}
}

static void mipiCallback(mipi_dsi_callback_args_t *p_args)
{
	uint32_t status;

	LCD_DeviceType *lcd = (LCD_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case MIPI_DSI_EVENT_SEQUENCE_0:
		s_mipi_cmd_tx_done = true;
		break;
	case MIPI_DSI_EVENT_SEQUENCE_1:
		break;
	case MIPI_DSI_EVENT_VIDEO:
		status = (uint32_t)p_args->video_status;
		if (status & MIPI_DSI_VIDEO_STATUS_TIMING_ERROR) {
			lcd->mipi_timing_err_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_UNDERFLOW) {
			lcd->mipi_underflow_cnt++;
		}
		if (status & MIPI_DSI_VIDEO_STATUS_OVERFLOW) {
			lcd->mipi_overflow_cnt++;
		}
		break;
	case MIPI_DSI_EVENT_RECEIVE:
		break;
	case MIPI_DSI_EVENT_FATAL:
		break;
	case MIPI_DSI_EVENT_PHY:
		break;
	case MIPI_DSI_EVENT_POST_OPEN:
		break;
	case MIPI_DSI_EVENT_PRE_START:
		break;
	}
}

#endif /* #if LCD_EN_ILI9881C */
