/**********************************************************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO
 * THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2025 Renesas Electronics Corporation. All rights reserved.
 *********************************************************************************************************************/
/******************************************************************************************************************//**
 * @file          LoaderFunc.c
 * @version       1.00
 * @brief         Implements functions declared in LoaderFunc.h for external flash programming.
 *********************************************************************************************************************/
/**********************************************************************************************************************
 * History : DD.MM.YYYY Version  Description
 *         : 01.01.2026 1.00     First Release
 *********************************************************************************************************************/

/**********************************************************************************************************************
 Includes   <System Includes> , "Project Includes"
 *********************************************************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "MX25LW51245G.h"
#include "RA8P1.h"

/**********************************************************************************************************************
 Macro definitions
 *********************************************************************************************************************/
// STATUS CODE
#define STATUS_OK    (0x00000000)
#define STATUS_ERROR (0xFFFFFFFF)

#define M8(adr)  (*((volatile uint8_t *)(adr)))     /* 8-bit access to a memory-mapped register or address */
#define M16(adr) (*((volatile uint16_t *)(adr)))    /* 16-bit access to a memory-mapped register or address */
#define M32(adr) (*((volatile uint32_t *)(adr)))    /* 32-bit access to a memory-mapped register or address */

#define REG_SET_BIT(reg, bit) ((reg) |= (1U << (bit)))  /* Set a specific bit in a register */
#define REG_CLR_BIT(reg, bit) ((reg) &= ~(1U << (bit))) /* Clear a specific bit in a register */
#define REG_SET(data, new_data, pos, mask) \
    ((data) = (((data) & ~(mask)) | (((new_data) << (pos)) & (mask))))    /* Update a field in a register using position and mask */

/* Macro that waits until a hardware register matches the required value */
#define HARDWARE_REGISTER_WAIT(reg_val, required_value) \
    do { /* Wait until the values match */ } while ((reg_val) != (required_value))

/* Macro that returns (result) if it is not STATUS_OK */
#define RETURN_IF_ERROR(result) \
    do {                        \
        int32_t ret = (result); \
        if (ret != STATUS_OK) { \
            return ret;         \
        }                       \
    } while (0)

#define OSPI_MEMORY_START_ADDRESS (OSPI_CS0_START_ADDRESS)
#define OSPI_MEMORY(addr)         ((uint8_t *)(addr))   /* Byte access to memory */

/**********************************************************************************************************************
 Local Typedef definitions
 *********************************************************************************************************************/

// Direction of transaction (Write or Read)
typedef enum xspi_transaction_direction_type
{
    DIRECTION_READ = 0, DIRECTION_WRITE = 1,
} xspi_transaction_direction_t;

// Transaction format in manual command mode
typedef struct xspi_transaction_type
{
    union
    {
        uint64_t data_u64;
        uint32_t data;
    };
    uint32_t address;
    uint16_t command;
    uint8_t dummy_cycle_num;
    uint8_t command_len;
    uint8_t address_len;
    uint8_t data_len;
    xspi_transaction_direction_t direction;
} xspi_transaction_t;

/**********************************************************************************************************************
 Exported global variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 Private (static) variables and functions
 *********************************************************************************************************************/
static void SendRecvTransactionManualMode(xspi_transaction_t *const p_frame);
static void Wait_us(uint32_t us);

static int32_t Initialize(uint32_t rfu);
static int32_t EraseSector(uint32_t addr);
static int32_t EraseChip(void);
static int32_t ProgramData(uint32_t addr, const void *data, uint32_t cnt);
static int32_t ReadData(uint32_t addr, void* data, uint32_t cnt);

