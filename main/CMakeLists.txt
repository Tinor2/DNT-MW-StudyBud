idf_component_register(SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ESP32-S3-LCD-2.8C-V1
    PRIV_REQUIRES driver esp_driver_gpio esp_driver_ledc esp_driver_spi
                esp_lcd esp_mm freertos heap log)
