#include "ov7725.h"

#if CAMERA_EN_DVP

/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include "hal_data.h"
#include "iic.h"

/*================================== MACROS =======================================================*/

#ifndef __OV7725_DEBUG
#define __OV7725_DEBUG	1
#endif

#define OV7725_PIN_PWR_ON		BSP_IO_PORT_07_PIN_10
#define OV7725_PIN_RESET		BSP_IO_PORT_07_PIN_09

#define iicWrite(reg, val)		IIC_SCCB_WriteReg(OV7725_IIC_ADDR, reg, 8, val)
#define iicRead(reg, val)		IIC_SCCB_ReadReg(OV7725_IIC_ADDR, reg, 8, val)

#if __OV7725_DEBUG
#include "utils/log.h"

#define CHECK_IIC_READ(err, reg)		{ if (err) { LOG_E(__FUNCTION__, "Read reg 0x%02X failed: %u", reg, err); return err; } }
#define CHECK_IIC_WRITE(err, reg)		{ if (err) { LOG_E(__FUNCTION__, "Write reg 0x%02X failed: %u", reg, err); return err; } }
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define OV7725_LOGD(msg, ...)			LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV7725_LOGI(msg, ...)			LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV7725_LOGW(msg, ...)			LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define OV7725_LOGE(msg, ...)			LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define CHECK_IIC_READ(err, reg)		{ if (err) { return err; } }
#define CHECK_IIC_WRITE(err, reg)		{ if (err) { return err; } }
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { return v; }
#define OV7725_LOGD(msg, ...)
#define OV7725_LOGI(msg, ...)
#define OV7725_LOGW(msg, ...)
#define OV7725_LOGE(msg, ...)
#endif

#define IIC_READ_CHECK(reg, val)	{ err = iicRead(reg, val); CHECK_IIC_READ(err, reg); }
#define IIC_WRITE_CHECK(reg, val)	{ err = iicWrite(reg, val); CHECK_IIC_WRITE(err, reg); }
#define IS_INVALID_POINT(p)			if (p == NULL) { return FSP_ERR_INVALID_POINTER; }

#define XCLK_FREQ					12000000
#define MAX_PCLK_FREQ                48000000U /* 调帧使用的 PCLK 上限，支持 VGA 60 FPS */
#define GAIN                        0x00 /* AGC - Gain control gain setting  */
#define BLUE                        0x01 /* AWB - Blue channel gain setting  */
#define RED                         0x02 /* AWB - Red channel gain setting   */
#define GREEN                       0x03 /* AWB - Green channel gain setting */
#define BAVG                        0x05 /* U/B Average Level  */
#define GAVG                        0x06 /* Y/Gb Average Level */
#define RAVG                        0x07 /* V/R Average Level  */
#define AECH                        0x08 /* Exposure Value - AEC MSBs */

#define COM2                        0x09 /* Common Control 2 */
#define COM2_SOFT_SLEEP             0x10 /* Soft sleep mode  */
#define COM2_OUT_DRIVE_1x           0x00 /* Output drive capability 1x */
#define COM2_OUT_DRIVE_2x           0x01 /* Output drive capability 2x */
#define COM2_OUT_DRIVE_3x           0x02 /* Output drive capability 3x */
#define COM2_OUT_DRIVE_4x           0x03 /* Output drive capability 4x */

#define PID                         0x0A /* Product ID Number MSB */
#define VER                         0x0B /* Product ID Number LSB */

#define COM3                        0x0C /* Common Control 3                                       */
#define COM3_VFLIP                  0x80 /* Vertical flip image ON/OFF selection                   */
#define COM3_MIRROR                 0x40 /* Horizontal mirror image ON/OFF selection               */
#define COM3_SWAP_BR                0x20 /* Swap B/R output sequence in RGB output mode            */
#define COM3_SWAP_YUV               0x10 /* Swap Y/UV output sequence in YUV output mode           */
#define COM3_SWAP_MSB               0x08 /* Swap output MSB/LSB                                    */
#define COM3_TRI_CLOCK              0x04 /* Tri-state option for output clock at power-down period */
#define COM3_TRI_DATA               0x02 /* Tri-state option for output data at power-down period  */
#define COM3_COLOR_BAR              0x01 /* Sensor color bar test pattern output enable            */
#define COM3_SET_CBAR(r, x)         ((r & 0xFE) | ((x & 1) << 0))
#define COM3_SET_MIRROR(r, x)       ((r & 0xBF) | ((x & 1) << 6))
#define COM3_SET_FLIP(r, x)         ((r & 0x7F) | ((x & 1) << 7))
#define COM3_GET_CBAR(r)            ((r >> 0) & 1)
#define COM3_GET_MIRROR(r)          ((r >> 6) & 1)
#define COM3_GET_FLIP(r)            ((r >> 7) & 1)

#define COM4                        0x0D /* Common Control 4         */
#define COM4_PLL_BYPASS             0x00 /* Bypass PLL               */
#define COM4_PLL_4x                 0x40 /* PLL frequency 4x         */
#define COM4_PLL_6x                 0x80 /* PLL frequency 6x         */
#define COM4_PLL_8x                 0xC0 /* PLL frequency 8x         */
#define COM4_AEC_FULL               0x00 /* AEC evaluate full window */
#define COM4_AEC_1_2                0x10 /* AEC evaluate 1/2 window  */
#define COM4_AEC_1_4                0x20 /* AEC evaluate 1/4 window  */
#define COM4_AEC_2_3                0x30 /* AEC evaluate 2/3 window  */
#define COM4_GET_PLL(r)             (r & 0xC0)

#define COM5                        0x0E /* Common Control 5 */
#define COM5_AFR                    0x80 /* Auto frame rate control ON/OFF selection (night mode) */
#define COM5_AFR_SPEED              0x40 /* Auto frame rate control speed selection */
#define COM5_AFR_0                  0x00 /* No reduction of frame rate          */
#define COM5_AFR_1_2                0x10 /* Max reduction to 1/2 frame rate     */
#define COM5_AFR_1_4                0x20 /* Max reduction to 1/4 frame rate     */
#define COM5_AFR_1_8                0x30 /* Max reduction to 1/8 frame rate     */
#define COM5_AFR_4x                 0x04 /* Add frame when AGC reaches 4x gain  */
#define COM5_AFR_8x                 0x08 /* Add frame when AGC reaches 8x gain  */
#define COM5_AFR_16x                0x0c /* Add frame when AGC reaches 16x gain */
#define COM5_AEC_NO_LIMIT           0x01 /* No limit to AEC increase step       */
#define COM5_SET_AFR(r, x)          ((r & 0x7F) | ((x & 0x1) << 7))

#define COM6                        0x0F /* Common Control 6 */
#define COM6_AUTO_WINDOW            0x01 /* Auto window setting ON/OFF selection when format changes */

#define AEC                         0x10 /* AEC[7:0] (see register AECH for AEC[15:8]) */

#define CLKRC                       0x11 /* Internal Clock */
#define CLKRC_NO_PRESCALE           0x40 /* Use external clock directly */
#define CLKRC_PRESCALER             0x3F /* Internal clock pre-scaler */

