/*================================== INCLUDES =====================================================*/

#include <inttypes.h>
#include "i2c.h"

/*================================== MACROS =======================================================*/

#define CACHE_SIZE	(I2C_SEND_CACHE_SIZE + 2)

#ifndef __I2C_DEBUG
#define __I2C_DEBUG 1
#endif

#define IS_INVALID_POINT(p)			if (p == NULL) { return FSP_ERR_INVALID_POINTER; }

#if __I2C_DEBUG
#include "utils/log.h"
#define I2C_LOGD(msg, ...)				LOG_D(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGI(msg, ...)				LOG_I(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGW(msg, ...)				LOG_W(__FUNCTION__, msg, ##__VA_ARGS__)
#define I2C_LOGE(msg, ...)				LOG_E(__FUNCTION__, msg, ##__VA_ARGS__)
#define FSP_ERR_CHECK(v, p, msg, ...)	if ( v != 0 ) { if (p != NULL) { p->p_api->close(p->p_ctrl); LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; } }
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { LOG_E(__FUNCTION__, msg, ##__VA_ARGS__); return v; }
#else
#define I2C_LOGD(msg, ...)
#define I2C_LOGI(msg, ...)
#define I2C_LOGW(msg, ...)
#define I2C_LOGE(msg, ...)
#define UNLIKE_RETURN(v, t, msg, ...)   if (v != t) { return v; }
#endif

/*================================== TYPES ========================================================*/
/*================================== GLOBAL VARIABLES =============================================*/
/*================================== LOCAL VARIABLES ==============================================*/

static uint8_t s_tx_cache[CACHE_SIZE];

/*================================== Private Functions Prototypes =================================*/

#if I2C_EN_COMMS_I2C
static void commsI2CCallback(rm_comms_callback_args_t *p_args);
#endif

#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
static void hardI2CCallback(i2c_master_callback_args_t *p_args);
#endif

/*================================== Public Functions =============================================*/

uint32_t I2C_Init(I2C_DeviceType *i2c, I2C_ConfigType *cfg)
{
	uint32_t err = FSP_ERR_UNSUPPORTED;

#if I2C_EN_COMMS_I2C
	const rm_comms_instance_t *comms_inst = NULL;
	rm_comms_i2c_instance_ctrl_t *comms_ctrl = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
	const i2c_master_cfg_t *comms_lower_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(cfg);
	IS_INVALID_POINT(cfg->inst);

	i2c->inst = cfg->inst;
	i2c->model = cfg->model;
	i2c->pin_scl = cfg->pin_scl;
	i2c->pin_sda = cfg->pin_sda;
	i2c->byte = 0;

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		comms_lower_cfg = (const i2c_master_cfg_t *)comms_cfg->p_lower_level_cfg;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		err = master_inst->p_api->open(master_inst->p_ctrl, master_inst->p_cfg);
		FSP_ERR_CHECK(err, master_inst, "I2C Master open failed: %" PRIu32, err);
		err = comms_inst->p_api->open(comms_inst->p_ctrl, comms_inst->p_cfg);
		if (err) {
			master_inst->p_api->close(master_inst->p_ctrl);
			comms_inst->p_api->close(comms_inst->p_ctrl);
			I2C_LOGE("I2C Master open failed: %" PRIu32, err);
		}
		comms_ctrl = (rm_comms_i2c_instance_ctrl_t *)comms_inst->p_ctrl;
		__disable_irq();
		comms_ctrl->p_callback = commsI2CCallback;
		comms_ctrl->p_context = i2c;
		memcpy(&i2c->runtime_cfg, comms_lower_cfg, sizeof(i2c_master_instance_t));
		comms_ctrl->p_lower_level_cfg = &i2c->runtime_cfg;
		__enable_irq();
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

		err = master_inst->p_api->open(master_inst->p_ctrl, master_inst->p_cfg);
		FSP_ERR_CHECK(err, master_inst, "I2C Master open failed: %" PRIu32, err);
		__disable_irq();
		master_inst->p_api->callbackSet(master_inst->p_ctrl, hardI2CCallback, i2c, NULL);
		__enable_irq();
	}
#endif

	i2c->status.op_done = 1;
	i2c->status.rx_done = 1;
	i2c->status.tx_done = 1;

	return err;
}

uint32_t I2C_ReadMem(I2C_DeviceType *i2c, I2C_WRMemParamType *param)
{
	uint8_t wlen;
	uint8_t wcache[2];
	uint32_t err = FSP_SUCCESS;

#if I2C_EN_COMMS_I2C
	rm_comms_write_read_params_t comms_wr_param;

	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

	(void)wlen;

	err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
	UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);

	if (param->mem_width == 8) {
		wlen = 1;
		wcache[0] = param->mem_addr & 0xFF;
	}
	else if (param->mem_width == 16) {
		wlen = 2;
		wcache[0] = (param->mem_addr >> 8) & 0xFF;
		wcache[1] = param->mem_addr & 0xFF;
	}
	else {
		I2C_LOGE("param->mem_width only support 8 or 16");
		return FSP_ERR_UNSUPPORTED;
	}

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}

		if (param->length < 256) {
			memcpy(s_tx_cache, wcache, wlen);
			comms_wr_param.p_src = s_tx_cache;
			comms_wr_param.src_bytes = wlen;
			comms_wr_param.p_dest = param->data;
			comms_wr_param.dest_bytes = (uint8_t)param->length;

			i2c->status.op_done = 0;
			err = RM_COMMS_I2C_WriteRead(comms_inst->p_ctrl, comms_wr_param);
			UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_WriteRead() failed: %" PRIu32, err);
			return err;
		}
		else {
			goto LOWER_I2C_SEND;
		}
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

