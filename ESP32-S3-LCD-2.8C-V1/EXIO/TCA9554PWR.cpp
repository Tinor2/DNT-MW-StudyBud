#include "TCA9554PWR.h"

uint8_t Read_REG(uint8_t REG)
{
    uint8_t bitsStatus = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9554_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9554_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &bitsStatus, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return bitsStatus;
}

void Write_REG(uint8_t REG, uint8_t Data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TCA9554_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG, true);
    i2c_master_write_byte(cmd, Data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin((i2c_port_t)I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

void Mode_EXIO(uint8_t Pin, uint8_t State)
{
    uint8_t bitsStatus = Read_REG(TCA9554_CONFIG_REG);
    uint8_t Data;
    if (State)
        Data = (0x01 << (Pin - 1)) | bitsStatus;
    else
        Data = ~(0x01 << (Pin - 1)) & bitsStatus;
    Write_REG(TCA9554_CONFIG_REG, Data);
}

void Mode_EXIOS(uint8_t PinState)
{
    Write_REG(TCA9554_CONFIG_REG, PinState);
}

uint8_t Read_EXIO(uint8_t Pin)
{
    uint8_t inputBits = Read_REG(TCA9554_INPUT_REG);
    return (inputBits >> (Pin - 1)) & 0x01;
}

uint8_t Read_EXIOS(void)
{
    return Read_REG(TCA9554_INPUT_REG);
}

void Set_EXIO(uint8_t Pin, uint8_t State)
{
    uint8_t Data = 0;
    uint8_t bitsStatus = Read_REG(TCA9554_OUTPUT_REG);
    if (State < 2 && Pin < 9 && Pin > 0) {
        if (State == 1)
            Data = (0x01 << (Pin - 1)) | bitsStatus;
        else
            Data = (~(0x01 << (Pin - 1)) & bitsStatus);
        Write_REG(TCA9554_OUTPUT_REG, Data);
    }
}

void Set_EXIOS(uint8_t PinState)
{
    Write_REG(TCA9554_OUTPUT_REG, PinState);
}

void Set_Toggle(uint8_t Pin)
{
    uint8_t bitsStatus = Read_EXIO(Pin);
    Set_EXIO(Pin, !bitsStatus);
}

void TCA9554PWR_Init(uint8_t PinState)
{
    Mode_EXIOS(PinState);
}

esp_err_t EXIO_Init(void)
{
    TCA9554PWR_Init(0x00);
    Set_EXIO(TCA9554_EXIO8, false);
    return ESP_OK;
}