#define COM7                        0x12 /* Common Control 7         */
#define COM7_RESET                  0x80 /* SCCB Register Reset      */
#define COM7_RES_VGA                0x00 /* Resolution VGA           */
#define COM7_RES_QVGA               0x40 /* Resolution QVGA          */
#define COM7_BT656                  0x20 /* BT.656 protocol ON/OFF   */
#define COM7_SENSOR_RAW             0x10 /* Sensor RAW               */
#define COM7_FMT_GBR422             0x00 /* RGB output format GBR422 */
#define COM7_FMT_RGB565             0x04 /* RGB output format RGB565 */
#define COM7_FMT_RGB555             0x08 /* RGB output format RGB555 */
#define COM7_FMT_RGB444             0x0C /* RGB output format RGB444 */
#define COM7_FMT_YUV                0x00 /* Output format YUV        */
#define COM7_FMT_P_BAYER            0x01 /* Output format Processed Bayer RAW */
#define COM7_FMT_RGB                0x02 /* Output format RGB        */
#define COM7_FMT_R_BAYER            0x03 /* Output format Bayer RAW  */
#define COM7_SET_FMT(r, x)          ((r & 0xFC) | ((x & 0x3) << 0))
#define COM7_SET_RES(r, x)          ((r & 0xBF) | (x))
#define COM7_GET_FMT(r)             (r & 0x03)

#define COM8                        0x13 /* Common Control 8              */
#define COM8_FAST_AUTO              0x80 /* Enable fast AGC/AEC algorithm */
#define COM8_STEP_VSYNC             0x00 /* AEC - Step size limited to vertical blank */
#define COM8_STEP_UNLIMIT           0x40 /* AEC - Step size unlimited step size       */
#define COM8_BANDF_EN               0x20 /* Banding filter ON/OFF */
#define COM8_AEC_BANDF              0x10 /* Enable AEC below banding value */
#define COM8_AEC_FINE_EN            0x08 /* Fine AEC ON/OFF control */
#define COM8_AGC_EN                 0x04 /* AGC Enable */
#define COM8_AWB_EN                 0x02 /* AWB Enable */
#define COM8_AEC_EN                 0x01 /* AEC Enable */
#define COM8_SET_AGC(r, x)          ((r & 0xFB) | ((x & 0x1) << 2))
#define COM8_SET_AWB(r, x)          ((r & 0xFD) | ((x & 0x1) << 1))
#define COM8_SET_AEC(r, x)          ((r & 0x7E) | ((x & 0x1) << 7) | ((x & 0x1) << 0))

#define COM9                        0x14 /* Common Control 9 */
#define COM9_HISTO_AVG              0x80 /* Histogram or average based AEC/AGC selection */
#define COM9_AGC_GAIN_2x            0x00 /* Automatic Gain Ceiling 2x  */
#define COM9_AGC_GAIN_4x            0x10 /* Automatic Gain Ceiling 4x  */
#define COM9_AGC_GAIN_8x            0x20 /* Automatic Gain Ceiling 8x  */
#define COM9_AGC_GAIN_16x           0x30 /* Automatic Gain Ceiling 16x */
#define COM9_AGC_GAIN_32x           0x40 /* Automatic Gain Ceiling 32x */
#define COM9_DROP_VSYNC             0x04 /* Drop VSYNC output of corrupt frame */
#define COM9_DROP_HREF              0x02 /* Drop HREF output of corrupt frame  */
#define COM9_SET_AGC(r, x)          ((r & 0x8F) | ((x & 0x07) << 4))

#define COM10                       0x15 /* Common Control 10 */
#define COM10_NEGATIVE              0x80 /* Output negative data */
#define COM10_HSYNC_EN              0x40 /* HREF changes to HSYNC */
#define COM10_PCLK_FREE             0x00 /* PCLK output option: free running PCLK */
#define COM10_PCLK_MASK             0x20 /* PCLK output option: masked during horizontal blank  */
#define COM10_PCLK_REV              0x10 /* PCLK reverse */
#define COM10_HREF_REV              0x08 /* HREF reverse */
#define COM10_VSYNC_FALLING         0x00 /* VSYNC changes on falling edge of PCLK */
#define COM10_VSYNC_RISING          0x04 /* VSYNC changes on rising edge of PCLK */
#define COM10_VSYNC_NEG             0x02 /* VSYNC negative */
#define COM10_OUT_RANGE_8           0x01 /* Output data range: Full range */
#define COM10_OUT_RANGE_10          0x00 /* Output data range: Data from [10] to [F0] (8 MSBs) */

#define REG16                       0x16 /* Register 16 */
#define REG16_BIT_SHIFT             0x80 /* Bit shift test pattern options */

#define HSTART                      0x17 /* Horizontal Frame (HREF column) Start 8 MSBs (2 LSBs are at HREF[5:4]) */
#define HSIZE                       0x18 /* Horizontal Sensor Size (2 LSBs are at HREF[1:0]) */
#define VSTART                      0x19 /* Vertical Frame (row) Start 8 MSBs (1 LSB is at HREF[6]) */
#define VSIZE                       0x1A /* Vertical Sensor Size (1 LSB is at HREF[2]) */
#define PSHFT                       0x1B /* Data Format - Pixel Delay Select */
#define MIDH                        0x1C /* Manufacturer ID Byte - High */
#define MIDL                        0x1D /* Manufacturer ID Byte - Low */
#define LAEC                        0x1F /* Fine AEC Value - defines exposure value less than one row period */

#define COM11                       0x20 /* Common Control 11 */
#define COM11_SNGL_FRAME_EN         0x02 /* Single frame ON/OFF selection */
#define COM11_SNGL_XFR_TRIG         0x01 /* Single frame transfer trigger */

#define BDBASE                      0x22 /* Banding Filter Minimum AEC Value */
#define BDSTEP                      0x23 /* Banding Filter Maximum Step */
#define AEW                         0x24 /* AGC/AEC - Stable Operating Region (Upper Limit) */
#define AEB                         0x25 /* AGC/AEC - Stable Operating Region (Lower Limit) */
#define VPT                         0x26 /* AGC/AEC Fast Mode Operating Region */
#define REG28                       0x28 /* Selection on the number of dummy rows, N */
#define HOUTSIZE                    0x29 /* Horizontal Data Output Size MSBs (2 LSBs at register EXHCH[1:0]) */
#define EXHCH                       0x2A /* Dummy Pixel Insert MSB */
#define EXHCL                       0x2B /* Dummy Pixel Insert LSB */
#define VOUTSIZE                    0x2C /* Vertical Data Output Size MSBs (LSB at register EXHCH[2])      */
#define ADVFL                       0x2D /* LSB of Insert Dummy Rows in Vertical Sync (1 bit equals 1 row) */
#define ADVFH                       0x2E /* MSB of Insert Dummy Rows in Vertical Sync */
#define YAVE                        0x2F /* Y/G Channel Average Value */
#define LUMHTH                      0x30 /* Histogram AEC/AGC Luminance High Level Threshold */
#define LUMLTH                      0x31 /* Histogram AEC/AGC Luminance Low Level Threshold  */
#define HREF                        0x32 /* Image Start and Size Control */
#define DM_LNL                      0x33 /* Dummy Row Low 8 Bits  */
#define DM_LNH                      0x34 /* Dummy Row High 8 Bits */
#define ADOFF_B                     0x35 /* AD Offset Compensation Value for B Channel  */
#define ADOFF_R                     0x36 /* AD Offset Compensation Value for R Channel  */
#define ADOFF_GB                    0x37 /* AD Offset Compensation Value for GB Channel */
#define ADOFF_GR                    0x38 /* AD Offset Compensation Value for GR Channel */
#define OFF_B                       0x39 /* AD Offset Compensation Value for B Channel  */
#define OFF_R                       0x3A /* AD Offset Compensation Value for R Channel  */
#define OFF_GB                      0x3B /* AD Offset Compensation Value for GB Channel */
#define OFF_GR                      0x3C /* AD Offset Compensation Value for GR Channel */

