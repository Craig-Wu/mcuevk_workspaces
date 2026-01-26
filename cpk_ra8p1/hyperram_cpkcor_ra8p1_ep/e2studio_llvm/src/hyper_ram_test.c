
#include <string.h>
#include <inttypes.h>
#include "hal_data.h"
#include "common_utils.h"
#include "hyper_ram_test.h"

#define ram_cfg     g_ospi1_cfg
#define ram_ctrl    g_ospi1_ctrl

#define OSPI_OM_RESET               BSP_IO_PORT_12_PIN_07
#define HYPER_RAM_RESET_DELAY()     R_BSP_SoftwareDelay(10UL, BSP_DELAY_UNITS_MICROSECONDS)

#define RAM_WORD_SIZE               (2UL)
#define RAM_WORDS_PER_BLOCK         (8UL)
#define RAM_BLOCK_SIZE              (RAM_WORD_SIZE * RAM_WORDS_PER_BLOCK)
#define RAM_BLOCKS_PER_PAGE         (64UL)
#define RAM_PAGE_SIZE               (RAM_BLOCK_SIZE * RAM_BLOCKS_PER_PAGE)
#define RAM_PAGE_COUNT              (32768UL)
#define RAM_DENSITY                 (RAM_PAGE_SIZE * RAM_PAGE_COUNT)

#define CHUNK_SIZE                  (64UL)


#define HYPER_RAM_CR_0_ADDRESS 0x01000000
#define HYPER_RAM_CR_1_ADDRESS 0x01000001

#define HYPER_RAM_RANGE_SIZE        (8388608)     //4 bytes alignment
#define HYPER_RAM_START_ADDRESS     (0x78000000)
#define PSRAM_SIZE                  ((uint32_t)0x2000000UL)     // 32MBytes
#define PSRAM_DATA_WIDTH            8
#define PSRAM_BANK_ADDR             ((uint32_t)0x78000000UL)    // XSPI1 CS1


#if !defined(LENGTH_OF)
 #define LENGTH_OF(arr)             (sizeof((arr)) / sizeof((arr)[0]))
#endif

#define TRY(fn)                     _TRY_BREAK(fn, false)
#define TRY_BREAK(fn)               _TRY_BREAK(fn, true)
#define _TRY_BREAK(fn,break_on_err)         \
    do                                      \
    {                                       \
        volatile fsp_err_t _err;            \
        if (FSP_SUCCESS != (_err = (fn)))   \
        {                                   \
            if (break_on_err) __BKPT(0); \
            return _err;                    \
        }                                   \
    } while (0)

#define CATCH_BREAK(fn)                     \
    do                                      \
    {                                       \
        volatile fsp_err_t _err;            \
        if (FSP_SUCCESS != (_err = (fn)))   \
        {                                   \
            __BKPT(0);                   \
        }                                   \
    } while (0)


void DWT_init();
uint32_t DWT_get_count();
void DWT_clean_count();
uint32_t DWT_count_to_us(uint32_t delta_count);

#define DWT_DEM *(uint32_t*)0xE000EDFC

void DWT_init()
{
    DWT->CTRL = 0;
    DWT_DEM |= 1<<24;
    DWT->CYCCNT = 0;
    DWT->CTRL |= 1<<0;
}


uint32_t DWT_get_count()
{
    return DWT->CYCCNT;
}

void DWT_clean_count()
{
    DWT->CYCCNT = 0;
}

uint32_t DWT_count_to_us(uint32_t delta_count)
{
    return delta_count/1000;
}


volatile uint32_t DWT_pre_count=0, DWT_post_count=0, time_hyperRAM_access=0;
volatile uint32_t DWT_delta=0;

typedef enum
{
    HYPER_RAM_CMD_READ           = 0x80,
    HYPER_RAM_CMD_WRITE          = 0x00,
    HYPER_RAM_CMD_MEMORY_SPACE   = 0x00,
    HYPER_RAM_CMD_REGISTER_SPACE = 0x40,
    HYPER_RAM_CMD_WRAP           = 0x00,
    HYPER_RAM_CMD_LINEAR         = 0x20,
} hyper_ram_cmd_t;

ospi_b_xspi_command_set_t g_hyper_ram_commands[] =
{
 {
  .protocol = SPI_FLASH_PROTOCOL_8D_8D_8D,
  .frame_format = OSPI_B_FRAME_FORMAT_XSPI_PROFILE_2,
  .latency_mode = OSPI_B_LATENCY_MODE_FIXED,
  .command_bytes = OSPI_B_COMMAND_BYTES_1,
  .address_bytes = SPI_FLASH_ADDRESS_BYTES_4,

  .read_command = 0xA0,
  .read_dummy_cycles = 13,
  .program_command = 0x20,
  .program_dummy_cycles = 13,

  .address_msb_mask = 0xF0,
  .status_needs_address = false,

  .p_erase_commands = NULL,
 }
};