/* Function Name: R_Flash_Initialize */
/******************************************************************************************************************//**
 * Initialize the external flash memory interface.
 * @param[in] rfu   Unused (reserved for future use)
 * 
 *.@return Status code of the operation.
 * @retval STATUS_OK            Operation successful
 * @retval STATUS_ERROR         Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_initialize"))) R_Flash_Initialize(uint32_t rfu)
{
    return Initialize (rfu);
}

/* Function Name: R_Flash_EraseSector */
/******************************************************************************************************************//**
 * Erase flash memory sector.
 * @param[in] addr   Sector address
 * 
 *.@return Status code of the operation.
 * @retval Erased size [byte]   Operation successful
 * @retval STATUS_ERROR         Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_erase_sector"))) R_Flash_EraseSector(uint32_t addr)
{
    return EraseSector (addr);
}

/* Function Name: R_Flash_EraseChip */
/******************************************************************************************************************//**
 * Erase complete flash memory.
 * 
 *.@return Status code of the operation.
 * @retval STATUS_OK            Operation successful
 * @retval STATUS_ERROR         Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_erase_chip"))) R_Flash_EraseChip(void)
{
    return EraseChip ();
}

/* Function Name: R_Flash_ProgramData */
/******************************************************************************************************************//**
 * Program data to flash memory.
 * @param[in] addr   Data address
 * @param[in] data   Pointer to a buffer containing the data to be programmed to flash memory 
 * @param[in] cnt    Number of data items to program
 * 
 *.@return Status code of the operation.
 * @retval Programmed size [byte]   Operation successful
 * @retval STATUS_ERROR             Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_program_data"))) R_Flash_ProgramData(uint32_t addr, const void *data, uint32_t cnt)
{
    return ProgramData (addr, data, cnt);
}

/* Function Name: R_Flash_Uninitialize */
/******************************************************************************************************************//**
 * Uninitialize the external flash memory interface.
 * @note This function is not used for RA8.
 * @param[in] rfu   Unused (reserved for future use)
 * 
 *.@return Status code of the operation.
 * @retval STATUS_OK            Operation successful
 * @retval STATUS_ERROR         Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_uninitialize"))) R_Flash_Uninitialize(uint32_t rfu __attribute__((unused)))
{
    return STATUS_OK;
}

/* Function Name: R_Flash_ReadData */
/******************************************************************************************************************//**
 * Read data from flash memory.
 * @note This function is not used for RA8.
 * @param[in] addr   Data address
 * @param[in] data   Pointer to a buffer containing the data to be read from flash memory
 * @param[in] cnt    Number of data items to read
 * 
 *.@return Status code of the operation.
 * @retval Read size [byte]     Operation successful
 * @retval STATUS_ERROR         Operation failuer
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_read_data"))) R_Flash_ReadData(uint32_t addr, void* data, uint32_t cnt)
{
    return ReadData(addr, data, cnt);
}

/* Function Name: R_Flash_CalcCRC */
/******************************************************************************************************************//**
 * Calculate the CRC value from flash memory.
 * @note This function is not used for RA8.
 * @param[in] crc       CRC initial value
 * @param[in] addr      Data address
 * @param[in] cnt       Number of data items
 * @param[in] polynom   Polynomial used for CRC calculation
 * 
 *.@return Status code of the operation.
 * @retval CRC value    Operation successful
 *********************************************************************************************************************/
int32_t __attribute__((section("ep_calc_crc"))) R_Flash_CalcCRC(uint32_t crc, uint32_t addr, uint32_t cnt, uint32_t polynom)
{
	(void)crc;
	(void)addr;
	(void)cnt;
	(void)polynom;

    return STATUS_OK;
}

/**********************************************************************************************************************
 Private functions
 *********************************************************************************************************************/
/**
 * @brief Enable writing to the port function selection register (PmnPFS)
 */
static inline void EnableAccessPFS()
{
    REG_CLR_BIT(M8(PFS_PWPR), PWPR_B0WI_Pos);
    REG_SET_BIT(M8(PFS_PWPR), PWPR_PFSWE_Pos);
}

/**
 * @brief Disable writing to the port function selection register (PmnPFS)
 */
static inline void DisableAccessPFS()
{
    REG_CLR_BIT(M8(PFS_PWPR), PWPR_PFSWE_Pos);
    REG_SET_BIT(M8(PFS_PWPR), PWPR_B0WI_Pos);
}

/**
 * @brief Select port functions for either General-purpose I/O or peripheral
 * functions
 *
 * @param use_peripheral Select peripheral function
 */