#define COM12                       0x3D /* DC offset compensation for analog process */

#define COM13                       0x3E /* Common Control 13 */
#define COM13_BLC_EN                0x80 /* BLC enable */
#define COM13_ADC_EN                0x40 /* ADC channel BLC ON/OFF control */
#define COM13_ANALOG_BLC            0x20 /* Analog processing channel BLC ON/OFF control */
#define COM13_ABLC_GAIN_EN          0x04 /* ABLC gain trigger enable */
#define COM13_SET_BLC(r, x)         ((r & 0x7F) | ((x & 0x1) << 7))

#define COM14                       0x3F /* Common Control 14 */
#define COM15                       0x40 /* Common Control 15 */
#define COM16                       0x41 /* Common Control 16 */
#define TGT_B                       0x42 /* BLC Blue Channel Target Value */
#define TGT_R                       0x43 /* BLC Red Channel Target Value  */
#define TGT_GB                      0x44 /* BLC Gb Channel Target Value   */
#define TGT_GR                      0x45 /* BLC Gr Channel Target Value   */

#define LC_CTR                      0x46 /* Lens Correction Control */
#define LC_CTR_RGB_COMP_1           0x00 /* R, G, and B channel compensation coefficient is set by LC_COEF (0x49) */
#define LC_CTR_RGB_COMP_3           0x04 /* R, G, and B channel compensation coefficient is set by registers
                                            LC_COEFB (0x4B), LC_COEF (0x49), and LC_COEFR (0x4C), respectively */
#define LC_CTR_EN                   0x01 /* Lens correction enable */
#define LC_XC                       0x47 /* X Coordinate of Lens Correction Center Relative to Array Center */
#define LC_YC                       0x48 /* Y Coordinate of Lens Correction Center Relative to Array Center */
#define LC_COEF                     0x49 /* Lens Correction Coefficient */
#define LC_RADI                     0x4A /* Lens Correction Radius */
#define LC_COEFB                    0x4B /* Lens Correction B Channel Compensation Coefficient */
#define LC_COEFR                    0x4C /* Lens Correction R Channel Compensation Coefficient */

#define FIXGAIN                     0x4D /* Analog Fix Gain Amplifier */
#define AREF0                       0x4E /* Sensor Reference Control */
#define AREF1                       0x4F /* Sensor Reference Current Control */
#define AREF2                       0x50 /* Analog Reference Control */
#define AREF3                       0x51 /* ADC Reference Control */
#define AREF4                       0x52 /* ADC Reference Control */
#define AREF5                       0x53 /* ADC Reference Control */
#define AREF6                       0x54 /* Analog Reference Control */
#define AREF7                       0x55 /* Analog Reference Control */
#define UFIX                        0x60 /* U Channel Fixed Value Output */
#define VFIX                        0x61 /* V Channel Fixed Value Output */
#define AWBB_BLK                    0x62 /* AWB Option for Advanced AWB  */

#define AWB_CTRL0                   0x63 /* AWB Control Byte 0   */
#define AWB_CTRL0_GAIN_EN           0x80 /* AWB gain enable      */
#define AWB_CTRL0_CALC_EN           0x40 /* AWB calculate enable */
#define AWB_CTRL0_WBC_MASK          0x0F /* WBC threshold 2      */

#define DSP_CTRL1                   0x64 /* DSP Control Byte 1                  */
#define DSP_CTRL1_FIFO_EN           0x80 /* FIFO enable/disable selection       */
#define DSP_CTRL1_UV_EN             0x40 /* UV adjust function ON/OFF selection */
#define DSP_CTRL1_SDE_EN            0x20 /* SDE enable                          */
#define DSP_CTRL1_MTRX_EN           0x10 /* Color matrix ON/OFF selection       */
#define DSP_CTRL1_INTRP_EN          0x08 /* Interpolation ON/OFF selection      */
#define DSP_CTRL1_GAMMA_EN          0x04 /* Gamma function ON/OFF selection     */
#define DSP_CTRL1_BLACK_EN          0x02 /* Black defect auto correction ON/OFF */
#define DSP_CTRL1_WHITE_EN          0x01 /* White defect auto correction ON/OFF */

#define DSP_CTRL2                   0x65 /* DSP Control Byte 2         */
#define DSP_CTRL2_VDCW_EN           0x08 /* Vertical DCW enable        */
#define DSP_CTRL2_HDCW_EN           0x04 /* Horizontal DCW enable      */
#define DSP_CTRL2_VZOOM_EN          0x02 /* Vertical zoom out enable   */
#define DSP_CTRL2_HZOOM_EN          0x01 /* Horizontal zoom out enable */

#define DSP_CTRL3                   0x66 /* DSP Control Byte 3                      */
#define DSP_CTRL3_UV_EN             0x80 /* UV output sequence option               */
#define DSP_CTRL3_CBAR_EN           0x20 /* DSP color bar ON/OFF selection          */
#define DSP_CTRL3_FIFO_EN           0x08 /* FIFO power down ON/OFF selection        */
#define DSP_CTRL3_SCAL1_PWDN        0x04 /* Scaling module power down control 1     */
#define DSP_CTRL3_SCAL2_PWDN        0x02 /* Scaling module power down control 2     */
#define DSP_CTRL3_INTRP_PWDN        0x01 /* Interpolation module power down control */
#define DSP_CTRL3_SET_CBAR(r, x)    ((r & 0xDF) | ((x & 1) << 5))

#define DSP_CTRL4                   0x67 /* DSP Control Byte 4          */
#define DSP_CTRL4_YUV_RGB           0x00 /* Output selection YUV or RGB */
#define DSP_CTRL4_RAW8              0x02 /* Output selection RAW8       */
#define DSP_CTRL4_RAW10             0x03 /* Output selection RAW10      */

#define AWB_BIAS                    0x68 /* AWB BLC Level Clip */
#define AWB_CTRL1                   0x69 /* AWB Control 1 */
#define AWB_CTRL2                   0x6A /* AWB Control 2 */

#define AWB_CTRL3                   0x6B /* AWB Control 3 */
#define AWB_CTRL3_ADVANCED          0x80 /* AWB mode select - Advanced AWB */
#define AWB_CTRL3_SIMPLE            0x00 /* AWB mode select - Simple AWB */