//volatile uint32_t dTCM_read[16000] BSP_PLACE_IN_SECTION("__dtcm_from_data_flash$$");
volatile uint8_t dTCM_read[16000] BSP_PLACE_IN_SECTION("__dtcm_noinit$$");
volatile uint32_t sRAM_read[16000] BSP_PLACE_IN_SECTION("__ram_noinit_nocache$$");
/** Start of the OSPI 1 region from the linker script. */
// static uint8_t * p_ospi_region = (uint8_t *) gp_ospi_b_region_cs1;
static uint8_t * p_ospi_region = (uint8_t *)((void *)0x78000000);
uint8_t rtt_data[100] = {0};
static uint8_t linear_data[4 * CHUNK_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,

    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,

    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,

    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

static uint8_t random_data[4 * CHUNK_SIZE] = {
    0x82, 0x3C, 0xE0, 0x71, 0x11, 0x3B, 0x56, 0x36, 0x5B, 0x98, 0xF2, 0xE9, 0x04, 0x71, 0x2D, 0xB5,
    0x46, 0xC4, 0x0E, 0xEA, 0x91, 0x7B, 0xA2, 0x11, 0x42, 0xA7, 0x97, 0x0B, 0x6B, 0xA9, 0xA3, 0x02,
    0x45, 0x4C, 0x67, 0x1F, 0x10, 0xD7, 0x55, 0x9F, 0x6E, 0xB2, 0x56, 0x22, 0xD3, 0x4E, 0x1C, 0x2E,
    0x59, 0x19, 0xA9, 0x9F, 0x60, 0x6D, 0x9D, 0x2D, 0x8D, 0x02, 0x93, 0xE7, 0x1D, 0xF8, 0xA2, 0x7B,

    0x2C, 0x40, 0x9C, 0x9A, 0x8F, 0x83, 0x64, 0x6A, 0x15, 0xD9, 0x7C, 0xE6, 0x38, 0x6F, 0x2F, 0xD8,
    0x3D, 0x73, 0x19, 0x56, 0x98, 0xCD, 0x34, 0x3A, 0xD5, 0xFF, 0x69, 0x03, 0xBC, 0x4E, 0xFE, 0x8C,
    0xCE, 0x6C, 0xF1, 0x36, 0xCE, 0xE5, 0x99, 0xA8, 0x0E, 0x52, 0x90, 0x1F, 0x2A, 0xB7, 0xAA, 0x58,
    0x8F, 0x2D, 0x48, 0x4E, 0x70, 0x2E, 0xEE, 0x1E, 0xC9, 0xFC, 0x2A, 0x9B, 0xFF, 0x1B, 0x46, 0x03,

    0x93, 0x57, 0x8F, 0xF1, 0x2B, 0x46, 0x90, 0x33, 0xF6, 0x19, 0xBC, 0xFD, 0xC2, 0xD9, 0x01, 0xED,
    0x1A, 0xF7, 0xD2, 0x92, 0xBD, 0xB7, 0xAF, 0x36, 0x7B, 0x7C, 0x6C, 0xBC, 0x4C, 0x64, 0xF5, 0x80,
    0x85, 0xBE, 0x8E, 0x7B, 0x8C, 0x06, 0x4B, 0x1F, 0xA7, 0xCD, 0x2F, 0x11, 0xAD, 0xB5, 0x03, 0xE2,
    0x78, 0x12, 0x71, 0x1C, 0x12, 0x10, 0x88, 0xD3, 0xC1, 0xFA, 0xC6, 0x70, 0x9A, 0xCA, 0x63, 0xAE,

    0xA2, 0x34, 0xBE, 0xB8, 0xB5, 0x36, 0x3C, 0xB2, 0xDD, 0x10, 0x59, 0x11, 0xE5, 0x36, 0x85, 0xF8,
    0xC9, 0xAD, 0x95, 0x45, 0xBD, 0xED, 0xF9, 0x20, 0x51, 0xF8, 0xD3, 0x13, 0x9D, 0x75, 0xEB, 0x7F,
    0xD4, 0x4F, 0x5A, 0x05, 0x49, 0x69, 0xA3, 0x9F, 0x1D, 0x6B, 0xBA, 0xAE, 0xA0, 0x20, 0xE1, 0x8A,
    0x2E, 0x15, 0x9B, 0xD1, 0xE3, 0x71, 0x9B, 0x5B, 0xC3, 0x12, 0x90, 0x07, 0x34, 0xCB, 0xF1, 0xBC,
};

static uint8_t buffer[CHUNK_SIZE];

#if 0
/**
 * Copies configuration settings from e2 studio into structs that can be modified during testing.
 * Additionally sets HyperRAM specific values.
 */
static void setup_structs(void)
{
    /* Copy the structures generated from e2 studio and adjust as necessary. */
    memcpy(&ram_cfg, g_ospi0.p_cfg, sizeof(ram_cfg));
    memcpy(&ram_ext_cfg, ram_cfg.p_extend, sizeof(ram_ext_cfg));
    memcpy(&ram_cmd_set, ram_ext_cfg.p_xspi_command_set, sizeof(ram_cmd_set));

    ram_cfg.p_extend = &ram_ext_cfg;

    ram_ext_cfg.p_xspi_command_set_list = &ram_cmd_set;
    ram_ext_cfg.xspi_command_set_list_length = 1;

    /* HyperRAM uses the same dummy cycles for programming as it does for reads.
     * This field hasn't been used up to this point. */
    ram_ext_cfg.program_dummy_cycles = ram_ext_cfg.read_dummy_cycles;
    ram_cmd_set.program_dummy_cycles = ram_cmd_set.read_dummy_cycles;

    /* No erasures for HyperRAM. */
    ram_cfg.erase_command_list_length = 0;

    /* This is an override for testing. It isn't in the proper driver. */
    ram_cmd_set.command_bytes = OSPI_B_COMMAND_BYTES_EXTENDED;
}
#endif

static void hyper_ram_test_init(void)
{
    //setup_structs();

    /* Change OM_RESET back to normal IO mode. */
    R_IOPORT_PinCfg(&g_ioport_ctrl,
                    OSPI_OM_RESET,
                    IOPORT_CFG_PORT_DIRECTION_OUTPUT
                        | IOPORT_CFG_DRIVE_HIGH
                        | IOPORT_CFG_PORT_DIRECTION_OUTPUT 
                        | IOPORT_CFG_PORT_OUTPUT_HIGH);

    /* Pin reset the OctaFlash */
    R_BSP_PinWrite(OSPI_OM_RESET, BSP_IO_LEVEL_LOW);
    HYPER_RAM_RESET_DELAY();
    R_BSP_PinWrite(OSPI_OM_RESET, BSP_IO_LEVEL_HIGH);
    HYPER_RAM_RESET_DELAY();
}

static void test_write_linear(uint32_t offset)
{
    for (size_t i = 0; i < LENGTH_OF(linear_data); i += CHUNK_SIZE)
    {
        CATCH_BREAK(R_OSPI_B_Write(&ram_ctrl, &linear_data[i], &p_ospi_region[offset + i], CHUNK_SIZE));
    }

    for (size_t i = 0; i < LENGTH_OF(linear_data); i += sizeof(buffer))
    {
        memcpy(buffer, &p_ospi_region[offset + i], sizeof(buffer));
        if (memcmp(&linear_data[i], buffer, sizeof(buffer)))
        {
            __BKPT(2);
        }
    }
}

static void test_write_random(uint32_t offset)
{
    for (size_t i = 0; i < LENGTH_OF(random_data); i += CHUNK_SIZE)
    {
        CATCH_BREAK(R_OSPI_B_Write(&ram_ctrl, &random_data[i], &p_ospi_region[offset + i], CHUNK_SIZE));
    }

    if (memcmp(random_data, &p_ospi_region[offset], sizeof(random_data)))
    {
        __BKPT(2);
    }
}

uint16_t swap16(uint16_t value)
{
    uint16_t ret;
    ret  = value << 8;
    ret |= value >> 8;
    return ret;
}

static fsp_err_t hyper_ram_config_get(uint32_t address, uint16_t * const p_value_out)
{
    spi_flash_direct_transfer_t xfer = {
        .address = address,
        .address_length = 4,
        .command = 0xC000,
        .command_length = 2,
        .data_length = 2,
        .dummy_cycles = 14,
    };

    TRY_BREAK(R_OSPI_B_DirectTransfer(&ram_ctrl, &xfer, SPI_FLASH_DIRECT_TRANSFER_DIR_READ));

    *p_value_out = (uint16_t) xfer.data;

    return FSP_SUCCESS;
}

static fsp_err_t hyper_ram_config_set(uint32_t address, uint16_t value)
{
    spi_flash_direct_transfer_t xfer = {
        .address = address,
        .address_length = 4,
        .command = 0x6000,
        .command_length = 2,
        .data = (uint32_t)value,
        .data_length = 2,
        .dummy_cycles = 0,
    };

    TRY_BREAK(R_OSPI_B_DirectTransfer(&ram_ctrl, &xfer, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE));

    return FSP_SUCCESS;
}

/**
 * Entry point for HyperRAM testing.
 */
void hyper_ram_test(void)
{
    hyper_ram_test_init();

    /* Open the interface and immediately transition to 8D-8D-8D mode.
     * Currently, the driver doesn't support setting command lengths in the startup mode. */
    R_OSPI_B_Open((spi_flash_ctrl_t *)&ram_ctrl, &ram_cfg);
    R_OSPI_B_SpiProtocolSet(&ram_ctrl, SPI_FLASH_PROTOCOL_8D_8D_8D);

    R_XSPI1 -> LIOCFGCS_b[1].WRMSKMD  = 1;

    DWT_init();
    uint16_t cfg_reg = 0;
    hyper_ram_config_get(HYPER_RAM_CR_0_ADDRESS, &cfg_reg);

    uint16_t cfg_reg1 = 0;
    hyper_ram_config_get(HYPER_RAM_CR_1_ADDRESS, &cfg_reg1);

    uint16_t value1 = 0xff81;
    hyper_ram_config_set(HYPER_RAM_CR_1_ADDRESS, swap16(value1));

    cfg_reg1 = 0;
    hyper_ram_config_get(HYPER_RAM_CR_1_ADDRESS, &cfg_reg1);

    APP_PRINT("##############################################################\r\n");
    APP_PRINT("##############################################################\r\n");
    APP_PRINT("##############################################################\r\n");
    APP_PRINT("**********Start to Test the HyperRAM! **********\r\n");


// test the HyperRAM full range scope;

// clear the hyperRAM
    static uint32_t * test = (uint32_t *)((void *)HYPER_RAM_START_ADDRESS);
    for (uint32_t i = 0; i < HYPER_RAM_RANGE_SIZE; i++)
    {
        *test++ = 0;
    }

    test = (uint32_t *)((void *)HYPER_RAM_START_ADDRESS);

    DWT_clean_count();
    DWT_pre_count = DWT_get_count();
// write all the hyperRAM test

    for (uint32_t i = 0; i < PSRAM_SIZE / 4; i++)
    {
#if PSRAM_DATA_WIDTH == 8
        *(__IO uint8_t *)(PSRAM_BANK_ADDR + i) = (uint8_t)0xA5;
#elif PSRAM_DATA_WIDTH == 16
        *(__IO uint16_t *)(PSRAM_BANK_ADDR + i * 4) = (uint16_t)0x5555;
#else
        *(__IO uint32_t *)(PSRAM_BANK_ADDR + i * 4) = i ^ (uint32_t)0x5A5AA5A5;
        //*(__IO uint32_t *)(PSRAM_BANK_ADDR + i * 4) = (uint32_t)0x5A5AA5A5;
#endif
    }


    DWT_post_count = DWT_get_count();
    DWT_delta = DWT_post_count - DWT_pre_count;
    if(DWT_delta==0)
    {
        APP_PRINT("DWT count error! \r\n");
    }
    time_hyperRAM_access = DWT_count_to_us(DWT_delta);

    APP_PRINT("HyperRAM write speed:%d MB/s \r\n", (32000000 / time_hyperRAM_access));

// read all the hyperRAM test
    test = (uint32_t *)((void *)HYPER_RAM_START_ADDRESS);
#if PSRAM_DATA_WIDTH == 8
    uint8_t data = 0;
#else
    uint32_t data = 0;
#endif
    DWT_clean_count();
    DWT_pre_count = DWT_get_count();
    static uint32_t j = 0;
    for (j = 0; j < PSRAM_SIZE / 4; j++)
    {
#if PSRAM_DATA_WIDTH == 8
        data = *(__IO uint8_t *)(PSRAM_BANK_ADDR + j);
        if (data != 0xA5)
        {
            //LOG_E("PSRAM test failed!");
            __BKPT(2);
            break;
        }
#elif PSRAM_DATA_WIDTH == 16
        data = *(__IO uint16_t *)(PSRAM_BANK_ADDR + j * 4);
        if (data != 0x5555)
        {
            LOG_E("PSRAM test failed!");
            break;
        }
#else
        data = *(__IO uint32_t *)(PSRAM_BANK_ADDR + j * 4);
        if (data != (j ^ 0x5A5AA5A5))
        //if (data != (0x5A5AA5A5))
        {
            __BKPT(2);
            break;
        }
#endif
    }

    DWT_post_count = DWT_get_count();
    DWT_delta = DWT_post_count - DWT_pre_count;
    if(DWT_delta==0)
    {
        APP_PRINT("DWT count error! \r\n");
    }
    time_hyperRAM_access = DWT_count_to_us(DWT_delta);

    APP_PRINT("HyperRAM read speed:%d MB/s \r\n", (32000000 / time_hyperRAM_access));


    while(1);

    test_write_linear(0);

    test_write_random(RAM_PAGE_SIZE * 10);

    R_OSPI_B_Close((spi_flash_ctrl_t *)&ram_ctrl);
}
