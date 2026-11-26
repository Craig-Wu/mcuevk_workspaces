#include "hal_data.h"
#include "nor_flash.h"
#include "utils/util.h"

#define OSPI_INSTANCE	g_ospi_w25qxx
#define OSPI_CFG		UTIL_CONCAT(OSPI_INSTANCE, _cfg)
#define OSPI_CTRL		UTIL_CONCAT(OSPI_INSTANCE, _ctrl)

#ifndef __NOR_FLASH_DEBUG
#define __NOR_FLASH_DEBUG	0
#endif

#if __NOR_FLASH_DEBUG
#include "utils/log.h"
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#define NF_LOGD(msg, ...)				LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define NF_LOGI(msg, ...)				LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define NF_LOGW(msg, ...)				LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define NF_LOGE(msg, ...)				LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#else
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { return v; }
#define NF_LOGD(msg, ...)
#define NF_LOGI(msg, ...)
#define NF_LOGW(msg, ...)
#define NF_LOGE(msg, ...)
#endif

static void softReset(void);

uint32_t NorFlash_EraseChip(void)
{
	uint32_t err;
	spi_flash_direct_transfer_t cmd;

	err = NorFlash_SetWriteEnable();
	UNLIKE_RETURN(err, 0, "NorFlash_SetWriteEnable() failed");
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0xC7;
	cmd.command_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

	return 0;
}

uint32_t NorFlash_EraseSector(uint32_t sector)
{
	uint32_t err;
	spi_flash_direct_transfer_t cmd;

	spi_flash_protocol_t current = OSPI_CFG.spi_protocol;

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);
	}
	err = NorFlash_SetWriteEnable();
	UNLIKE_RETURN(err, 0, "NorFlash_SetWriteEnable() failed");
	memset(&cmd, 0, sizeof(cmd));
	cmd.address = sector * NORFLASH_SECTOR_SIZE;
	cmd.address_length = 0x04;
	cmd.command = 0x21;
	cmd.command_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		NorFlash_WaitOperation(200);
		R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, current);
	}

	return err;
}

uint32_t NorFlash_GetID(uint8_t *manufacturer_id, uint8_t *device_id, uint64_t *unique_id)
{
	spi_flash_direct_transfer_t cmd;

	spi_flash_protocol_t current = OSPI_CTRL.spi_protocol;

	memset(&cmd, 0, sizeof(cmd));
	if ((manufacturer_id != NULL) || (device_id != NULL)) {
		cmd.command_length = 0x01;
		if (OSPI_CTRL.spi_protocol == SPI_FLASH_PROTOCOL_1S_1S_1S) {
			cmd.address_length = 0x04;
			cmd.command = 0x90;
			cmd.data_length = 0x02;
		}
		else {
			cmd.address = 0xF0;
			cmd.address_length = 0x04;
			cmd.command = 0x94;
			cmd.data_length = 0x04;
			cmd.dummy_cycles = 0x06;
		}
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);

		if (manufacturer_id != NULL) {
			*manufacturer_id = (uint8_t)(cmd.data >> 8);
		}

		if (device_id != NULL) {
			*device_id = cmd.data & 0x00FF;
		}
	}

	if (unique_id != NULL) {
		if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
			R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, SPI_FLASH_PROTOCOL_1S_1S_1S);
			R_BSP_SoftwareDelay(5, BSP_DELAY_UNITS_MICROSECONDS);
		}
		cmd.address_length = 0x04;
		cmd.command = 0x4B;
		cmd.command_length = 0x01;
		cmd.data_length = 0x08;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		*unique_id = cmd.data_u64;
		if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
			R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, current);
		}
	}

	return 0;
}

uint32_t NorFlash_Init(void)
{
	uint8_t manufacturer_id, manufacturer_id_1;
	uint8_t device_id, device_id_1;
	uint64_t unique_id, unique_id_1;
	spi_flash_direct_transfer_t cmd;

	R_OSPI_B_Open(OSPI_INSTANCE.p_ctrl, OSPI_INSTANCE.p_cfg);
	R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);

	softReset();
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x15;
	cmd.command_length = 0x01;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	if ((cmd.data & 0x01) == 0x00) {
		NF_LOGD("Enter 4-byte address mode");
		cmd.command = 0xB7;
		cmd.data_length = 0x00;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
		cmd.command = 0x15;
		cmd.data_length = 0x01;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		if ((cmd.data & 0x01) == 0x00) {
			NF_LOGE("Enter 4-byte address mode failed, Status REG3: 0x%X", cmd.data);
		}
	}
	else {
		NF_LOGD("Already in 4-byte address mode");
	}

	/* Try a faster clock speed */
	NorFlash_GetID(&manufacturer_id, &device_id, &unique_id);
	NF_LOGD("manufacturer_id: 0x%X", manufacturer_id);
	NF_LOGD("device_id: 0x%X", device_id);
	NF_LOGD("unique_id: 0x%llX", unique_id);
	if (manufacturer_id != 0xEF) {
		NF_LOGE("manufacturer_id error: 0x%X", manufacturer_id);
		return FSP_ERR_UNSUPPORTED;
	}

	/* Switch to 1S-4S-4S */
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x35;
	cmd.command_length = 0x01;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	if ((cmd.data & 0x02) == 0) {
		cmd.command = 0x50;
		cmd.data_length = 0x00;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

		cmd.command = 0x31;
		cmd.data |= 0x02;
		cmd.data_length = 0x01;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

		cmd.command = 0x35;
		cmd.data = 0x00;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);

		if ((cmd.data & 0x02) == 0) {
			NF_LOGE("Set QE failed");
		}
		else {
			NF_LOGD("Set QE success");
		}
	}
	else {
		NF_LOGD("QE already enable");
	}

	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
	R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, SPI_FLASH_PROTOCOL_1S_4S_4S);
	NorFlash_GetID(&manufacturer_id_1, &device_id_1, &unique_id_1);
	if ((manufacturer_id != manufacturer_id_1) || (device_id != device_id_1) || (unique_id != unique_id_1)) {
		NF_LOGW("Read ID in 1S-4S-4S doesn't equal to 1S-1S-1S");
		NF_LOGW("manufacturer_id: 0x%X", manufacturer_id_1);
		NF_LOGW("device_id: 0x%X", device_id_1);
		NF_LOGW("unique_id: 0x%llX", unique_id_1);
	}
	NF_LOGD("Enter 1S-4S-4S mode.");

	return 0;
}

