#ifndef _ML_I2C_H_
#define _ML_I2C_H_

#include "headfile.h"

/* Shared open-drain software I2C bus: PB0=SCL, PB1=SDA. */
#define I2C_GPIO             GPIOB
#define I2C_SCL_GPIO_Pin     DL_GPIO_PIN_0
#define I2C_SDA_GPIO_Pin     DL_GPIO_PIN_1
#define I2C_SCL              PB0
#define I2C_SDA              PB1

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendByte(uint8_t byte);
uint8_t I2C_ReceiveByte(void);
void I2C_SendAck(void);
void I2C_NotSendAck(void);
uint8_t I2C_WaitAck(void);

/* address8 uses the conventional left-shifted address (VL53L0X = 0x52). */
uint8_t I2C_WriteBuffer(uint8_t address8, uint8_t reg,
                        const uint8_t *data, uint16_t length);
uint8_t I2C_ReadBuffer(uint8_t address8, uint8_t reg,
                       uint8_t *data, uint16_t length);

#endif
