#include "ml_i2c.h"

/*
 * PB0/PB1 are shared by the OLED and TOF200F/VL53L0X. The bus is open drain:
 * a zero enables the low output and a one releases the pin.
 */
#define I2C_HALF_PERIOD_US         3U
#define I2C_CLOCK_HIGH_TIMEOUT_US 1000U

static void i2c_delay(void)
{
    delay_us(I2C_HALF_PERIOD_US);
}

static void i2c_drive_low(uint32_t pin)
{
    gpio_set(I2C_GPIO, pin, 0U);
    DL_GPIO_enableOutput(I2C_GPIO, pin);
    i2c_delay();
}

static void i2c_release(uint32_t pin)
{
    DL_GPIO_disableOutput(I2C_GPIO, pin);
    i2c_delay();
}

static uint8_t i2c_raise_scl(void)
{
    uint32_t timeout = I2C_CLOCK_HIGH_TIMEOUT_US;

    i2c_release(I2C_SCL_GPIO_Pin);
    while((gpio_get(I2C_GPIO, I2C_SCL_GPIO_Pin) == 0U) && (timeout > 0U))
    {
        delay_us(1U);
        timeout--;
    }

    return (timeout > 0U) ? 0U : 1U;
}

void I2C_Init(void)
{
    DL_GPIO_initDigitalOutput(I2C_SCL);
    DL_GPIO_initDigitalOutput(I2C_SDA);
    gpio_set(I2C_GPIO, I2C_SCL_GPIO_Pin | I2C_SDA_GPIO_Pin, 0U);
    i2c_release(I2C_SCL_GPIO_Pin);
    i2c_release(I2C_SDA_GPIO_Pin);
}

void I2C_Start(void)
{
    i2c_release(I2C_SDA_GPIO_Pin);
    (void) i2c_raise_scl();
    i2c_drive_low(I2C_SDA_GPIO_Pin);
    i2c_drive_low(I2C_SCL_GPIO_Pin);
}

void I2C_Stop(void)
{
    i2c_drive_low(I2C_SDA_GPIO_Pin);
    (void) i2c_raise_scl();
    i2c_release(I2C_SDA_GPIO_Pin);
}

void I2C_SendByte(uint8_t byte)
{
    uint8_t mask;

    for(mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if((byte & mask) != 0U)
        {
            i2c_release(I2C_SDA_GPIO_Pin);
        }
        else
        {
            i2c_drive_low(I2C_SDA_GPIO_Pin);
        }
        (void) i2c_raise_scl();
        i2c_drive_low(I2C_SCL_GPIO_Pin);
    }
    i2c_release(I2C_SDA_GPIO_Pin);
}

uint8_t I2C_ReceiveByte(void)
{
    uint8_t value = 0U;
    uint8_t i;

    i2c_release(I2C_SDA_GPIO_Pin);
    for(i = 0U; i < 8U; i++)
    {
        value <<= 1U;
        (void) i2c_raise_scl();
        if(gpio_get(I2C_GPIO, I2C_SDA_GPIO_Pin) != 0U)
        {
            value |= 1U;
        }
        i2c_drive_low(I2C_SCL_GPIO_Pin);
    }
    return value;
}

void I2C_SendAck(void)
{
    i2c_drive_low(I2C_SDA_GPIO_Pin);
    (void) i2c_raise_scl();
    i2c_drive_low(I2C_SCL_GPIO_Pin);
    i2c_release(I2C_SDA_GPIO_Pin);
}

void I2C_NotSendAck(void)
{
    i2c_release(I2C_SDA_GPIO_Pin);
    (void) i2c_raise_scl();
    i2c_drive_low(I2C_SCL_GPIO_Pin);
}

uint8_t I2C_WaitAck(void)
{
    uint8_t no_ack;

    i2c_release(I2C_SDA_GPIO_Pin);
    if(i2c_raise_scl() != 0U)
    {
        i2c_drive_low(I2C_SCL_GPIO_Pin);
        return 1U;
    }
    no_ack = gpio_get(I2C_GPIO, I2C_SDA_GPIO_Pin);
    i2c_drive_low(I2C_SCL_GPIO_Pin);
    return no_ack;
}

uint8_t I2C_WriteBuffer(uint8_t address8, uint8_t reg,
                        const uint8_t *data, uint16_t length)
{
    uint16_t i;

    if((data == NULL) && (length != 0U))
    {
        return 4U;
    }

    I2C_Start();
    I2C_SendByte((uint8_t) (address8 & 0xFEU));
    if(I2C_WaitAck() != 0U)
    {
        I2C_Stop();
        return 1U;
    }
    I2C_SendByte(reg);
    if(I2C_WaitAck() != 0U)
    {
        I2C_Stop();
        return 2U;
    }
    for(i = 0U; i < length; i++)
    {
        I2C_SendByte(data[i]);
        if(I2C_WaitAck() != 0U)
        {
            I2C_Stop();
            return 3U;
        }
    }
    I2C_Stop();
    return 0U;
}

uint8_t I2C_ReadBuffer(uint8_t address8, uint8_t reg,
                       uint8_t *data, uint16_t length)
{
    uint16_t i;

    if((data == NULL) || (length == 0U))
    {
        return 4U;
    }

    I2C_Start();
    I2C_SendByte((uint8_t) (address8 & 0xFEU));
    if(I2C_WaitAck() != 0U)
    {
        I2C_Stop();
        return 1U;
    }
    I2C_SendByte(reg);
    if(I2C_WaitAck() != 0U)
    {
        I2C_Stop();
        return 2U;
    }
    I2C_Start();
    I2C_SendByte((uint8_t) (address8 | 0x01U));
    if(I2C_WaitAck() != 0U)
    {
        I2C_Stop();
        return 1U;
    }
    for(i = 0U; i < length; i++)
    {
        data[i] = I2C_ReceiveByte();
        if((i + 1U) < length)
        {
            I2C_SendAck();
        }
        else
        {
            I2C_NotSendAck();
        }
    }
    I2C_Stop();
    return 0U;
}