#define AWB_CTRL4                   0x6C /* AWB Control 4  */
#define AWB_CTRL5                   0x6D /* AWB Control 5  */
#define AWB_CTRL6                   0x6E /* AWB Control 6  */
#define AWB_CTRL7                   0x6F /* AWB Control 7  */
#define AWB_CTRL8                   0x70 /* AWB Control 8  */
#define AWB_CTRL9                   0x71 /* AWB Control 9  */
#define AWB_CTRL10                  0x72 /* AWB Control 10 */
#define AWB_CTRL11                  0x73 /* AWB Control 11 */
#define AWB_CTRL12                  0x74 /* AWB Control 12 */
#define AWB_CTRL13                  0x75 /* AWB Control 13 */
#define AWB_CTRL14                  0x76 /* AWB Control 14 */
#define AWB_CTRL15                  0x77 /* AWB Control 15 */
#define AWB_CTRL16                  0x78 /* AWB Control 16 */
#define AWB_CTRL17                  0x79 /* AWB Control 17 */
#define AWB_CTRL18                  0x7A /* AWB Control 18 */
#define AWB_CTRL19                  0x7B /* AWB Control 19 */
#define AWB_CTRL20                  0x7C /* AWB Control 20 */
#define AWB_CTRL21                  0x7D /* AWB Control 21 */
#define GAM1                        0x7E /* Gamma Curve 1st Segment Input End Point 0x04 Output Value */
#define GAM2                        0x7F /* Gamma Curve 2nd Segment Input End Point 0x08 Output Value */
#define GAM3                        0x80 /* Gamma Curve 3rd Segment Input End Point 0x10 Output Value */
#define GAM4                        0x81 /* Gamma Curve 4th Segment Input End Point 0x20 Output Value */
#define GAM5                        0x82 /* Gamma Curve 5th Segment Input End Point 0x28 Output Value */
#define GAM6                        0x83 /* Gamma Curve 6th Segment Input End Point 0x30 Output Value */
#define GAM7                        0x84 /* Gamma Curve 7th Segment Input End Point 0x38 Output Value */
#define GAM8                        0x85 /* Gamma Curve 8th Segment Input End Point 0x40 Output Value */
#define GAM9                        0x86 /* Gamma Curve 9th Segment Input Enpd Point 0x48 Output Value */
#define GAM10                       0x87 /* Gamma Curve 10th Segment Input End Point 0x50 Output Value */
#define GAM11                       0x88 /* Gamma Curve 11th Segment Input End Point 0x60 Output Value */
#define GAM12                       0x89 /* Gamma Curve 12th Segment Input End Point 0x70 Output Value */
#define GAM13                       0x8A /* Gamma Curve 13th Segment Input End Point 0x90 Output Value */
#define GAM14                       0x8B /* Gamma Curve 14th Segment Input End Point 0xB0 Output Value */
#define GAM15                       0x8C /* Gamma Curve 15th Segment Input End Point 0xD0 Output Value */
#define SLOP                        0x8D /* Gamma Curve Highest Segment Slope */
#define DNSTH                       0x8E /* De-noise Threshold */
#define EDGE0                       0x8F /* Edge Enhancement Strength Control */
#define EDGE1                       0x90 /* Edge Enhancement Threshold Control */
#define DNSOFF                      0x91 /* Auto De-noise Threshold Control */
#define EDGE2                       0x92 /* Edge Enhancement Strength Upper Limit */
#define EDGE3                       0x93 /* Edge Enhancement Strength Upper Limit */
#define MTX1                        0x94 /* Matrix Coefficient 1 */
#define MTX2                        0x95 /* Matrix Coefficient 2 */
#define MTX3                        0x96 /* Matrix Coefficient 3 */
#define MTX4                        0x97 /* Matrix Coefficient 4 */
#define MTX5                        0x98 /* Matrix Coefficient 5 */
#define MTX6                        0x99 /* Matrix Coefficient 6 */

#define MTX_CTRL                    0x9A /* Matrix Control */
#define MTX_CTRL_DBL_EN             0x80 /* Matrix double ON/OFF selection */

#define BRIGHTNESS                  0x9B /* Brightness Control */
#define CONTRAST                    0x9C /* Contrast Gain */
#define UVADJ0                      0x9E /* Auto UV Adjust Control 0 */
#define UVADJ1                      0x9F /* Auto UV Adjust Control 1 */
#define SCAL0                       0xA0 /* DCW Ratio Control */
#define SCAL1                       0xA1 /* Horizontal Zoom Out Control */
#define SCAL2                       0xA2 /* Vertical Zoom Out Control */
#define FIFODLYM                    0xA3 /* FIFO Manual Mode Delay Control */
#define FIFODLYA                    0xA4 /* FIFO Auto Mode Delay Control */

#define SDE                         0xA6 /* Special Digital Effect Control */
#define SDE_NEGATIVE_EN             0x40 /* Negative image enable          */
#define SDE_GRAYSCALE_EN            0x20 /* Gray scale image enable        */
#define SDE_V_FIXED_EN              0x10 /* V fixed value enable           */
#define SDE_U_FIXED_EN              0x08 /* U fixed value enable           */
#define SDE_CONT_BRIGHT_EN          0x04 /* Contrast/Brightness enable     */
#define SDE_SATURATION_EN           0x02 /* Saturation enable              */
#define SDE_HUE_EN                  0x01 /* Hue enable                     */

#define USAT                        0xA7 /* U Component Saturation Gain     */
#define VSAT                        0xA8 /* V Component Saturation Gain     */
#define HUECOS                      0xA9 /* Cosine value × 0x80             */
#define HUESIN                      0xAA /* Sine value × 0x80               */
#define SIGN_BIT                    0xAB /* Sign Bit for Hue and Brightness */

#define DSPAUTO                     0xAC /* DSP Auto Function ON/OFF Control */
#define DSPAUTO_AWB_EN              0x80 /* AWB auto threshold control */
#define DSPAUTO_DENOISE_EN          0x40 /* De-noise auto threshold control */
#define DSPAUTO_EDGE_EN             0x20 /* Sharpness (edge enhancement) auto strength control */
#define DSPAUTO_UV_EN               0x10 /* UV adjust auto slope control */
#define DSPAUTO_SCAL0_EN            0x08 /* Auto scaling factor control (register SCAL0 (0xA0)) */
#define DSPAUTO_SCAL1_EN            0x04 /* Auto scaling factor control (registers SCAL1 (0xA1 and SCAL2 (0xA2)) */

/*================================== TYPES ========================================================*/

struct OV7725Reg {
	uint8_t reg;
	uint8_t val;
};

/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static const struct OV7725Reg sc_init_table[] = {
	/* 输出窗口设置 */
	{CLKRC,     0x00}, /* [5:0]Internal clock pre-scale: 0, PCLK = 12M * 4(PLL) / (0 + 1) / 2 = 24MHz */
	{COM7,      0x06}, /* [6]Resolution: VGA; [3:2]RGB Format: RGB565; Output format: RGB */
	{HSTART,    0x23}, /* 水平起始位置。手册 VGA 默认 */
	{HSIZE,     0xA0}, /* 水平尺寸。手册 VGA 默认 */
	{VSTART,    0x07}, /* 垂直起始位置。手册 VGA 默认 */
	{VSIZE,     0xF0}, /* 垂直尺寸。手册 VGA 默认 */
	{HREF,      0x00},
	{HOUTSIZE,  0xA0}, /* 输出尺寸。手册 VGA 默认 */
	{VOUTSIZE,  0xF0}, /* 输出尺寸。手册 VGA 默认 */
	{COM10,     0x02},
	/* DSP control */
	{TGT_B,     0x7F},
	{FIXGAIN,   0x09},
	{AWB_CTRL0, 0xE0},
	{DSP_CTRL1, 0xFF},
	{DSP_CTRL2, 0x00},
	{DSP_CTRL3, 0x00},
	{DSP_CTRL4, 0x00},
	/* AGC AEC AWB */
	{COM8,      0xF0},
	{COM4,      0x41}, /* [7:6]PLL: 4X; [5:4]AEC: Full window */
	{COM6,      0xC5},
	{COM9,      0x11},
	{BDBASE,    0x7F},
	{BDSTEP,    0x03},
	{AEW,       0x40},
	{AEB,       0x30},
	{VPT,       0xA1},
	{EXHCL,     0x9E},
	{AWB_CTRL3, 0xAA},
	{COM8,      0xFF},
	/* matrix shapness brightness contrast */
	{EDGE1,     0x08},
	{DNSOFF,    0x01},
	{EDGE2,     0x03},
	{EDGE3,     0x00},
	{MTX1,      0xB0},
	{MTX2,      0x9D},
	{MTX3,      0x13},
	{MTX4,      0x16},
	{MTX5,      0x7B},
	{MTX6,      0x91},
	{MTX_CTRL,  0x1E},
	{BRIGHTNESS,0x08},
	{CONTRAST,  0x20},
	{UVADJ0,    0x81},
	{SDE,       0X06},
	{USAT,      0x65},
	{VSAT,      0x65},
	{HUECOS,    0X80},
	{HUESIN,    0X80},
	/* GAMMA config */
	{GAM1,      0x0C},
	{GAM2,      0x16},
	{GAM3,      0x2A},
	{GAM4,      0x4E},
	{GAM5,      0x61},
	{GAM6,      0x6F},
	{GAM7,      0x7B},
	{GAM8,      0x86},
	{GAM9,      0x8E},
	{GAM10,     0x97},
	{GAM11,     0xA4},
	{GAM12,     0xAF},
	{GAM13,     0xC5},
	{GAM14,     0xD7},
	{GAM15,     0xE8},
	{SLOP,      0x20},
	/* Horizontal mirror image
	 * 注：datasheet默认0x10,即改变YUV为UVY格式。但是摄像头不是芯片而是模组时，要将0X10中的1变成0，即设置YUV格式。 */
	{COM3,      0x80},
	{COM5,      0x65},
};

