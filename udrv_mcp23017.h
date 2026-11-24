/*
 * udrv_example.h
 *
 *  Created on: Sep 16, 2026
 *      Author: hammad
 */

#ifndef INC_UDRV_MCP23017_H_
#define INC_UDRV_MCP23017_H_

#include "stm32f1xx_hal.h"

#define IODIRA 0x00
#define IODIRB 0x01

#define IODIRA_IN 0xFF
#define IODIRB_IN 0xFF
#define IODIRA_OUT 0x00
#define IODIRB_OUT 0x00

#define GPINTENA 0x04
#define GPINTENB 0x05

#define GPINTENA_HIGH 0xFF
#define GPINTENB_HIGH 0xFF
#define GPINTENA_LOW 0x00
#define GPINTENB_LOW 0x00

#define IOCONA 0x0A
#define IOCONB 0x0B

#define GPPUA 0x0C
#define GPPUB 0x0D

#define GPPUA_HIGH 0xFF
#define GPPUB_HIGH 0xFF

#define GPIOA_REG 0x12
#define GPIOB_REG 0x13

#define	ALL_GPIOA_HIGH 0xFF
#define ALL_GPIOB_HIGH 0xFF
#define	ALL_GPIOA_LOW 0x00
#define ALL_GPIOB_LOW 0x00

#define DEFVALA 0x06
#define DEFVALB 0x07
#define INTCONA 0x08
#define INTCONB 0x09

#define H_Sig 0xFF
#define L_Sig 0x00


typedef enum{
	MCP_PORT_A = 0,
	MCP_PORT_B = 1,
}MCP_Port;
typedef enum{
	INT_OFF = 0,
	INT_ON = 1
}MCP_INT;

typedef struct{
	I2C_HandleTypeDef *hi2c;
	uint8_t i2c_address;
	uint16_t exti_pin;
	MCP_Port int_port;
	volatile uint8_t int_flag;
}MCP23017_States;


/*
 * @brief
 *
 *
 * */
void udrv_mcp23017_init(MCP23017_States *dev,I2C_HandleTypeDef *hi2c,uint8_t dev_number);
HAL_StatusTypeDef udrv_mcp23017_set_port_dir(MCP23017_States *dev,MCP_Port port,uint8_t dir_mask);
HAL_StatusTypeDef udrv_mcp23017_setting_int(MCP23017_States *dev,MCP_Port port,MCP_INT intpt_val,uint16_t exti_pin);
HAL_StatusTypeDef udrv_mcp23017_write_port(MCP23017_States *dev,MCP_Port port, uint8_t value);
HAL_StatusTypeDef udrv_mcp23017_pullup(MCP23017_States *dev,MCP_Port port,uint8_t pullup_mask);
HAL_StatusTypeDef udrv_mcp23017_read_both_ports(MCP23017_States *dev, uint8_t *data);
HAL_StatusTypeDef udrv_mcp23017_mirror_int(MCP23017_States *dev,MCP_INT mirror_int);
uint8_t udrv_mcp23017_checking_int(MCP23017_States *dev);
HAL_StatusTypeDef udrv_mcp23017_read_port(MCP23017_States *dev,MCP_Port port,uint8_t* data);
//uint8_t udrv_mcp23017_read_port(MCP23017_States *dev,MCP_Port port,uint8_t* data);

/* TODO: Pin wise config */


#endif /* INC_UDRV_MCP23017_H_ */
