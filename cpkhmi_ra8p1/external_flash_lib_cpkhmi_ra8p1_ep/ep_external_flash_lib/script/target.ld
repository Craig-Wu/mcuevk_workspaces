MEMORY
{
    INFO (r) : ORIGIN = 0x00000000, LENGTH = 0x1000
    SRAM (rwx) : ORIGIN = 0x00100000, LENGTH = 220K
}

SECTIONS
{
    loader_info 0x00000000 : {
        KEEP(*(loader_info))
    } > INFO

    ep_initialize 0x00100000 : {
        KEEP(*(ep_initialize))
    } > SRAM

    ep_erase_sector 0x00100040 : {
        KEEP(*(ep_erase_sector))
    } > SRAM

    ep_erase_chip 0x00100080 : {
        KEEP(*(ep_erase_chip))
    } > SRAM

    ep_program_data 0x001000C0 : {
        KEEP(*(ep_program_data))
    } > SRAM

    ep_uninitialize 0x00100100 : {
        KEEP(*(ep_uninitialize))
    } > SRAM

    ep_read_data 0x00100140 : {
        KEEP(*(ep_read_data))
    } > SRAM

    ep_calc_crc 0x00100180 : {
        KEEP(*(ep_calc_crc))
    } > SRAM

    .text 0x00100200: {
        KEEP(*(.text))
    } > SRAM

    .data : {
        *(.data)
    } > SRAM

    .bss : {
        *(.bss)
    } > SRAM

    .ARM.attributes : {
        *(.ARM.attributes)
    } > SRAM
}
