#include <stdio.h>

#include "hal_data.h"
#include "utils/log.h"

void __assert_func(const char * file, int line, const char * func, const char * expr)
{
    __disable_irq();
    LOG_PutsEndl("\r\n====== Assert Error ======");
    LOG_PutsEndl("File:");
    LOG_Puts(file);
    LOG_PrintfEndl(" at line: %d", line);
    LOG_Puts("Function: ");
    LOG_PutsEndl(func);
    LOG_Puts("Assert: ");
    LOG_PutsEndl(expr);
    while (1) {
        R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);
		R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
		R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW);
		R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

#if !defined(__clang_version__) && !defined(__ARMCOMPILER_VERSION)
#pragma GCC diagnostic ignored "-Wformat="
void HardFault_Handler(void);
#endif
void HardFault_Handler(void)
{
    uint32_t msp;
    uint32_t mmfsr;
    uint32_t bfsr;
    uint32_t ufsr;

    uint32_t *p_stack = NULL;

    __disable_irq();
    LOG_PutsEndl("\r\n=============== In HardFault ===============");
    if (SCB->HFSR & 0x80000000) {
        LOG_PutsEndl("HardFault occurred by DEBUG");
    }
    else if (SCB->HFSR & 0x40000000) {
        LOG_PutsEndl("HardFault occurred by other exception forced");
    }

    msp = __get_MSP();
    p_stack = (uint32_t *)msp;
    LOG_PutsEndl("========== MSP Stack  ==========");
    LOG_PrintfEndl("R0:  0x%08X", p_stack[0]);
    LOG_PrintfEndl("R1:  0x%08X", p_stack[1]);
    LOG_PrintfEndl("R2:  0x%08X", p_stack[2]);
    LOG_PrintfEndl("R3:  0x%08X", p_stack[3]);
    LOG_PrintfEndl("R12: 0x%08X", p_stack[4]);
    LOG_PrintfEndl("LR:  0x%08X", p_stack[5]);
    LOG_PrintfEndl("PC:  0x%08X", p_stack[6]);
    LOG_PrintfEndl("PSR: 0x%08X", p_stack[7]);

    LOG_PutsEndl("\r\n========= SCB Register =========");
    LOG_PrintfEndl("SCB->CSFR: 0x%08X", SCB->CFSR);
    LOG_PrintfEndl("SCB->ICSR: 0x%08X", SCB->ICSR);
    LOG_PrintfEndl("SCB->HFSR: 0x%08X", SCB->HFSR);

    LOG_PutsEndl("\r\n======== Other Register ========");
    LOG_PrintfEndl("IPSR: 0x%08X", __get_IPSR());

    /* https://support.arm.com/documentation/100235/0100/The-Cortex-M33-Peripherals/System-Control-Block/Configurable-Fault-Status-Register?lang=en */
    mmfsr = SCB->CFSR & 0xFF;
    bfsr = (SCB->CFSR & 0xFF00) >> 8;
    ufsr = (SCB->CFSR & 0xFFFF0000) >> 8;

    if (mmfsr & 0x01) {
    	LOG_PutsEndl("Instruction access violation");
    }
    if (mmfsr & 0x02) {
    	LOG_PutsEndl("Data access violation");
    }
    if (mmfsr & 0x08) {
    	LOG_PutsEndl("MemManage fault on unstacking for a return from exception");
    }
    if (mmfsr & 0x10) {
    	LOG_PutsEndl("MemManage fault on stacking for exception entry");
    }
    if (mmfsr & 0x20) {
    	LOG_PutsEndl("A MemManage fault occurred during floating-point lazy state preservation");
    }
    if (mmfsr & 0x80) {
    	LOG_PutsEndl("MMFAR holds a valid fault address");
    	LOG_PrintfEndl("MMFAR: 0x%08X", SCB->MMFAR);
    }

    if (bfsr & 0x01) {
    	LOG_PutsEndl("Instruction bus error");
    }
    if (bfsr & 0x02) {
    	LOG_PutsEndl("Precise data bus error");
    	LOG_PrintfEndl("BFAR: 0x%08X", SCB->BFAR);
    }
    if (bfsr & 0x08) {
    	LOG_PutsEndl("BusFault on unstacking for a return from exception");
    }
    if (bfsr & 0x10) {
    	LOG_PutsEndl("BusFault on stacking for exception entry");
    }
    if (bfsr & 0x20) {
    	LOG_PutsEndl("A bus fault occurred during floating-point lazy state preservation");
    }
    if (bfsr & 0x80) {
    	LOG_PutsEndl("BFAR holds a valid fault address");
    	LOG_PrintfEndl("BFAR: 0x%08X", SCB->BFAR);
    }

    if (ufsr & 0x01) {
    	LOG_PutsEndl("Undefined instruction");
    }
    if (ufsr & 0x02) {
    	LOG_PutsEndl("Invalid state");
    }
    if (ufsr & 0x04) {
    	LOG_PutsEndl("Invalid PC");
    }
    if (ufsr & 0x08) {
    	LOG_PutsEndl("No coprocessor");
    }
    if (ufsr & 0x10) {
    	LOG_PutsEndl("Stack overflow");
    	LOG_PrintfEndl("MSP:    0x%08X", msp);
        LOG_PrintfEndl("MSPLIM: 0x%08X", __get_MSPLIM());
        LOG_PrintfEndl("PSP:    0x%08X", __get_PSP());
        LOG_PrintfEndl("PSPLIM: 0x%08X", __get_PSPLIM());
    }
    if (ufsr & 0x0100) {
    	LOG_PutsEndl("Unaligned access");
    }
    if (ufsr & 0x0200) {
    	LOG_PutsEndl("Divide by zero");
    }

    __BKPT(0);
}