#if I2C_EN_COMMS_I2C
LOWER_I2C_SEND:
#endif
		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, wcache, wlen, true);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
		err = I2C_WaitOperation(i2c, I2C_OP_TX_DONE, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait TX_DONE failed: %" PRIu32, err);
		i2c->byte = 0;
		err = master_inst->p_api->read(master_inst->p_ctrl, param->data, param->length, false);
		UNLIKE_RETURN(err, 0, "I2C read failed: %" PRIu32, err);
		i2c->status.rx_ing = 1;
	}
#endif

	return err;
}

uint32_t I2C_ReadReg(I2C_DeviceType *i2c, I2C_ReadRegParamType *param)
{
	uint8_t rlen;
	uint8_t wlen;
	uint8_t wcache[2];
	uint32_t err = FSP_SUCCESS;
#if I2C_EN_COMMS_I2C
	rm_comms_write_read_params_t comms_wr_param;

	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

	(void)rlen;
	(void)wlen;

	err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
	UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);

	if (param->addr_width == 8) {
		wlen = 1;
		wcache[0] = param->reg_addr & 0xFF;
	}
	else if (param->addr_width == 16) {
		wlen = 2;
		wcache[0] = (param->reg_addr >> 8) & 0xFF;
		wcache[1] = param->reg_addr & 0xFF;
	}
	else {
		I2C_LOGE("param->addr_width only support 8 or 16");
		return FSP_ERR_UNSUPPORTED;
	}

	if (param->val_width == 8) {
		rlen = 1;
	}
	else if (param->val_width == 16) {
		rlen = 2;
	}
	else {
		I2C_LOGE("param->val_width only support 8 or 16");
		return FSP_ERR_UNSUPPORTED;
	}

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}
		memcpy(s_tx_cache, wcache, wlen);
		comms_wr_param.p_src = s_tx_cache;
		comms_wr_param.src_bytes = wlen;
		comms_wr_param.p_dest = param->val;
		comms_wr_param.dest_bytes = rlen;

		i2c->status.op_done = 0;
		err = RM_COMMS_I2C_WriteRead(comms_inst->p_ctrl, comms_wr_param);
		UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_WriteRead() failed: %" PRIu32, err);
		return err;
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, wcache, wlen, true);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
		err = I2C_WaitOperation(i2c, I2C_OP_TX_DONE, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait TX_DONE failed: %" PRIu32, err);
		i2c->byte = 0;
		err = master_inst->p_api->read(master_inst->p_ctrl, param->val, rlen, false);
		UNLIKE_RETURN(err, 0, "I2C read failed: %" PRIu32, err);
		i2c->status.rx_ing = 1;
	}
