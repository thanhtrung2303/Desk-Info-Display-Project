#ifndef LED_BRIGHTNESS_H
#define LED_BRIGHTNESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lcd_brightness_init(void);
void lcd_set_brightness(int percent);

#ifdef __cplusplus
}
#endif

#endif /* LED_BRIGHTNESS_H */