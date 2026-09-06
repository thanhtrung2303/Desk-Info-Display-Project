SET(SOURCES screens/ui_Screen1.c
    screens/ui_Screen2.c
    screens/ui_Screen3.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_878964720.c
    images/ui_img_icon_humid_png.c
    images/ui_img_icon_home_40_png.c
    images/ui_img_icon_ngoai_troi_40_png.c
    images/ui_img_icon_aqi_32_png.c
    fonts/ui_font_FontVN.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "."
                       REQUIRES lvgl rtc_ds3231 led_brightness)
