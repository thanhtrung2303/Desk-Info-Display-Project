idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash display_drv UI lvgl rtc_ds3231 wifi_drv api_service led_brightness aht30)