uint32_t NorFlash_Program(uint32_t address, void const *data, uint32_t length)
{
	uint32_t i;
	uint32_t err;

	uint8_t *p_flash = (uint8_t *)address;
	uint8_t *p_data = (uint8_t *)data;
	uint32_t repeat = length / 64;
	uint32_t remain = length % 64;
	spi_flash_protocol_t current = OSPI_CTRL.spi_protocol;

	if ((address < NORFLASH_MAP_START_ADDR) || (address > (NORFLASH_MAP_START_ADDR + NORFLASH_SIZE))) {
		NF_LOGE("dest out of range");
		return FSP_ERR_OUT_OF_MEMORY;
	}

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, SPI_FLASH_PROTOCOL_1S_1S_1S);
	}

	for (i = 0; i < repeat; i++) {
		err = NorFlash_SetWriteEnable();
		UNLIKE_RETURN(err, 0, "WriteEnable() failed, err: %u, i: %u", err, i);
		err = R_OSPI_B_Write(OSPI_INSTANCE.p_ctrl, p_data, p_flash, 64);
		UNLIKE_RETURN(err, 0, "R_OSPI_B_Write() failed, err: %u, i: %u", err, i);
		err = NorFlash_WaitOperation(2000);
		UNLIKE_RETURN(err, 0, "WaitOperation() failed, err: %u, i: %u", err, i);
		p_data = &p_data[64];
		p_flash = &p_flash[64];
	}
	if (remain) {
		err = NorFlash_SetWriteEnable();
		UNLIKE_RETURN(err, 0, "WriteEnable() failed, err: %u", err);
		err = R_OSPI_B_Write(OSPI_INSTANCE.p_ctrl, p_data, p_flash, remain);
		UNLIKE_RETURN(err, 0, "R_OSPI_B_Write() failed, err: %u", err);
		err = NorFlash_WaitOperation(2000);
		UNLIKE_RETURN(err, 0, "WaitOperation() failed, err: %u", err);
	}

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		NorFlash_WaitOperation(200);
		R_OSPI_B_SpiProtocolSet(OSPI_INSTANCE.p_ctrl, current);
	}

	return err;
}

uint32_t NorFlash_SetWriteEnable(void)
{
	spi_flash_direct_transfer_t cmd;

	uint32_t timeout = 100;
	spi_flash_protocol_t current = OSPI_CTRL.spi_protocol;

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(&OSPI_CTRL, SPI_FLASH_PROTOCOL_1S_1S_1S);
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x06;
	cmd.command_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	cmd.command = 0x05;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	while (((cmd.data & 0x02) == 0x00) && (timeout)) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		timeout--;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	}

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(&OSPI_CTRL, SPI_FLASH_PROTOCOL_1S_4S_4S);
	}

	if ((cmd.data & 0x02) == 0x00) {
		return FSP_ERR_TIMEOUT;
	}

	return 0;
}

uint32_t NorFlash_WaitOperation(uint32_t timeout)
{
	spi_flash_direct_transfer_t cmd;

	spi_flash_protocol_t current = OSPI_CTRL.spi_protocol;

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(&OSPI_CTRL, SPI_FLASH_PROTOCOL_1S_1S_1S);
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x05;
	cmd.command_length = 0x01;
	cmd.data_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	while ((cmd.data & 0x01) && timeout) {
		R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
		timeout--;
		R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
	}

	if (current == SPI_FLASH_PROTOCOL_1S_4S_4S) {
		R_OSPI_B_SpiProtocolSet(&OSPI_CTRL, SPI_FLASH_PROTOCOL_1S_4S_4S);
	}

	if (cmd.data & 0x01) {
		NF_LOGE("Timeout, last read: 0x%X", cmd.data);
		return FSP_ERR_TIMEOUT;
	}

	return 0;
}

static void softReset(void)
{
	spi_flash_direct_transfer_t cmd;

	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = 0x66;
	cmd.command_length = 0x01;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
	cmd.command = 0x99;
	R_OSPI_B_DirectTransfer(OSPI_INSTANCE.p_ctrl, &cmd, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MICROSECONDS);
}
