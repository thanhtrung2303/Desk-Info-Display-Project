#ifndef DISPLAY_DRV_H
#define DISPLAY_DRV_H

#include "lvgl.h"
#include "esp_err.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Kích thước màn hình */
#define TFT_HOR_RES     480
#define TFT_VER_RES     320

/* Cấu hình SPI cho ST7796 */
#define TFT_SPI_HOST    SPI2_HOST
#define TFT_MISO       -1
#define TFT_MOSI        11
#define TFT_SCLK        12
#define TFT_CS          10
#define TFT_DC          9
#define TFT_RST         8
#define TFT_BL          4

/* Cấu hình I2C & Ngắt cho FT6336U */
#define TOUCH_I2C_HOST  I2C_NUM_0
#define TOUCH_SDA       6
#define TOUCH_SCL       7
#define TOUCH_INT       3 

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

esp_err_t display_drv_init(void);
void touch_interrupt_task(void *pvParameters);

#endif /* DISPLAY_DRV_H */