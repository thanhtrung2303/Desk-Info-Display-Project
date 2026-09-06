idf_component_register(SRCS "led_brightness.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_ledc driver)