#include "I2C_Driver.h"

static const char *I2C_TAG = "I2C";

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_TOUCH_SDA_IO;
    conf.scl_io_num = (gpio_num_t)I2C_TOUCH_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config((i2c_port_t)I2C_MASTER_NUM, &conf);
    return i2c_driver_install((i2c_port_t)I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void I2C_Init(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(I2C_TAG, "I2C initialized successfully");
}

esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
    uint8_t buf[Length + 1];
    buf[0] = Reg_addr;
    memcpy(&buf[1], Reg_data, Length);
    return i2c_master_write_to_device((i2c_port_t)I2C_MASTER_NUM, Driver_addr, buf, Length + 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
    return i2c_master_write_read_device((i2c_port_t)I2C_MASTER_NUM, Driver_addr, &Reg_addr, 1, Reg_data, Length, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}