static bool s_capture_start;
static uint8_t s_current_fps;
static uint16_t s_current_length;
static uint16_t s_current_width;
static ceu_capture_format_t s_capture_format;
static CameraDVPErrorCountType s_error_count;
static CameraResolutionEnum s_resolution;
static CameraPixelFormatEnum s_pixel_format;
static CameraRotationBufferType *s_rotation_buf;
static volatile bool s_capture_ready;
static volatile uint32_t s_capture_cnt;

/* FSP runtime config */
static ceu_instance_ctrl_t *s_ceu_inst_ctrl = NULL;
static capture_cfg_t s_capture_cfg;
static ceu_extended_cfg_t s_extend_cfg;

/*================================== Private Functions Prototypes =================================*/

static void ceuCallback(capture_callback_args_t *p_args);
static void hardReset(void);
static void pinConfig(void);
static uint32_t setOutputWindow(CameraResolutionEnum res, uint16_t length, uint16_t width);
static uint32_t softReset(void);

/*================================== Public Functions =============================================*/

uint32_t OV7725_GetCaptureCnt(CameraInterfaceEnum interface, uint32_t *cnt)
{
	IS_INVALID_POINT(cnt);

	(void)interface;

	*cnt = s_capture_cnt;

	return 0;
}

uint32_t OV7725_GetErrorCnt(CameraDVPErrorCountType *err_cnt)
{
	memcpy(err_cnt, &s_error_count, sizeof(s_error_count));

	return 0;
}

uint32_t OV7725_GetID(uint16_t *id)
{
	uint8_t val;
	fsp_err_t err;

	IIC_READ_CHECK(MIDH, &val);
	*id = (uint16_t)(((uint16_t)val) << 8);
	IIC_READ_CHECK(MIDL, &val);
	*id |= val;

	return err;
}

uint32_t OV7725_GetResolution(CameraInterfaceEnum interface, uint16_t *length, uint16_t *width, CameraResolutionEnum *res)
{
	(void)interface;

	if (length != NULL) {
		*length = s_current_length;
	}
	if (width != NULL) {
		*width = s_current_width;
	}
	if (res != NULL) {
		*res = s_resolution;
	}

	return 0;
}

uint32_t OV7725_Init(CameraInterfaceEnum interface, CameraRotationBufferType *rb)
{
	uint8_t val;
	uint16_t i;
	uint16_t init_len;
	uint32_t err;

	(void)val;
	(void)interface;

	pinConfig();
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
	hardReset();
	R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MILLISECONDS);
	err = softReset();
	UNLIKE_RETURN(err, 0, "softReset() failed: %" PRIu32, err);

	err = OV7725_GetID(&i);
	UNLIKE_RETURN(err, 0, "OV7725_GetID() failed: %" PRIu32, err);
	OV7725_LOGD("ID: 0x%04" PRIX32, i);

	init_len = (uint16_t)(sizeof(sc_init_table) / sizeof(sc_init_table[0]));
	OV7725_LOGD("Init table length: %" PRIu32, init_len);
	for (i = 0; i < init_len; i++) {
    	IIC_WRITE_CHECK(sc_init_table[i].reg, sc_init_table[i].val);
	#if __OV7725_DEBUG
    	IIC_READ_CHECK(sc_init_table[i].reg, &val);
    	if (val != sc_init_table[i].val) {
    		OV7725_LOGW("REG 0x%" PRIX32 ": W[0x%" PRIX32 "] != R[0x%" PRIX32 "]", sc_init_table[i].reg, sc_init_table[i].val, val);
    	}
	#endif
    }

	s_capture_start = false;
	s_current_fps = 25;
	s_current_length = 640;
	s_current_width = 480;
	s_resolution = CAMERA_RESOLUTION_VGA;
	s_pixel_format = CAMARA_PIXEL_RGB565;
	s_rotation_buf = rb;
	memset(&s_error_count, 0, sizeof(s_error_count));

	return err;
}

uint32_t OV7725_IsCaptureStart(CameraInterfaceEnum interface, bool *result)
{
	IS_INVALID_POINT(result);

	(void)interface;

	*result = s_capture_start;

	return 0;
}

uint32_t OV7725_IsImageReady(CameraInterfaceEnum interface, uint8_t **p_image)
{
	(void)interface;
	(void)p_image;

	if (s_capture_ready) {
		s_capture_ready = false;
		if (p_image != NULL) {
			if (s_rotation_buf->index == 0) {
				if (s_rotation_buf->length == 1) {
					*p_image = s_rotation_buf->buffer[0];
				}
				else {
					*p_image = s_rotation_buf->buffer[s_rotation_buf->length - 1];
				}
			}
			else {
				*p_image = s_rotation_buf->buffer[s_rotation_buf->index - 1];
			}
		}
		return 1;
	}
	else {
		return 0;
	}
}

uint32_t OV7725_IsPowerDown(CameraInterfaceEnum interface)
{
	(void)interface;

	if (R_BSP_PinRead(OV7725_PIN_PWR_ON) == BSP_IO_LEVEL_HIGH) {
		return 1;
	}
	else {
		return 0;
	}
}

uint32_t OV7725_PowerDown(CameraInterfaceEnum interface, uint8_t enable)
{
	(void)interface;

	R_BSP_PinAccessEnable();
	if (enable) {
		R_BSP_PinWrite(OV7725_PIN_PWR_ON, BSP_IO_LEVEL_HIGH);
	}
	else {
		R_BSP_PinWrite(OV7725_PIN_PWR_ON, BSP_IO_LEVEL_LOW);
	}
	R_BSP_PinAccessDisable();

	return 0;
}

/**
 * @brief	设置 VGA/QVGA 基准帧率，当前时钟不适用时自动调整 PLL 和分频
 * @param 	target       目标整数帧率，必须非零；VGA 最高 60 FPS
 * @param 	real         回传计算得到的基准帧率（四舍五入为整数）；NULL 表示不回传
 * @param 	auto_decrese 是否允许低照度自动降帧，开启时最多降至基准帧率的 1/8
 * @retval 	FSP_SUCCESS 设置成功
 * @note 	优先沿用当前时钟，否则选择能达到目标的最低 PCLK（不超过 48 MHz）。
 * 			在设置帧率前最好先关闭 CEU，因为可能会修改时钟频率。
 */