#endif

	return err;
}

uint32_t I2C_WaitOperation(I2C_DeviceType *i2c, I2C_OperationEnum op, uint32_t timeout_ms)
{
	uint32_t err = FSP_ERR_TIMEOUT;

	IS_INVALID_POINT(i2c);

#if I2C_EN_COMMS_I2C
	if ((i2c->model == I2C_MODEL_COMMS_I2C) && (op == I2C_OP_ALL)) {
		op = I2C_OP_OPERATION;
	}
#endif

	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		if ((i2c->status.rx_ing == 0) && (i2c->status.tx_ing == 0)) {
			return FSP_SUCCESS;
		}
	}
	if (op == I2C_OP_ALL) {
		if (i2c->model == I2C_MODEL_COMMS_I2C) {
			op = I2C_OP_OPERATION;
		}
		if (i2c->status.rx_ing == 0) {
			op = I2C_OP_TX_DONE;
		}
		if (i2c->status.tx_ing == 0) {
			op = I2C_OP_RX_DONE;
		}
 	}

	switch (op) {
	case I2C_OP_OPERATION:
		do {
			if (i2c->status.op_done) {
				err = FSP_SUCCESS;
				break;
			}
			if (i2c->status.abort) {
				err = FSP_ERR_ABORTED;
				break;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			timeout_ms--;
		} while (timeout_ms);
		break;
	case I2C_OP_RX_DONE:
		do {
			if (i2c->status.rx_done) {
				err = FSP_SUCCESS;
				break;
			}
			if (i2c->status.abort) {
				err = FSP_ERR_ABORTED;
				break;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			timeout_ms--;
		} while (timeout_ms);
		break;
	case I2C_OP_TX_DONE:
		do {
			if (i2c->status.tx_done) {
				err = FSP_SUCCESS;
				break;
			}
			if (i2c->status.abort) {
				err = FSP_ERR_ABORTED;
				break;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			timeout_ms--;
		} while (timeout_ms);
		break;
	case I2C_OP_ALL:
		do {
			if ((i2c->status.tx_done) && (i2c->status.rx_done)) {
				err = FSP_SUCCESS;
				break;
			}
			if (i2c->status.abort) {
				err = FSP_ERR_ABORTED;
				break;
			}
			R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
			timeout_ms--;
		} while (timeout_ms);
		break;
	default:
		err = FSP_ERR_UNSUPPORTED;
		break;
	}

	return err;
}

uint32_t I2C_Write(I2C_DeviceType *i2c, I2C_WRMemParamType *param)
{
	uint32_t err = FSP_SUCCESS;
#if I2C_EN_COMMS_I2C
	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}

		err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);
		i2c->status.op_done = 0;
		err = RM_COMMS_I2C_Write(comms_inst->p_ctrl, param->data, param->length);
		UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_Write() failed: %" PRIu32, err);
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

		err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);
		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, param->data, param->length, false);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
	}
#endif

	return err;
}