static inline void SetPortMode(bool use_peripheral)
{
    if (use_peripheral)
    {
        REG_SET_BIT(M32(PFS_OM_SCLK), PmnPFS_PMR_Pos);
        REG_SET_BIT(M32(PFS_OM_CS0), PmnPFS_PMR_Pos);
        REG_SET_BIT(M32(PFS_OM_SIO0), PmnPFS_PMR_Pos);
        REG_SET_BIT(M32(PFS_OM_SIO1), PmnPFS_PMR_Pos);
        REG_SET_BIT(M32(PFS_OM_SIO2), PmnPFS_PMR_Pos);
        REG_SET_BIT(M32(PFS_OM_SIO3), PmnPFS_PMR_Pos);
    }
    else
    {
        REG_CLR_BIT(M32(PFS_OM_SCLK), PmnPFS_PMR_Pos);
        REG_CLR_BIT(M32(PFS_OM_CS0), PmnPFS_PMR_Pos);
        REG_CLR_BIT(M32(PFS_OM_SIO0), PmnPFS_PMR_Pos);
        REG_CLR_BIT(M32(PFS_OM_SIO1), PmnPFS_PMR_Pos);
        REG_CLR_BIT(M32(PFS_OM_SIO2), PmnPFS_PMR_Pos);
        REG_CLR_BIT(M32(PFS_OM_SIO3), PmnPFS_PMR_Pos);
    }
}

/**
 * @brief Set the PmnPFS register
 *
 * @param podr Port output data
 * @param pidr Port input data
 * @param pdr Port direction
 * @param psel Port select mode (GPIO or Peripheral)
 * @return uint32_t PmnPFS register value
 */
static inline uint32_t SetupPmnPFS(uint8_t podr, uint8_t pidr, uint8_t pdr, uint8_t psel)
{
    uint32_t pmnpfs = 0;
    pmnpfs |= (podr << PmnPFS_PODR_Pos);
    pmnpfs |= (pidr << PmnPFS_PIDR_Pos);
    pmnpfs |= (pdr << PmnPFS_PDR_Pos);
    pmnpfs |= (psel << PmnPFS_PSEL_Pos);
    return pmnpfs;
}

/**
 * @brief Initialize OSPI pin
 */
static void SetupPinForOSPI(void)
{
    // Permission to access the PmnPFS register
    EnableAccessPFS ();
    // Change the port mode to general-purpose I/O (GPIO)
    SetPortMode (false);

    M32(PFS_OM_SCLK) = SetupPmnPFS (PODR_LOW, PIDR_LOW, PDR_OUT, PSEL_OSPI);
    M32(PFS_OM_CS0) = SetupPmnPFS (PODR_HIGH, PIDR_HIGH, PDR_OUT, PSEL_OSPI);
    M32(PFS_OM_SIO0) = SetupPmnPFS (PODR_LOW, PIDR_LOW, PDR_OUT, PSEL_OSPI);
    M32(PFS_OM_SIO1) = SetupPmnPFS (PODR_LOW, PIDR_LOW, PDR_OUT, PSEL_OSPI);
    M32(PFS_OM_SIO2) = SetupPmnPFS (PODR_LOW, PIDR_LOW, PDR_OUT, PSEL_OSPI);
    M32(PFS_OM_SIO3) = SetupPmnPFS (PODR_LOW, PIDR_LOW, PDR_OUT, PSEL_OSPI);

    // Change the port mode to peripheral function
    SetPortMode (true);
    // Prohibit access to the PmnPFS register
    DisableAccessPFS ();
}

/**
 * @brief Hardware reset of external memory devices
 */
static void HardwareReset(void)
{
    xspi_transaction_t cmd;

    cmd.address = 0x00;
    cmd.address_len = 0x00;
    cmd.command = 0x66;
    cmd.command_len = 0x01;
    cmd.data_len = 0x00;
    cmd.data_u64 = 0x00;
    cmd.direction = DIRECTION_WRITE;
    cmd.dummy_cycle_num = 0x00;
    SendRecvTransactionManualMode(&cmd);

    cmd.command = 0x99;
    SendRecvTransactionManualMode(&cmd);
    Wait_us(30);
}

/**
 * @brief Initial settings for memory mapping mode
 *
 * @return int32_t Status code
 */
