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
#endif

void HardFault_Handler(void) __attribute__((naked));
static void hard_fault_print(const uint32_t *p_stack, uint32_t exc_return, uint32_t msp, uint32_t psp)
    __attribute__((used, noinline));

/**
 * @brief	保存异常入口的栈指针和 EXC_RETURN，再进入 C 处理函数。
 */
void HardFault_Handler(void)
{
    /* No C statements here: a function prologue would change MSP and calls would overwrite LR. */
    __asm volatile (
        "cpsid i                    \n"
        "mrs r2, msp                \n"
        "mrs r3, psp                \n"
        "mov r1, lr                 \n"
        "tst r1, #4                 \n"
        "ite eq                     \n"
        "moveq r0, r2               \n"
        "movne r0, r3               \n"
        "b hard_fault_print         \n"
    );
}

/**
 * @brief	打印异常栈帧和故障寄存器，随后停机。
 * @param	p_stack    EXC_RETURN 指定的异常栈帧。
 * @param	exc_return 异常入口的 LR。
 * @param	msp        异常入口的 MSP。
 * @param	psp        异常入口的 PSP。
 */
static void hard_fault_print(const uint32_t *p_stack, uint32_t exc_return, uint32_t msp, uint32_t psp)
{
    const uint32_t cfsr = SCB->CFSR;

    LOG_PutsEndl("\r\n=============== In HardFault ===============");
    if (SCB->HFSR & 0x80000000) {
        LOG_PutsEndl("HardFault occurred by DEBUG");
    }
    else if (SCB->HFSR & 0x40000000) {
        LOG_PutsEndl("HardFault occurred by other exception forced");
    }

    LOG_PrintfEndl("EXC_RETURN: 0x%08X", exc_return);
    LOG_PrintfEndl("MSP (entry): 0x%08X", msp);
    LOG_PrintfEndl("PSP (entry): 0x%08X", psp);
    if (exc_return & EXC_RETURN_SPSEL) {
        LOG_PutsEndl("========== PSP Stack  ==========");
    }
    else {
        LOG_PutsEndl("========== MSP Stack  ==========");
    }
    LOG_PrintfEndl("Frame: 0x%08X", (uint32_t) p_stack);
    LOG_PrintfEndl("Frame type: %s", (exc_return & EXC_RETURN_FTYPE) ? "Basic" : "Extended FP/MVE");

    if ((cfsr & (SCB_CFSR_MSTKERR_Msk | SCB_CFSR_STKERR_Msk | SCB_CFSR_STKOF_Msk)) ||
        (p_stack == NULL) || (((uint32_t) p_stack & 0x3U) != 0U)) {
        LOG_PutsEndl("Stack frame unavailable: stacking failed, stack overflow or invalid alignment.");
    }
    else {
        /*
         * In the same-security-state Armv8-M frame, R0..xPSR are the first eight words.
         * FP/MVE context and stack alignment padding follow them, including with lazy stacking.
         */
        LOG_PrintfEndl("R0:  0x%08X", p_stack[0]);
        LOG_PrintfEndl("R1:  0x%08X", p_stack[1]);
        LOG_PrintfEndl("R2:  0x%08X", p_stack[2]);
        LOG_PrintfEndl("R3:  0x%08X", p_stack[3]);
        LOG_PrintfEndl("R12: 0x%08X", p_stack[4]);
        LOG_PrintfEndl("LR:  0x%08X", p_stack[5]);
        LOG_PrintfEndl("PC:  0x%08X", p_stack[6]);
        LOG_PrintfEndl("PSR: 0x%08X", p_stack[7]);
    }

    LOG_PutsEndl("\r\n========= SCB Register =========");
    LOG_PrintfEndl("SCB->CFSR: 0x%08X", cfsr);
    LOG_PrintfEndl("SCB->ICSR: 0x%08X", SCB->ICSR);
    LOG_PrintfEndl("SCB->HFSR: 0x%08X", SCB->HFSR);

    LOG_PutsEndl("\r\n======== Other Register ========");
    LOG_PrintfEndl("IPSR: 0x%08X", __get_IPSR());

    if (cfsr & 0x02000000) {
        LOG_PutsEndl("Checked Error: Divide By Zero");
    }
    if (cfsr & 0x01000000) {
        LOG_PutsEndl("Checked Error: Unaligned Access");
    }
    if (cfsr & 0x00100000) {
        LOG_PutsEndl("Checked Error: Stack Overflow");
        LOG_PrintfEndl("MSP:    0x%08X", msp);
        LOG_PrintfEndl("MSPLIM: 0x%08X", __get_MSPLIM());
        LOG_PrintfEndl("PSP:    0x%08X", psp);
        LOG_PrintfEndl("PSPLIM: 0x%08X", __get_PSPLIM());
    }
    if (cfsr & 0x00080000) {
        LOG_PutsEndl("Checked Error: No Coprocessor");
    }
    if (cfsr & 0x00040000) {
        LOG_PutsEndl("Checked Error: Invalid PC");
    }
    if (cfsr & 0x00020000) {
        LOG_PutsEndl("Checked Error: Invalid State");
    }
    if (cfsr & 0x00010000) {
        LOG_PutsEndl("Checked Error: Undefined Instruction");
    }
    if (cfsr & 0x00000200) {
        LOG_PutsEndl("Checked Error: Precise data bus error");
        if (cfsr & 0x00008000) {
            LOG_PrintfEndl("BFAR is valid: 0x%08X\r\n", SCB->BFAR);
        }
    }
    if (cfsr & 0x00000100) {
        LOG_PutsEndl("Checked Error: Instruction Bus Error");
    }

    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) {
        __BKPT(0);
    }
    /*
     * Keep this function returning from the compiler's point of view so it saves LR
     * (EXC_RETURN) and callee-saved registers, and emits their debug unwind rules.
     * A noreturn attribute or a C infinite loop lets LLVM omit those saves.
     * The assembly loop still halts permanently, including after resuming from BKPT.
     */
    __asm volatile (
        "1: b 1b                    \n"
    );
}
