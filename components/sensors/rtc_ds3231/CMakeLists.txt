idf_component_register(SRCS "rtc_ds3231.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl driver esp_driver_i2c UI)