static void SetupMemoryMappingMode(void)
{
    // Setting the SPI protocol mode
    const uint32_t liocfg_prtmd = OSPI_LIOCFGCS_PRTMD_1S_1S_1S;  // Protocol mode is 1S-1S-1S
    REG_SET(M32(OSPI_LIOCFGCS0), liocfg_prtmd, OSPI_LIOCFGCS_PRTMD_Pos, OSPI_LIOCFGCS_PRTMD_Msk);

    // Setting the memory-mapped mode
    uint32_t cmcfg0 = 0, cmcfg1 = 0, cmcfg2 = 0;

    cmcfg0 |= ((0x0 << OSPI_CMCFGCS_CMCFG0_FFMT_Pos) & OSPI_CMCFGCS_CMCFG0_FFMT_Msk);         // Normal format
    cmcfg0 |= ((0x3 << OSPI_CMCFGCS_CMCFG0_ADDSIZE_Pos) & OSPI_CMCFGCS_CMCFG0_ADDSIZE_Msk);   // Address size: 4 bytes
    cmcfg0 |= ((0xF0 << OSPI_CMCFGCS_CMCFG0_ADDRPEN_Pos) & OSPI_CMCFGCS_CMCFG0_ADDRPEN_Msk);  // Allow address replacement

    // For a 1-byte command, use only the upper 1 byte
    const uint16_t read_command = (uint16_t) ((SPI_COMMAND_NORMAL_READ & 0xFF) << 8);
    const uint16_t write_command = (uint16_t) ((SPI_COMMAND_PROGRAM_PAGE & 0xFF) << 8);

    // Setting read transactions in memory-mapped mode
    cmcfg1 |= ((uint32_t) read_command << OSPI_CMCFGCS_CMCFG1_RDCMD_Pos) & OSPI_CMCFGCS_CMCFG1_RDCMD_Msk;
    cmcfg1 |= (0 << OSPI_CMCFGCS_CMCFG1_RDLATE_Pos) & OSPI_CMCFGCS_CMCFG1_RDLATE_Msk;

    // Setting write transactions in memory-mapped mode
    cmcfg2 |= ((uint32_t) write_command << OSPI_CMCFGCS_CMCFG2_WRCMD_Pos) & OSPI_CMCFGCS_CMCFG2_WRCMD_Msk;
    cmcfg2 |= (0 << OSPI_CMCFGCS_CMCFG2_WRLATE_Pos) & OSPI_CMCFGCS_CMCFG2_WRLATE_Msk;

    // Write to the register for Channel 1
    M32(OSPI_CMCFG0CS0) = cmcfg0;
    M32(OSPI_CMCFG1CS0) = cmcfg1;
    M32(OSPI_CMCFG2CS0) = cmcfg2;

    // Setting optional features in memory-mapped mode
    uint32_t bmcfgch = 0;
    bmcfgch |= ((0x0 << OSPI_BMCFGCH_WRMD_Pos) & OSPI_BMCFGCH_WRMD_Msk);        // Disable response after write
    bmcfgch |= ((0x1 << OSPI_BMCFGCH_MWRCOMB_Pos) & OSPI_BMCFGCH_MWRCOMB_Msk);  // Enable concatenation mode
    bmcfgch |= ((0xF << OSPI_BMCFGCH_MWRSIZE_Pos) & OSPI_BMCFGCH_MWRSIZE_Msk);  // Maximum concatenation size: 64 bytes
    bmcfgch |= ((0x1 << OSPI_BMCFGCH_PREEN_Pos) & OSPI_BMCFGCH_PREEN_Msk);      // Enable prefetch

    M32(OSPI_BMCFGCH0) = bmcfgch;
    M32(OSPI_BMCFGCH1) = bmcfgch;
}

/**
 * @brief Transaction process in manual command mode
 *
 * @param p_frame Command frame format
 * @return int32_t Status code
 */
