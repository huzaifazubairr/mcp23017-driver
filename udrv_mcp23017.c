/*
 * udrv_sensor_model.c
 *
 *  Created on: Sep 16, 2026
 *      Author: hammad
 */
#include "udrv_mcp23017.h"
#include "abs_i2c.h"

#define max_mcp_devices 4

static MCP23017_States* mcp_registry[max_mcp_devices];
static uint8_t mcp_device_count = 0;

/*
 * Change the file of these abs
 * change the nomenclature
 * change the arguments, use same arguments as HAL
 * Error handling with HAL_StatusTypeDef
*/

typedef enum{
	flag_clear = 0,
	flag_rise = 1
}Flag_state;

void udrv_mcp23017_init(MCP23017_States *dev,I2C_HandleTypeDef *hi2c,uint8_t dev_address)
{
	dev -> hi2c = hi2c;
	dev -> int_flag = 0U;
	dev-> exti_pin = 0U;
	dev-> i2c_address = dev_address;

	if(mcp_device_count < max_mcp_devices)
	{
		mcp_registry[mcp_device_count] = dev;
		mcp_device_count++;
	}
}

HAL_StatusTypeDef udrv_mcp23017_set_port_dir(MCP23017_States *dev,MCP_Port port,uint8_t dir_mask){
	HAL_StatusTypeDef status;
	uint8_t reg_address;

	if(port == 0)
	{
		reg_address = IODIRA;
	}
	else if(port == 1)
	{
		reg_address = IODIRB;
	}
	status = i2c_write_reg(dev->hi2c,dev->i2c_address, reg_address,I2C_MEMADD_SIZE_8BIT,&dir_mask,1,100);
	return status;
}

HAL_StatusTypeDef udrv_mcp23017_write_port(MCP23017_States *dev,MCP_Port port, uint8_t value)
{
	HAL_StatusTypeDef status;
	uint8_t reg_address;

	if(port == MCP_PORT_A)
	{
		reg_address = GPIOA_REG;
	}
	else if(port == MCP_PORT_B)
	{
		reg_address = GPIOB_REG;
	}

	status = i2c_write_reg(dev->hi2c,dev->i2c_address, reg_address,I2C_MEMADD_SIZE_8BIT,&value,1,100);

	return status;
}
HAL_StatusTypeDef udrv_mcp23017_read_both_ports(MCP23017_States *dev, uint8_t *data){
	return i2c_read_reg(dev->hi2c,dev->i2c_address, GPIOA_REG ,I2C_MEMADD_SIZE_8BIT,data,2,100);
}

HAL_StatusTypeDef udrv_mcp23017_pullup(MCP23017_States *dev,MCP_Port port,uint8_t pullup_mask)
{
	HAL_StatusTypeDef status;
	uint8_t reg_address;

	if(port == 0)
	{
		reg_address = GPPUA;
	}
	else if(port == 1)
	{
		reg_address = GPPUB;
	}
	status = i2c_write_reg(dev->hi2c,dev->i2c_address, reg_address,I2C_MEMADD_SIZE_8BIT,&pullup_mask,1,100);
	return status;
}

HAL_StatusTypeDef udrv_mcp23017_read_port(MCP23017_States *dev,MCP_Port port,uint8_t* data)
{
	uint8_t reg_address;
	HAL_StatusTypeDef status;

	if(port ==  0)
	{
		reg_address = GPIOA_REG;
	}
	else if(port == 1)
	{
		reg_address = GPIOB_REG;
	}

	status = i2c_read_reg(dev->hi2c,dev->i2c_address, reg_address,I2C_MEMADD_SIZE_8BIT,data,1,100);
	return status;
}

HAL_StatusTypeDef udrv_mcp23017_setting_int(MCP23017_States *dev, MCP_Port port,MCP_INT intpt_val,uint16_t exti_pin)
{
	HAL_StatusTypeDef status;
	status = HAL_ERROR;
	uint8_t reg_state;
	uint8_t off_var = L_Sig;
	dev -> int_port = port;
	dev -> exti_pin = exti_pin;


	if(port == 0)
	{
		status = i2c_write_reg(dev->hi2c,dev->i2c_address, INTCONA,I2C_MEMADD_SIZE_8BIT, &off_var,1,100);
		if(status == HAL_OK)
		{
			if(intpt_val == 1)
			{
				reg_state = GPINTENA_HIGH;
			}
			else if(intpt_val ==0)
			{
				reg_state = GPINTENA_LOW;
			}
			status = i2c_write_reg(dev->hi2c,dev->i2c_address, GPINTENA,I2C_MEMADD_SIZE_8BIT,&reg_state,1,100);
		}
	}
	else if(port == 1)
	{
		status = i2c_write_reg(dev->hi2c,dev->i2c_address, INTCONB,I2C_MEMADD_SIZE_8BIT,&off_var,1,100);

		if(status == HAL_OK)
		{
			if(intpt_val == 1)
			{
				reg_state = GPINTENB_HIGH;
			}
			else if(intpt_val ==0)
			{
				reg_state = GPINTENB_LOW;
			}
			status = i2c_write_reg(dev->hi2c,dev->i2c_address, GPINTENB,I2C_MEMADD_SIZE_8BIT,&reg_state,1,100);
		}
	}
	return status;
}

uint8_t udrv_mcp23017_checking_int(MCP23017_States *dev)
{
	Flag_state clear_init = flag_clear;
	if(dev->int_flag == 1)
	{
		dev->int_flag = 0;
		clear_init = flag_rise;
	}
	return clear_init;
}

HAL_StatusTypeDef udrv_mcp23017_mirror_int(MCP23017_States *dev,MCP_INT mirror_state)
{
	HAL_StatusTypeDef status;
	uint8_t iocon_val = 0;

	status = i2c_read_reg(dev->hi2c,dev->i2c_address, IOCONA,I2C_MEMADD_SIZE_8BIT,&iocon_val,1,100);
	if(status == HAL_OK)
	{
		if(mirror_state == INT_ON)
		{
			iocon_val |= 0x40;
		}
		else if(mirror_state == INT_OFF)
		{
			iocon_val &= ~0x40;
		}
		status = i2c_write_reg(dev->hi2c,dev->i2c_address, IOCONA,I2C_MEMADD_SIZE_8BIT,&iocon_val,1,100);
	}
	return status;

}
//
//void ucbk_stm32_callback(uint16_t GPIO_Pin)
//{
//	for(int i=0; i<mcp_device_count;i++)
//	{
//		if(mcp_registry[i]->exti_pin == GPIO_Pin)
//		{
//			mcp_registry[i]->int_flag = 1;
//		}
//	}
//}