uint32_t I2C_WriteMem(I2C_DeviceType *i2c, I2C_WRMemParamType *param)
{
	uint8_t reg_len;

	uint32_t err = FSP_SUCCESS;
#if I2C_EN_COMMS_I2C
	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

	if (param->mem_width == 8) {
		reg_len = 1;
		s_tx_cache[0] = param->mem_addr & 0xFF;
	}
	else if (param->mem_width == 16) {
		reg_len = 2;
		s_tx_cache[0] = (param->mem_addr >> 8) & 0xFF;
		s_tx_cache[1] = param->mem_addr & 0xFF;
	}
	else {
		I2C_LOGE("param->mem_width only support 8 or 16");
		return FSP_ERR_UNSUPPORTED;
	}

	if (param->length > I2C_SEND_CACHE_SIZE) {
		I2C_LOGE("param->length > I2C_SEND_CACHE_SIZE");
		return FSP_ERR_OUT_OF_MEMORY;
	}
	memcpy(&s_tx_cache[reg_len], param->data, param->length);

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}

		err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);
		i2c->status.op_done = 0;
		err = RM_COMMS_I2C_Write(comms_inst->p_ctrl, s_tx_cache, param->length + reg_len);
		UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_Write() failed: %" PRIu32, err);
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

		err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
		UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);
		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, s_tx_cache, param->length + reg_len, false);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
	}
#endif

	return err;
}

uint32_t I2C_WriteRead(I2C_DeviceType *i2c, I2C_WriteReadParamType *param)
{
	uint32_t err = FSP_SUCCESS;
#if I2C_EN_COMMS_I2C
	rm_comms_write_read_params_t comms_wr_param;

	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

	err = I2C_WaitOperation(i2c, I2C_OP_ALL, param->timeout_ms);
	UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}

		if ((param->wlen < 257) && (param->rlen < 257)) {
			comms_wr_param.p_src = param->wdata;
			comms_wr_param.src_bytes = param->wlen;
			comms_wr_param.p_dest = param->rdata;
			comms_wr_param.dest_bytes = (uint8_t)param->rlen;

			i2c->status.op_done = 0;
			err = RM_COMMS_I2C_WriteRead(comms_inst->p_ctrl, comms_wr_param);
			UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_WriteRead() failed: %" PRIu32, err);
			return err;
		}
		else {
			goto LOWER_I2C_SEND;
		}
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

#if I2C_EN_COMMS_I2C
LOWER_I2C_SEND:
#endif
		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, param->wdata, param->wlen, true);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
		err = I2C_WaitOperation(i2c, I2C_OP_TX_DONE, param->timeout_ms);
		UNLIKE_RETURN(err, 0, "Wait TX_DONE failed: %" PRIu32, err);
		i2c->byte = 0;
		err = master_inst->p_api->read(master_inst->p_ctrl, param->rdata, param->rlen, false);
		UNLIKE_RETURN(err, 0, "I2C read failed: %" PRIu32, err);
		i2c->status.rx_ing = 1;
	}
#endif

	return err;
}