static void SendRecvTransactionManualMode(xspi_transaction_t *const p_frame)
{
    // Select CS0
    REG_CLR_BIT(M32(OSPI_CDCTL0), OSPI_CDCTL0_CSSEL_Pos);

    // Canceling the transaction
    REG_CLR_BIT(M32(OSPI_CDCTL0), OSPI_CDCTL0_TRREQ_Pos);
    // Waiting for transaction abort
    HARDWARE_REGISTER_WAIT(M32(OSPI_CDCTL0) & OSPI_CDCTL0_TRREQ_Msk, 0);

    // Setting the command format
    uint32_t cdtbuf0 = 0;
    cdtbuf0 |= ((uint32_t) p_frame->command_len << OSPI_CDBUF_CDT_CMDSIZE_Pos) & OSPI_CDBUF_CDT_CMDSIZE_Msk;
    cdtbuf0 |= ((uint32_t) p_frame->address_len << OSPI_CDBUF_CDT_ADDSIZE_Pos) & OSPI_CDBUF_CDT_ADDSIZE_Msk;
    cdtbuf0 |= ((uint32_t) p_frame->data_len << OSPI_CDBUF_CDT_DATASIZE_Pos) & OSPI_CDBUF_CDT_DATASIZE_Msk;
    cdtbuf0 |= ((uint32_t) p_frame->dummy_cycle_num << OSPI_CDBUF_CDT_LATE_Pos) & OSPI_CDBUF_CDT_LATE_Msk;
    cdtbuf0 |= ((uint32_t) p_frame->direction << OSPI_CDBUF_CDT_TRTYPE_Pos) & OSPI_CDBUF_CDT_TRTYPE_Msk;
    cdtbuf0 |= (p_frame->command_len == 1) ? ((p_frame->command << OSPI_CDBUF_CDT_CMD_1B_Pos) &
    OSPI_CDBUF_CDT_CMD_1B_Msk) :
                                             ((p_frame->command << OSPI_CDBUF_CDT_CMD_2B_Pos) &
                                             OSPI_CDBUF_CDT_CMD_2B_Msk);

    M32(OSPI_CDTBUF0) = cdtbuf0;
    M32(OSPI_CDABUF0) = p_frame->address;  // Setting the address field

    // Setting the data field
    if (p_frame->direction == DIRECTION_WRITE)
    {
        M32(OSPI_CDD0BUF0) = (uint32_t) (p_frame->data_u64 & UINT32_MAX);
        if (p_frame->data_len > sizeof(uint32_t))
        {
            M32(OSPI_CDD1BUF0) = (uint32_t) (p_frame->data_u64 >> 32);
        }
    }

    // Start of transaction
    REG_SET_BIT(M32(OSPI_CDCTL0), OSPI_CDCTL0_TRREQ_Pos);
    HARDWARE_REGISTER_WAIT(M32(OSPI_INTS) & OSPI_INTS_CMDCMP_Msk, OSPI_INTS_CMDCMP_Msk);

    // Reading received data
    if (p_frame->direction == DIRECTION_READ)
    {
        p_frame->data_u64 = (uint64_t) M32(OSPI_CDD0BUF0);
        if (p_frame->data_len > sizeof(uint32_t))
        {
            p_frame->data_u64 |= (((uint64_t) M32(OSPI_CDD1BUF0)) << 32);
        }
    }

    // Clear state of interrupt
    REG_SET_BIT(M32(OSPI_INTC), OSPI_INTC_CMDCMPC_Pos);
}

/**
 * @brief Initial settings for RA8M1 OSPI controller
 */
static void InitializeOspiController(void)
{
    uint32_t liocfg = OSPI_LIOCFGCS_PRTMD_1S_1S_1S << OSPI_LIOCFGCS_PRTMD_Pos;
    // Prohibit read/write access from the system bus
    M32(OSPI_BMCTL0) = M32(OSPI_BMCTL0) & ~OSPI_BMCTL0_CH0CS0ACC_Msk;
    // Setting the SPI protocol mode
    M32(OSPI_LIOCFGCS0) = liocfg;
    // Setting the sampling timing of xSPI
    M32(OSPI_WRAPCFG) = (0 << OSPI_WRAPCFG_DSSFTCS0_Pos) & OSPI_WRAPCFG_DSSFTCS0_Msk;

    // Setting the timing of the CS pin
    liocfg |= (0 << OSPI_LIOCFGCS_CSMIN_Pos) & OSPI_LIOCFGCS_CSMIN_Msk;
    liocfg |= (0 << OSPI_LIOCFGCS_CSASTEX_Pos) & OSPI_LIOCFGCS_CSASTEX_Msk;
    liocfg |= (0 << OSPI_LIOCFGCS_CSNEGEX_Pos) & OSPI_LIOCFGCS_CSNEGEX_Msk;
    M32(OSPI_LIOCFGCS0) = liocfg;

    // Setting of memory mapping mode
    SetupMemoryMappingMode ();

    // Enable memory mapping mode
    M32(OSPI_BMCTL0) = M32(OSPI_BMCTL0) | OSPI_BMCTL0_CH0CS0ACC_Msk;
}

