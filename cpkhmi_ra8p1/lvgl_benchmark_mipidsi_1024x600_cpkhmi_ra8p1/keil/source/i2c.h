#ifndef __I2C_H
#define __I2C_H

#include <stdint.h>
#include "hal_data.h"

#ifndef I2C_EN_COMMS_I2C
#define I2C_EN_COMMS_I2C		0
#endif

#ifndef I2C_EN_IIC_B_MASTER
#define I2C_EN_IIC_B_MASTER		0
#endif

#ifndef I2C_EN_IIC_MASTER
#define I2C_EN_IIC_MASTER		1
#endif

#ifndef I2C_EN_SCI_B_I2C
#define I2C_EN_SCI_B_I2C		0
#endif

#ifndef I2C_EN_IIC_B_SLAVE
#define I2C_EN_IIC_B_SLAVE		0
#endif

#ifndef I2C_EN_IIC_SLAVE
#define I2C_EN_IIC_SLAVE		0
#endif

#ifndef I2C_DEFAULT_TIMEOUT
#define I2C_DEFAULT_TIMEOUT		1000
#endif

#ifndef I2C_SEND_CACHE_SIZE
#define I2C_SEND_CACHE_SIZE		256
#endif

#if I2C_EN_COMMS_I2C
#if (I2C_EN_IIC_B_MASTER == 0) && (I2C_EN_IIC_MASTER == 0) && (I2C_EN_SCI_B_I2C == 0)
#error "Config Error: when I2C_EN_COMMS_I2C enable, must enable one of master"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	I2C_MODEL_UNKNOW,
	I2C_MODEL_COMMS_I2C,
	I2C_MODEL_IIC_B_MASTER,
	I2C_MODEL_IIC_MASTER,
	I2C_MODEL_SCI_B_I2C,
	I2C_MODEL_IIC_B_SLAVE,
	I2C_MODEL_IIC_SLAVE,
	I2C_MODEL_SOFT,
} I2C_ModelEnum;

typedef enum {
	I2C_OP_OPERATION,
	I2C_OP_RX_DONE,
	I2C_OP_TX_DONE,
	I2C_OP_ALL,
} I2C_OperationEnum;

typedef struct {
	const void *inst;
	I2C_ModelEnum model;
	uint32_t pin_scl;
	uint32_t pin_sda;
#if I2C_EN_COMMS_I2C
	I2C_ModelEnum lower_model;
#endif
} I2C_ConfigType;

typedef struct {
	const void *inst;
	I2C_ModelEnum model;
	uint32_t pin_scl;
	uint32_t pin_sda;
#if I2C_EN_COMMS_I2C
	I2C_ModelEnum lower_model;
	i2c_master_cfg_t runtime_cfg;
#endif

	union {
		uint8_t byte;
		struct {
			uint8_t abort : 1;
			uint8_t tx_done : 1;
			uint8_t tx_ing : 1;
			uint8_t rx_done : 1;
			uint8_t rx_ing : 1;
			uint8_t op_done : 1; /* Only use in I2C_MODEL_COMMS_I2C */
			uint8_t : 2;
		} status;
	};
} I2C_DeviceType;

typedef struct {
	uint32_t slave;
	uint16_t reg_addr;
	uint8_t addr_width;
	uint8_t *val;
	uint8_t val_width;
} I2C_ReadRegParamType;

typedef struct {
	uint32_t slave;
	uint8_t *wdata;
	uint16_t wlen;
	uint8_t *rdata;
	uint16_t rlen;
	uint32_t timeout_ms;
} I2C_WriteReadParamType;

typedef struct {
	uint32_t slave;
	uint16_t reg_addr;
	uint8_t addr_width;
	uint16_t val;
	uint8_t val_width;
} I2C_WriteRegParamType;

typedef struct {
	uint32_t slave;
	uint16_t mem_addr;
	uint8_t mem_width;
	uint8_t *data;
	uint16_t length;
} I2C_WRMemParamType;

uint32_t I2C_Init(I2C_DeviceType *i2c, I2C_ConfigType *cfg);
uint32_t I2C_ReadMem(I2C_DeviceType *i2c, I2C_WRMemParamType *param);
uint32_t I2C_ReadReg(I2C_DeviceType *i2c, I2C_ReadRegParamType *param);
uint32_t I2C_WaitOperation(I2C_DeviceType *i2c, I2C_OperationEnum op, uint32_t timeout_ms);
uint32_t I2C_Write(I2C_DeviceType *i2c, I2C_WRMemParamType *param);
uint32_t I2C_WriteMem(I2C_DeviceType *i2c, I2C_WRMemParamType *param);
uint32_t I2C_WriteRead(I2C_DeviceType *i2c, I2C_WriteReadParamType *param);
uint32_t I2C_WriteReg(I2C_DeviceType *i2c, I2C_WriteRegParamType *param);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __I2C_H */
