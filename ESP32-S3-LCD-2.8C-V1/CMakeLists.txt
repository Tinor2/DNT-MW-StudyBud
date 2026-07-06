idf_component_register(                    SRCS "display_test.c++"
                          "I2C_Driver/I2C_Driver.cpp"
                          "EXIO/TCA9554PWR.cpp"
                          "LCD_Driver/ST7701S.cpp"
                    INCLUDE_DIRS "." "I2C_Driver" "EXIO" "LCD_Driver"
                    PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_i2c
                                  esp_driver_spi esp_lcd esp_hal_lcd driver freertos log heap)