/**
 * @brief  Execution of Write Enable transaction
 */
static void COM_WriteEnable(void)
{
    xspi_transaction_t frame;
    frame.data_u64 = 0;
    frame.address = 0;
    frame.command = SPI_COMMAND_WRITE_ENABLE;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 0;
    frame.data_len = 0;
    frame.direction = DIRECTION_WRITE;

    SendRecvTransactionManualMode (&frame);
}

/**
 * @brief Execution of Read Device ID transaction
 */
static void COM_ReadDeviceID(uint8_t *manufacturer_id, uint8_t *device_type, uint8_t *device_density)
{
    xspi_transaction_t frame;
    frame.data_u64 = 0;
    frame.address = 0;
    frame.command = SPI_COMMAND_READ_DEVICE_ID;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 0;
    frame.data_len = 3;
    frame.direction = DIRECTION_READ;

    SendRecvTransactionManualMode (&frame);

    *manufacturer_id = (uint8_t) ((frame.data >> 0) & 0xFF);
    *device_type = (uint8_t) ((frame.data >> 8) & 0xFF);
    *device_density = (uint8_t) ((frame.data >> 16) & 0xFF);
}

/**
 * @brief Execution of Read Status Register transaction
 *
 * @param status_reg_val Read value of Status register
 */
static void COM_ReadStatusRegister(uint8_t *status_reg_val)
{
    xspi_transaction_t frame;
    frame.data_u64 = 0;
    frame.address = 0;
    frame.command = SPI_COMMAND_READ_STATUS_REGISTER;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 0;
    frame.data_len = 1;
    frame.direction = DIRECTION_READ;

    SendRecvTransactionManualMode (&frame);
    *status_reg_val = (uint8_t) (frame.data & 0xFF);
}

/**
 * @brief Execution of Read Configuration Register 2 transaction
 * 
 * @param address address
 * @param config_reg_val Read value of Configuration register 2
 */
static void COM_ReadConfigurationRegister2(uint32_t address, uint8_t *config_reg_val)
{
    xspi_transaction_t frame;
    frame.data_u64 = 0;
    frame.address = address;
    frame.command = SPI_COMMAND_READ_CONFIG_REGISTER2;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 4;
    frame.data_len = 1;
    frame.direction = DIRECTION_READ;

    SendRecvTransactionManualMode (&frame);
    *config_reg_val = (uint8_t) (frame.data & 0xFF);
}

/**
 * @brief Execution of Write Configuration Register 2 transaction
 * 
 * @param address address
 * @param config_reg_val Write value of Configuration register 2
 */
static void COM_WriteConfigurationRegister2(uint32_t address, uint8_t config_reg_val)
{
    xspi_transaction_t frame;
    frame.data_u64 = config_reg_val;
    frame.address = address;
    frame.command = SPI_COMMAND_WRITE_CONFIG_REGISTER2;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 4;
    frame.data_len = 1;
    frame.direction = DIRECTION_WRITE;

    SendRecvTransactionManualMode (&frame);
}

/**
 * @brief Execution of Read Security Register transaction
 * 
 * @param security_reg_val Read value of Security register
 */
static void COM_ReadSecurityRegister(uint8_t *security_reg_val)
{
    xspi_transaction_t frame;
    frame.data_u64 = 0;
    frame.address = 0;
    frame.command = SPI_COMMAND_READ_SECURITY_REGISTER;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 0;
    frame.data_len = 1;
    frame.direction = DIRECTION_READ;

    SendRecvTransactionManualMode (&frame);
    *security_reg_val = (uint8_t) (frame.data & 0xFF);
}

/**
 * @brief Send Write Enable transaction and check write enable bit
 *
 * @param is_bit_check Whether the write enable bit is checked or not
 * @return int32_t Status code
 */
static int32_t WriteEnable(bool is_bit_check)
{
    COM_WriteEnable ();

    // Check write enable bit
    if (is_bit_check)
    {
        uint8_t read_status_val = 0;
        COM_ReadStatusRegister (&read_status_val);

        if ((read_status_val & STATUS_REG_WEL_MSK) != STATUS_REG_WEL_MSK)
        {
            return STATUS_ERROR;
        }
    }

    return STATUS_OK;
}

