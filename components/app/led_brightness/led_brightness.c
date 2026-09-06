#include "led_brightness.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define TFT_LED_PIN    4
#define LEDC_TIMER     LEDC_TIMER_0
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL   LEDC_CHANNEL_0
#define LEDC_DUTY_RES  LEDC_TIMER_8_BIT
#define LEDC_FREQ      5000

void lcd_brightness_init(void)
{
    /* Cau hinh Timer */
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    /* Cau hinh PWM */
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = TFT_LED_PIN,
        .duty = 255, /* Mac dinh khoi dong do sang 100% */
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);

}
void lcd_set_brightness(int percent)
    {
        if(percent > 100) percent = 100;
        if(percent < 10) percent = 10;

        uint32_t duty = (percent * 255) / 100;

        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }