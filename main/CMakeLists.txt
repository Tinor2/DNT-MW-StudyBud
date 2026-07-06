idf_component_register(SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ESP32-S3-LCD-2.8C-V1
    PRIV_REQUIRES driver freertos log heap esp_lcd)