/**
 * @brief Check transaction results
 *
 * @param err_check_mask Mask for security register bit
 * @return int32_t Status code
 */
static int32_t CheckCommandStatus(uint8_t err_check_mask)
{
    int32_t status = STATUS_OK;
    uint8_t reg_val = 0;

    (void)err_check_mask;

    for (;;)
    {
    	Wait_us(10);
        COM_ReadStatusRegister (&reg_val);

        // Check command completed
        if ((reg_val & STATUS_REG_WIP_MSK) == 0)
        {
            status = STATUS_OK;
            break;
        }
    }

    return status;
}

/**
 * @brief Execution of Erase Block transaction
 *
 * @param address Erase address
 */
static void COM_EraseBlock(uint32_t address)
{
    xspi_transaction_t frame;
    frame.command = SPI_COMMAND_BLOCK_ERASE;
    frame.address = address;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 4;
    frame.data_len = 0;
    frame.direction = DIRECTION_WRITE;

    SendRecvTransactionManualMode (&frame);
}

/**
 * @brief Execution of Erase Chip transaction
 */
static void COM_EraseChip(void)
{
    xspi_transaction_t frame;
    frame.command = SPI_COMMAND_CHIP_ERASE;
    frame.address = 0;
    frame.dummy_cycle_num = 0;
    frame.command_len = 1;
    frame.address_len = 0;
    frame.data_len = 0;
    frame.direction = DIRECTION_WRITE;

    SendRecvTransactionManualMode (&frame);
}

/**
 * @brief Check target device by comparing Device ID
 *
 * @return true Device ID matches target device
 * @return false Device ID does not matches target device
 */
static bool IsTargetSpiDevice(void)
{
    uint8_t manufacturer_id = 0;
    uint8_t device_type = 0;
    uint8_t device_density = 0;

    COM_ReadDeviceID (&manufacturer_id, &device_type, &device_density);

    return (manufacturer_id == MANUFACTURER_ID) && (device_type == DEVICE_ID_MEM_TYPE)
            && (device_density == DEVICE_ID_MEM_DENSITY);
}

/**
 * @brief Initialization of volatile configuration registers in external memory
 *
 * @return int32_t Status code
 */
static int32_t SetupConfigurationRegisters(void)
{
	xspi_transaction_t cmd;

    /* Enter 4 byte address mode */
	cmd.address = 0x00;
	cmd.address_len = 0x00;
	cmd.command = 0xB7;
	cmd.command_len = 0x01;
	cmd.data_len = 0x00;
	cmd.data_u64 = 0x00;
	cmd.direction = DIRECTION_WRITE;
	cmd.dummy_cycle_num = 0x00;
	SendRecvTransactionManualMode(&cmd);

    return STATUS_OK;
}

/**
 * @brief NOP wait function
 *
 * @param us Wait time (unit: us)
 * 
 * @warning 
 * Since NOP wait is used, the wait time is inaccurate and may result in a large margin of error.
 */
static void Wait_us(uint32_t us)
{
    uint32_t dwCount;
    uint32_t dwTotalNsPerCycle;

    dwTotalNsPerCycle = 1000000000 / CPU_OPERATING_FREQUENCY;  // Calculate ns per cycle
    if (dwTotalNsPerCycle == 0)
    {
        // Fail-safe when exceeding 100Mhz
        dwTotalNsPerCycle = 1;
    }
    // Calculates the number of cycles required to wait for the specified us unit
    // time
    dwCount = (us * 1000) / dwTotalNsPerCycle;
    // Since it takes at least about 14 cycles to execute one for statement, the
    // number of executions of it is calculated based on that.
    dwCount = dwCount / 6;

    if (dwCount == 0)
    {
        // Fail-safe if dwCount is below 14
        dwCount = 1;
    }

    for (; dwCount > 0; dwCount--)
    {
        __asm("NOP");
    }
}

/**
 * @brief Write combined data to SPI memory
 */
#define ACCESS_ALIGNMENT (8U)
static int32_t ProgramCombineData(uint32_t addr, const void *data, uint32_t cnt)
{
    if ((cnt % ACCESS_ALIGNMENT) != 0)
    {
        return STATUS_ERROR;
    }

    uint64_t *p_dest64 = (uint64_t*) ((uint32_t) OSPI_MEMORY(addr) & ~(ACCESS_ALIGNMENT - 1));
    uint64_t *p_src64 = (uint64_t*) data;
    uint32_t write_size = cnt;

    RETURN_IF_ERROR(WriteEnable(true));

    while (sizeof(uint64_t) <= write_size)
    {
        *p_dest64 = *p_src64;
        p_dest64++;
        p_src64++;
        write_size -= sizeof(uint64_t);
    }

    M32(OSPI_BMCTL1) = (0x01 << 8UL);

    return CheckCommandStatus (SECURITY_REG_PROGRAM_FAIL_MSK);
}

/**
 * @brief Initialization function body
 *
 * @param rfu Unused parameter
 * @return int32_t Status code
 */
static int32_t Initialize(uint32_t rfu)
{
    (void) rfu;

    // OSPI pin configuration
    SetupPinForOSPI ();

    // Initialize the OSPI controller
    InitializeOspiController ();

    // SPI flash hardware reset
    HardwareReset ();

    // Initialization of SPI flash
    RETURN_IF_ERROR(SetupConfigurationRegisters ());

    // Read JEDEC ID and verify device
    if (!IsTargetSpiDevice ())
    {
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

/**
 * @brief Erase Sector function body
 *
 * @param addr Address in erase sector
 * @return Success: Erase size (byte)
 * @return Failuer: 0xFFFFFFFF
 */
static int32_t EraseSector(uint32_t addr)
{
    int32_t status = STATUS_OK;

    // Check address
    if ((addr < OSPI_MEMORY_START_ADDRESS) || (addr >= (OSPI_MEMORY_START_ADDRESS + MEMORY_SIZE)))
    {
        return STATUS_ERROR;
    }
    uint32_t start_addr = addr - OSPI_MEMORY_START_ADDRESS;  // Adjust to 0 base address

    RETURN_IF_ERROR(WriteEnable(true));
    COM_EraseBlock (start_addr);

    status = CheckCommandStatus (SECURITY_REG_ERASE_FAIL_MSK);
    return (status == STATUS_OK) ? BLOCK_64KB_SIZE : STATUS_ERROR;
}

/**
 * @brief Erase Chip function body
 *
 * @return Success: 0x00000000
 * @return Failuer: 0xFFFFFFFF
 */
static int32_t EraseChip(void)
{
    RETURN_IF_ERROR(WriteEnable(true));
    COM_EraseChip ();

    return CheckCommandStatus (SECURITY_REG_ERASE_FAIL_MSK);
}

/**
 * @brief Program Data function body
 *
 * @param addr write address
 * @param data Write data
 * @param cnt  write size
 * @return Success: Write size (Byte)
 * @return Failuer: 0xFFFFFFFF
 */
static int32_t ProgramData(uint32_t addr, const void *data, uint32_t cnt)
{
    int32_t status = STATUS_OK;
    const uint32_t combine_size = 64;
    uint32_t start_addr = addr;
    uint8_t *p_data = (uint8_t*) data;
    uint32_t remain = cnt;

    // Alignment check by write unit
    if ((addr % combine_size) != 0)
    {
        return STATUS_ERROR;
    }

    while (remain > 0)
    {
        const uint32_t write_size = (remain > combine_size) ? combine_size : remain;

        RETURN_IF_ERROR(ProgramCombineData (start_addr, p_data, write_size));

        start_addr += write_size;
        p_data += write_size;
        remain -= write_size;
    }

    return (status == STATUS_OK) ? cnt : STATUS_ERROR;
}

static int32_t ReadData(uint32_t addr, void* data, uint32_t cnt)
{
	uint32_t i;

	uint8_t *p8_r = (uint8_t *)(addr);
	uint8_t *p8_data = (uint8_t *)data;

	for (i = 0; i < cnt; i++) {
		*p8_data = *p8_r;
		p8_data++;
		p8_r++;
	}

	return STATUS_OK;
}

/**********************************************************************************************************************
 * End of private functions
 *********************************************************************************************************************/
