idf_component_register(SRCS "display_drv.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp_timer lvgl esp_lcd_st7796)