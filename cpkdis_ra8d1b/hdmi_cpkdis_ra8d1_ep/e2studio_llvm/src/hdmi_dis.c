#include "hdmi_dis.h"
#include "common_utils.h"
#include "hal_data.h"
//#include "sii902_linux.h"



void hdmi_power_on()
{
    // HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_RESET);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_LOW);
    // HAL_Delay(1000);
    R_BSP_SoftwareDelay (1000, BSP_DELAY_UNITS_MILLISECONDS);

    // HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_SET);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_HIGH);
    // HAL_Delay(1000);
    R_BSP_SoftwareDelay (1000, BSP_DELAY_UNITS_MILLISECONDS);


}


/*******************************************************************************
** 函数名称: hdmi_power_off
** 功能描述: 
** 参数说明: None
** 返回说明: None
** 创建人员: Main
** 创建日期: 2021-03-30
**------------------------------------------------------------------------------
** 修改人员:
** 修改日期:
** 修改描述:
**------------------------------------------------------------------------------
********************************************************************************/
void hdmi_power_off(void)
{
    // HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, GPIO_PIN_RESET);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_00, BSP_IO_LEVEL_LOW);

//    HAL_Delay(100);
    R_BSP_SoftwareDelay (100, BSP_DELAY_UNITS_MILLISECONDS);


}

extern void sil902x_test(void);
void hdmi_init(void)
{
	hdmi_power_on();
	
	// #ifdef __LINUX__
		sil902x_device_init();
	// #else
	// 	siHdmiTx_VideoSel(HDMI_720P60);
	// 	siHdmiTx_TPI_Init();
	// #endif
	
	//sil902x_device_init();
	
}

extern void show_pattern();
void hdmi_loop(void)
{
    siHdmiTx_TPI_Poll();
    // HAL_Delay(5);
//    R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);  //Modify by GJ 7.12


//	while(1)
//	{
////	    APP_PRINT("siHdmiTx_TPI_Poll  -> ");
////	    show_pattern(1);
//	    siHdmiTx_TPI_Poll();
//        // HAL_Delay(5);
//        R_BSP_SoftwareDelay (5, BSP_DELAY_UNITS_MILLISECONDS);
//    }
}