uint32_t I2C_WriteReg(I2C_DeviceType *i2c, I2C_WriteRegParamType *param)
{
	uint8_t wlen;

	uint32_t err = FSP_SUCCESS;
#if I2C_EN_COMMS_I2C
	const rm_comms_instance_t *comms_inst = NULL;
	const rm_comms_cfg_t *comms_cfg = NULL;
	const rm_comms_i2c_bus_extended_cfg_t *comms_bus_cfg = NULL;
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	const i2c_master_instance_t *master_inst = NULL;
#endif

	IS_INVALID_POINT(i2c);
	IS_INVALID_POINT(param);

	(void)wlen;

	err = I2C_WaitOperation(i2c, I2C_OP_ALL, I2C_DEFAULT_TIMEOUT);
	UNLIKE_RETURN(err, 0, "Wait previous operation failed: %" PRIu32, err);

	if (param->addr_width == 8) {
		s_tx_cache[0] = param->reg_addr & 0xFF;
		if (param->val_width == 8) {
			s_tx_cache[1] = param->val & 0xFF;
			wlen = 2;
		}
		else if (param->val_width == 16) {
			s_tx_cache[1] = (param->val >> 8) & 0xFF;
			s_tx_cache[2] = param->val & 0xFF;
			wlen = 3;
		}
		else {
			I2C_LOGE("param->val_width only support 8 or 16");
			return FSP_ERR_UNSUPPORTED;
		}
	}
	else if (param->addr_width == 16) {
		s_tx_cache[0] = (param->reg_addr >> 8) & 0xFF;
		s_tx_cache[1] = param->reg_addr & 0xFF;
		if (param->val_width == 8) {
			s_tx_cache[2] = param->val & 0xFF;
			wlen = 3;
		}
		else if (param->val_width == 16) {
			s_tx_cache[2] = (param->val >> 8) & 0xFF;
			s_tx_cache[3] = param->val & 0xFF;
			wlen = 4;
		}
		else {
			I2C_LOGE("param->val_width only support 8 or 16");
			return FSP_ERR_UNSUPPORTED;
		}
	}
	else {
		I2C_LOGE("param->addr_width only support 8 or 16");
		return FSP_ERR_UNSUPPORTED;
	}

#if I2C_EN_COMMS_I2C
	if (i2c->model == I2C_MODEL_COMMS_I2C) {
		comms_inst = (const rm_comms_instance_t *)i2c->inst;
		comms_cfg = (const rm_comms_cfg_t *)comms_inst->p_cfg;
		comms_bus_cfg = (rm_comms_i2c_bus_extended_cfg_t *)comms_cfg->p_extend;
		master_inst = (const i2c_master_instance_t *)comms_bus_cfg->p_driver_instance;

		if (i2c->runtime_cfg.slave != param->slave) {
			i2c->runtime_cfg.slave = param->slave;
		}

		i2c->byte = 0;
		err = RM_COMMS_I2C_Write(comms_inst->p_ctrl, s_tx_cache, wlen);
		UNLIKE_RETURN(err, 0, "RM_COMMS_I2C_Write() failed: %" PRIu32, err);
	}
#endif
#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
	if ((i2c->model == I2C_MODEL_IIC_B_MASTER) || (i2c->model == I2C_MODEL_IIC_MASTER) || (i2c->model == I2C_MODEL_SCI_B_I2C)) {
		master_inst = (const i2c_master_instance_t *)i2c->inst;

		master_inst->p_api->slaveAddressSet(master_inst->p_ctrl, param->slave, I2C_MASTER_ADDR_MODE_7BIT);
		i2c->byte = 0;
		err = master_inst->p_api->write(master_inst->p_ctrl, s_tx_cache, wlen, false);
		UNLIKE_RETURN(err, 0, "I2C write failed: %" PRIu32, err);
		i2c->status.tx_ing = 1;
	}
#endif

	return err;
}

/*================================== Private Functions ============================================*/

#if I2C_EN_COMMS_I2C
static void commsI2CCallback(rm_comms_callback_args_t *p_args)
{
	I2C_DeviceType *i2c = (I2C_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case RM_COMMS_EVENT_OPERATION_COMPLETE:
		i2c->status.op_done = 1;
		break;
	case RM_COMMS_EVENT_ERROR:
		i2c->status.abort = 1;
		break;
	default:
		break;
	}
}
#endif

#if I2C_EN_IIC_B_MASTER || I2C_EN_IIC_MASTER || I2C_EN_SCI_B_I2C
static void hardI2CCallback(i2c_master_callback_args_t *p_args)
{
	I2C_DeviceType *i2c = (I2C_DeviceType *)p_args->p_context;

	switch (p_args->event) {
	case I2C_MASTER_EVENT_ABORTED:
		i2c->status.abort = 1;
		break;
	case I2C_MASTER_EVENT_RX_COMPLETE:
		i2c->status.rx_done = 1;
		i2c->status.rx_ing = 0;
		break;
	case I2C_MASTER_EVENT_TX_COMPLETE:
		i2c->status.tx_done = 1;
		i2c->status.tx_ing = 0;
		break;
	default:
		break;
	}
}
#endif