uint32_t OV7725_SetFPS(CameraInterfaceEnum interface, uint16_t target, uint16_t *real, bool auto_decrese)
{
	fsp_err_t err;
	uint8_t com4, clkrc, com5, exhch;
	uint32_t pll_mul, clock_div, clocks_per_pixel;
	uint32_t line_pixels, base_lines, frame_lines, dummy_lines;
	uint64_t clock_num, line_den, target_den, frame_den;
	uint64_t error_short, error_long;
	uint8_t actual_fps;
	uint8_t new_com4, new_clkrc;
	bool change_clock = false;

	(void)interface;

	if (target == 0U) {
		return FSP_ERR_INVALID_ARGUMENT;
	}

	/* Datasheet Figure 6/7：包括基础消隐的完整行长和帧高。 */
	switch (s_resolution) {
	case CAMERA_RESOLUTION_VGA:
		if (target > 60U) {
			return FSP_ERR_INVALID_RATE;
		}
		line_pixels = 784U;
		base_lines = 510U;
		break;
	case CAMERA_RESOLUTION_QVGA:
		line_pixels = 576U;
		base_lines = 278U;
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	switch (s_pixel_format) {
	case CAMERA_PIXEL_GRB422:
	case CAMARA_PIXEL_RGB565:
	case CAMERA_PIXEL_YUV422:
		clocks_per_pixel = 2U;
		break;
	default:
		return FSP_ERR_UNSUPPORTED;
	}

	IIC_READ_CHECK(COM4, &com4);
	IIC_READ_CHECK(CLKRC, &clkrc);
	switch (COM4_GET_PLL(com4)) {
	case COM4_PLL_4x:
		pll_mul = 4U;
		break;
	case COM4_PLL_6x:
		pll_mul = 6U;
		break;
	case COM4_PLL_8x:
		pll_mul = 8U;
		break;
	default: /* COM4_PLL_BYPASS */
		pll_mul = 1U;
		break;
	}
	clock_div = (clkrc & CLKRC_NO_PRESCALE) ? 1U : (((uint32_t)clkrc & CLKRC_PRESCALER) + 1U) * 2U;

	/* 保留时钟分数，避免先计算整数 PCLK 导致精度损失。
	 * FPS = XCLK * PLL / (clock_div * clocks_per_pixel * line_pixels * frame_lines)。
	 * 额外水平消隐统一清零，仅使用 DM_LNH/L 的 16 位空行计数调节帧率。
	 */
	clock_num = (uint64_t)XCLK_FREQ * pll_mul;
	line_den = (uint64_t)clock_div * clocks_per_pixel * line_pixels;
	target_den = line_den * target;
	new_com4 = com4;
	new_clkrc = clkrc;
	if ((clock_num < target_den * base_lines) ||
		(clock_num > target_den * (base_lines + UINT16_MAX)) ||
		(clock_num > (uint64_t)MAX_PCLK_FREQ * clock_div)) {
		uint32_t best_div = 0U;
		uint64_t best_num = 0U;
		uint32_t pll_index, div_index;

		/* PLL 支持 1/4/6/8 倍；分频支持直通及 2、4、...、128。
		 * 同频时优先保留原分频，例如 12 MHz XCLK 的 VGA 60 FPS
		 * 从 PLL x4、/2 切换为 PLL x8、/2，而不是切换至直通。
		 */
		for (pll_index = 0U; pll_index < 4U; pll_index++) {
			uint32_t candidate_mul = (pll_index == 0U) ? 1U : (pll_index + 1U) * 2U;
			uint64_t candidate_num = (uint64_t)XCLK_FREQ * candidate_mul;
			for (div_index = 0U; div_index <= 64U; div_index++) {
				uint32_t candidate_div = (div_index == 0U) ? 1U : div_index * 2U;
				uint64_t candidate_den = (uint64_t)candidate_div * clocks_per_pixel * line_pixels * target;
				if ((candidate_num > (uint64_t)MAX_PCLK_FREQ * candidate_div) ||
					(candidate_num < candidate_den * base_lines) ||
					(candidate_num > candidate_den * (base_lines + UINT16_MAX))) {
					continue;
				}
				if ((best_div == 0U) || (candidate_num * best_div < best_num * candidate_div) ||
					((candidate_num * best_div == best_num * candidate_div) &&
					 (candidate_div == clock_div) && (best_div != clock_div))) {
					best_num = candidate_num;
					best_div = candidate_div;
					new_com4 = (uint8_t)((com4 & 0x3FU) | (pll_index << 6));
					new_clkrc = (uint8_t)((clkrc & 0x80U) |
						((div_index == 0U) ? CLKRC_NO_PRESCALE : (div_index - 1U)));
				}
			}
		}
		if (best_div == 0U) {
			return FSP_ERR_INVALID_RATE;
		}
		clock_num = best_num;
		clock_div = best_div;
		line_den = (uint64_t)clock_div * clocks_per_pixel * line_pixels;
		target_den = line_den * target;
		change_clock = true;
	}

	frame_lines = (uint32_t)(clock_num / target_den);
	if (frame_lines < (base_lines + UINT16_MAX)) {
		/* 比较相邻两个整数帧高对应的帧率误差，取更接近目标的一项。 */
		error_short = clock_num - target_den * frame_lines;
		error_long = target_den * (frame_lines + 1U) - clock_num;
		if (error_long * frame_lines < error_short * (frame_lines + 1U)) {
			frame_lines++;
		}
	}
	dummy_lines = frame_lines - base_lines;
	frame_den = line_den * frame_lines;
	actual_fps = (uint8_t)((clock_num + frame_den / 2U) / frame_den);

	IIC_READ_CHECK(EXHCH, &exhch);
	IIC_READ_CHECK(COM5, &com5);

	/* 先关闭自动插行，再清除旧消隐；EXHCH 低半字节包含输出尺寸，必须保留。 */
	s_current_fps = 0;
	com5 = (uint8_t)(com5 & (uint8_t)~COM5_AFR);
	IIC_WRITE_CHECK(COM5, com5);
	if (change_clock) {
		/* 先切至最大分频，再修改 PLL，最后应用目标分频，避免中途 PCLK 超限。 */
		IIC_WRITE_CHECK(CLKRC, (uint8_t)((clkrc & 0x80U) | CLKRC_PRESCALER));
		IIC_WRITE_CHECK(COM4, new_com4);
		IIC_WRITE_CHECK(CLKRC, new_clkrc);
	}
	IIC_WRITE_CHECK(EXHCH, (uint8_t)(exhch & 0x0FU));
	IIC_WRITE_CHECK(EXHCL, 0x00);
	IIC_WRITE_CHECK(ADVFL, 0x00);
	IIC_WRITE_CHECK(ADVFH, 0x00);
	IIC_WRITE_CHECK(DM_LNH, (uint8_t)(dummy_lines >> 8));
	IIC_WRITE_CHECK(DM_LNL, (uint8_t)(dummy_lines & 0xFFU));
	if (auto_decrese) {
		/* 保留速度、保留位和 AEC 步进设置；4x 增益触发，最多降到 1/8。 */
		com5 = (uint8_t)((com5 & 0x43U) | COM5_AFR | COM5_AFR_1_8 | COM5_AFR_4x);
		IIC_WRITE_CHECK(COM5, com5);
	}

	s_current_fps = (uint8_t)target;
	if (real != NULL) {
		*real = actual_fps;
	}

	return FSP_SUCCESS;
}

/**
 * @brief 设置预设尺寸，或从能够容纳目标窗口的最小基础模式居中裁剪。
 * @param length 水平像素数，范围 128~640，必须为 8 的倍数。
 * @param width 垂直行数，范围 96~480，必须为 4 的倍数。
 * @note QVGA 为 320x240；320x480 使用 VGA 裁剪。修改前必须停止采集，
 *       下次 StartCapture() 应传入能够容纳新尺寸的缓冲区。
 */
uint32_t OV7725_SetOutputSize(CameraInterfaceEnum interface, uint16_t length, uint16_t width)
{
	if ((length < 128U) || (length > 640U) || (width < 96U) || (width > 480U) ||
		((length & 7U) != 0U) || ((width & 3U) != 0U)) {
		return FSP_ERR_INVALID_SIZE;
	}
	if ((length == 640U) && (width == 480U)) {
		return OV7725_SetOutputSizePreset(CAMERA_RESOLUTION_VGA);
	}
	if ((length == 320U) && (width == 240U)) {
		return OV7725_SetOutputSizePreset(CAMERA_RESOLUTION_QVGA);
	}

	(void)interface;

	CameraResolutionEnum res = ((length <= 320U) && (width <= 240U)) ?
		CAMERA_RESOLUTION_QVGA : CAMERA_RESOLUTION_VGA;
	return setOutputWindow(res, length, width);
}

/** @brief 设置 VGA 640x480 或 QVGA 320x240 预设，保留像素格式及图像调节参数。 */
uint32_t OV7725_SetOutputSizePreset(CameraResolutionEnum res)
{
	switch (res) {
	case CAMERA_RESOLUTION_VGA:
		return setOutputWindow(res, 640U, 480U);
	case CAMERA_RESOLUTION_QVGA:
		return setOutputWindow(res, 320U, 240U);
	default:
		return FSP_ERR_UNSUPPORTED;
	}
}

uint32_t OV7725_StartCapture(const void *peri, CameraInterfaceEnum interface, uint8_t *image_buffer)
{
	uint32_t err;

	IS_INVALID_POINT(image_buffer);
	IS_INVALID_POINT(peri);
	if ((s_current_length == 0U) || (s_current_width == 0U)) {
		return FSP_ERR_NOT_INITIALIZED;
	}

	ceu_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
	const capture_cfg_t *p_cfg = ((capture_instance_t *)peri)->p_cfg;
	const ceu_extended_cfg_t *p_extend = (const ceu_extended_cfg_t *)p_cfg->p_extend;

	(void)interface;

	if (s_ceu_inst_ctrl != p_ctrl) {
		if ((s_ceu_inst_ctrl != NULL) && s_ceu_inst_ctrl->open) {
			return FSP_ERR_IN_USE;
		}
		memcpy(&s_extend_cfg, p_extend, sizeof(ceu_extended_cfg_t));
		memcpy(&s_capture_cfg, p_cfg, sizeof(capture_cfg_t));
		s_ceu_inst_ctrl = p_ctrl;
	}
	s_capture_start = false;
	s_capture_ready = false;
	if (s_ceu_inst_ctrl->open) {
		err = R_CEU_Close(s_ceu_inst_ctrl);
		UNLIKE_RETURN(err, 0, "CEU_Close failed: %" PRIu32, err);
		R_BSP_SoftwareDelay(20, BSP_DELAY_UNITS_MILLISECONDS);
	}
	/* Init 后首次设置尺寸时 CEU 尚未绑定，在此将实际尺寸应用到运行时副本。 */
	s_capture_cfg.x_capture_pixels = s_current_length;
	s_capture_cfg.y_capture_pixels = s_current_width;
	s_capture_cfg.x_capture_start_pixel = 0;
	s_capture_cfg.y_capture_start_pixel = 0;
	s_capture_cfg.bytes_per_pixel = 2;
	s_capture_cfg.p_callback = ceuCallback;
	s_capture_cfg.p_context = p_ctrl;
	s_capture_cfg.p_extend = &s_extend_cfg;
	s_extend_cfg.image_area_size = (uint32_t)s_current_length * s_current_width * 2U;
	err = R_CEU_Open(s_ceu_inst_ctrl, &s_capture_cfg);
	UNLIKE_RETURN(err, 0, "CEU_Open failed: %" PRIu32, err);
	/* CAMCR/CMCYR 改写后至少等待 10 个 PCLK；1 ms 覆盖允许的最低分频时钟。 */
	R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
	s_capture_format = s_extend_cfg.capture_format;
	s_capture_start = true;

	/* 兼容旧的代码 */
	s_rotation_buf->index = 0;
	if (image_buffer != NULL) {
		s_rotation_buf->buffer[0] = image_buffer;
		s_rotation_buf->buffer[1] = NULL;
		s_rotation_buf->buffer[2] = NULL;
		s_rotation_buf->index = 0;
		s_rotation_buf->length = 1;
		err = R_CEU_CaptureStart(s_ceu_inst_ctrl, image_buffer);
	}
	else {
		err = R_CEU_CaptureStart(s_ceu_inst_ctrl, s_rotation_buf->buffer[0]);
	}

	if (err != 0) {
		OV7725_LOGE("CaptureStart() failed: %" PRIu32, err);
		s_capture_start = false;
	}

	return err;
}

uint32_t OV7725_StopCapture(const void *peri, CameraInterfaceEnum interface)
{
	fsp_err_t err;

	(void)interface;

	ceu_instance_ctrl_t *p_ctrl = ((capture_instance_t *)peri)->p_ctrl;
	err = R_CEU_Close(p_ctrl);
	if (err == 0) {
		s_capture_start = false;
	}

	return err;
}

static void ceuCallback(capture_callback_args_t *p_args)
{
	uint32_t err;

	bool has_err = false;

	if (p_args->event == CEU_EVENT_NONE) {}
	if (p_args->event & CEU_EVENT_VD) {}
	if (p_args->event & CEU_EVENT_CRAM_OVERFLOW) {
		s_error_count.overflow++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_HD_MISMATCH) {
		s_error_count.hd_mismatch++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_MISMATCH) {
		s_error_count.vd_mismatch++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_ERROR) {
		s_error_count.vd_error++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_FIREWALL) {
		s_error_count.firewall++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_HD_MISSING) {
		s_error_count.hd_missing++;
		has_err = true;
	}
	if (p_args->event & CEU_EVENT_VD_MISSING) {
		s_error_count.vd_missing++;
		has_err = true;
	}

	if ((p_args->event & CEU_EVENT_FRAME_END) &&  (has_err == false)) {
		s_capture_cnt++;
		s_capture_ready = true;
		if (s_capture_format == CEU_CAPTURE_FORMAT_DATA_SYNCHRONOUS) {
			/* FSP 的这个模式只采集一帧就停了，需要再次发起采集 */
			if (s_capture_start) {
				s_rotation_buf->index++;
				if (s_rotation_buf->index >= s_rotation_buf->length) {
					s_rotation_buf->index = 0;
				}
				if (s_rotation_buf->buffer[s_rotation_buf->index] == NULL) {
					OV7725_LOGE("Buffer rotation error");
					s_capture_ready = false;
					s_capture_start = false;
				}
				err = R_CEU_CaptureStart(p_args->p_context, s_rotation_buf->buffer[s_rotation_buf->index]);
				if (err) {
					OV7725_LOGE("CaptureStart failed: %" PRIu32, err);
					s_capture_ready = false;
					s_capture_start = false;
				}
			}
		}
	}
}

/*================================== Private Functions ============================================*/

/**
 * @brief 应用基础模式和居中窗口，只修改分辨率相关寄存器。
 * @note s_resolution 表示 VGA/QVGA 基础时序模式，实际输出尺寸单独记录。
 *       基础模式变化时重新应用原目标帧率和自动降帧策略；不改变图像调节设置。
 *       写入失败后输出尺寸标记为无效，禁止使用旧尺寸开始采集。
 */
static uint32_t setOutputWindow(CameraResolutionEnum res, uint16_t length, uint16_t width)
{
	fsp_err_t err;
	uint8_t com7, href, exhch, dsp_ctrl2, com5;
	uint8_t target_fps = s_current_fps;
	uint16_t base_length, base_width, hstart, vstart;
	bool mode_changed = (res != s_resolution);

	if ((s_resolution != CAMERA_RESOLUTION_VGA) && (s_resolution != CAMERA_RESOLUTION_QVGA)) {
		return FSP_ERR_NOT_INITIALIZED;
	}
	if (s_capture_start) {
		return FSP_ERR_IN_USE;
	}
	if (!mode_changed && (s_current_length == length) && (s_current_width == width)) {
		return FSP_SUCCESS;
	}

	/* Datasheet Table 6：VGA 起点 0x23/0x07，QVGA 起点 0x3F/0x03。 */
	if (res == CAMERA_RESOLUTION_QVGA) {
		base_length = 320U;
		base_width = 240U;
		hstart = 0x3FU << 2;
		vstart = 0x03U << 1;
	}
	else {
		base_length = 640U;
		base_width = 480U;
		hstart = 0x23U << 2;
		vstart = 0x07U << 1;
	}
	hstart = (uint16_t)(hstart + (base_length - length) / 2U);
	vstart = (uint16_t)(vstart + (base_width - width) / 2U);

	IIC_READ_CHECK(COM7, &com7);
	IIC_READ_CHECK(HREF, &href);
	IIC_READ_CHECK(EXHCH, &exhch);
	IIC_READ_CHECK(DSP_CTRL2, &dsp_ctrl2);
	IIC_READ_CHECK(COM5, &com5);

	/* CEU 配置对象被驱动引用，必须先关闭，再修改运行时副本。 */
	if ((s_ceu_inst_ctrl != NULL) && s_ceu_inst_ctrl->open) {
		err = R_CEU_Close(s_ceu_inst_ctrl);
		UNLIKE_RETURN(err, 0, "CEU_Close failed: %" PRIu32, err);
	}
	s_capture_ready = false;
	s_current_length = 0;
	s_current_width = 0;
	s_current_fps = 0;

	com7 = (uint8_t)COM7_SET_RES(com7, (res == CAMERA_RESOLUTION_QVGA) ? COM7_RES_QVGA : COM7_RES_VGA);
	IIC_WRITE_CHECK(COM7, com7);
	/* 裁剪使用等大的输入窗口和输出窗口，关闭 DSP 的抽样及缩放，保留保留位。 */
	IIC_WRITE_CHECK(DSP_CTRL2, (uint8_t)(dsp_ctrl2 & 0xF0U));
	IIC_WRITE_CHECK(HSTART, (uint8_t)(hstart >> 2));
	IIC_WRITE_CHECK(HSIZE, (uint8_t)(length >> 2));
	IIC_WRITE_CHECK(VSTART, (uint8_t)(vstart >> 1));
	IIC_WRITE_CHECK(VSIZE, (uint8_t)(width >> 1));
	href = (uint8_t)((href & 0x88U) | ((hstart & 3U) << 4) | ((vstart & 1U) << 6) |
		(length & 3U) | ((width & 1U) << 2));
	IIC_WRITE_CHECK(HREF, href);
	IIC_WRITE_CHECK(HOUTSIZE, (uint8_t)(length >> 2));
	IIC_WRITE_CHECK(VOUTSIZE, (uint8_t)(width >> 1));
	exhch = (uint8_t)((exhch & 0xF8U) | (length & 3U) | ((width & 1U) << 2));
	IIC_WRITE_CHECK(EXHCH, exhch);

	s_resolution = res;
	if (mode_changed && (target_fps != 0U)) {
		err = OV7725_SetFPS(CAMERA_INTERFACE_DVP, target_fps, NULL, (com5 & COM5_AFR) != 0U);
		UNLIKE_RETURN(err, 0, "SetFPS failed: %" PRIu32, err);
	}
	else {
		s_current_fps = target_fps;
	}
	s_current_length = length;
	s_current_width = width;
	s_capture_cfg.x_capture_pixels = length;
	s_capture_cfg.y_capture_pixels = width;
	s_capture_cfg.x_capture_start_pixel = 0;
	s_capture_cfg.y_capture_start_pixel = 0;
	s_capture_cfg.bytes_per_pixel = 2;
	s_extend_cfg.image_area_size = (uint32_t)length * width * 2U;

	return FSP_SUCCESS;
}

static void hardReset(void)
{
	R_BSP_PinAccessEnable();
	R_BSP_PinWrite(OV7725_PIN_RESET, BSP_IO_LEVEL_LOW);
	R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
	R_BSP_PinWrite(OV7725_PIN_RESET, BSP_IO_LEVEL_HIGH);
	R_BSP_PinAccessDisable();
}

static void pinConfig(void)
{
	ioport_instance_ctrl_t pin_ctrl;
	ioport_pin_cfg_t pin_cfg_data[13];
    ioport_cfg_t pin_cfg;

    pin_cfg_data[0].pin = BSP_IO_PORT_04_PIN_14;
    pin_cfg_data[0].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[1].pin = BSP_IO_PORT_04_PIN_00;
    pin_cfg_data[1].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[2].pin = BSP_IO_PORT_04_PIN_01;
    pin_cfg_data[2].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[3].pin = BSP_IO_PORT_04_PIN_05;
    pin_cfg_data[3].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[4].pin = BSP_IO_PORT_04_PIN_06;
    pin_cfg_data[4].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[5].pin = BSP_IO_PORT_07_PIN_00;
    pin_cfg_data[5].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[6].pin = BSP_IO_PORT_07_PIN_01;
    pin_cfg_data[6].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[7].pin = BSP_IO_PORT_07_PIN_02;
    pin_cfg_data[7].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[8].pin = BSP_IO_PORT_07_PIN_03;
    pin_cfg_data[8].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[9].pin = BSP_IO_PORT_04_PIN_15;
    pin_cfg_data[9].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;
    pin_cfg_data[10].pin = BSP_IO_PORT_07_PIN_08;
    pin_cfg_data[10].pin_cfg = (uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_CEU;

    pin_cfg_data[11].pin = OV7725_PIN_PWR_ON;
    pin_cfg_data[11].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW;
    pin_cfg_data[12].pin = OV7725_PIN_RESET;
    pin_cfg_data[12].pin_cfg = (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_HIGH;

    pin_cfg.number_of_pins = 13;
    pin_cfg.p_pin_cfg_data = (ioport_pin_cfg_t const *)&pin_cfg_data;
    pin_cfg.p_extend = NULL;
    R_IOPORT_Open(&pin_ctrl, &pin_cfg);
}

static uint32_t softReset(void)
{
	fsp_err_t err;

	IIC_WRITE_CHECK(COM7, COM7_RESET);
	R_BSP_SoftwareDelay(300, BSP_DELAY_UNITS_MILLISECONDS);

	return err;
}

#endif /* #if CAMERA_EN_DVP */
