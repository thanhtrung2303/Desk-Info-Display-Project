#include "display_drv.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h" 
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define FT6336U_ADDR 0x38

/*******************************************************************************
 * Variables
 ******************************************************************************/

static lv_display_t * display = NULL;
static lv_indev_t * indev_touchpad;
static i2c_master_dev_handle_t touch_i2c_dev_handle;

static int16_t last_x = 0;
static int16_t last_y = 0;
static bool is_pressed = false;

// Hàng đợi FreeRTOS để giao tiếp giữa ngắt (ISR) và luồng LVGL
static QueueHandle_t touch_queue;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

esp_err_t display_drv_init(void);

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Hàm phục vụ ngắt (ISR) cho cảm ứng */
static void IRAM_ATTR touch_isr_handler(void* arg)
{
    uint32_t pin = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Gửi tín hiệu vào queue báo hiệu có ngắt xảy ra
    xQueueSendFromISR(touch_queue, &pin, &xHigherPriorityTaskWoken);
    
    if(xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    if(display) 
    {
        lv_display_flush_ready(display);
    }
    return false;
}

/* Hàm vẽ pixel */
static void disp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    uint32_t size_px = lv_area_get_width(area) * lv_area_get_height(area);
    
    /* Thuật toán đảo byte kép */
    uint32_t *buf32 = (uint32_t *)px_map;
    uint32_t size32 = size_px / 2; /* Xử lý 2 pixel cùng lúc */
    for(uint32_t i = 0; i < size32; i++) 
    {
        uint32_t v = buf32[i];
        buf32[i] = ((v & 0x00FF00FF) << 8) | ((v & 0xFF00FF00) >> 8);
    }
    if(size_px % 2) 
    { 
        /* Xử lý lẻ pixel cuối cùng (Nếu có) */
        uint16_t *buf16 = (uint16_t *)px_map;
        buf16[size_px - 1] = (buf16[size_px - 1] >> 8) | (buf16[size_px - 1] << 8);
    }

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    uint32_t pin;
    bool read_i2c = false;
    
    while(xQueueReceive(touch_queue, &pin, 0) == pdTRUE) 
    {
        read_i2c = true;
    }

    if(read_i2c) 
    {
        uint8_t reg_td_status = 0x02;
        uint8_t reg_p1_xh = 0x03;
        uint8_t td_status = 0;
        uint8_t touch_data[4];

        esp_err_t ret = i2c_master_transmit_receive(touch_i2c_dev_handle, &reg_td_status, 1, &td_status, 1, 100);

        if(ret == ESP_OK && (td_status & 0x0F) > 0) 
        {
            i2c_master_transmit_receive(touch_i2c_dev_handle, &reg_p1_xh, 1, touch_data, 4, 100);

            uint16_t touch_x = ((touch_data[0] & 0x0F) << 8) | touch_data[1];
            uint16_t touch_y = ((touch_data[2] & 0x0F) << 8) | touch_data[3];

            last_x = touch_y;
            last_y = TFT_VER_RES - touch_x; 
            is_pressed = true;

            ESP_LOGI("LVGL_TOUCH", "Có chạm! X: %d | Y: %d", last_x, last_y); 
        } 
        else
        {
            is_pressed = false;
        }
    }

    if(is_pressed) 
    {
        data->state = LV_INDEV_STATE_PRESSED; 
        data->point.x = last_x;
        data->point.y = last_y;
    } 
    else 
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


esp_err_t display_drv_init(void)
{
    /* Chia kích thước về 1/10 màn hình để tránh hiện tượng nhấp nháy màn hình */
    size_t draw_buf_size = TFT_HOR_RES * TFT_VER_RES / 10 * sizeof(uint16_t);

    spi_bus_config_t buscfg = {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = TFT_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = draw_buf_size + 8, 
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC,
        .cs_gpio_num = TFT_CS,
        .pclk_hz = 80 * 1000 * 1000, 
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle));

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, NULL));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, 
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false)); 
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true)); 
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    gpio_set_direction(TFT_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(TFT_BL, 1);

    lv_init();

    /* Chuyển bộ đệm vào PSRAM */
    void *buf1 = heap_caps_malloc(draw_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(draw_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    assert(buf1 != NULL && buf2 != NULL);

    display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_user_data(display, panel_handle);
    lv_display_set_flush_cb(display, disp_flush_cb);
    
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    
    /* Cung cấp khung hình lớn nhưng chỉ bắt LVGL vẽ lại các pixel có sự thay đổi */
    lv_display_set_buffers(display, buf1, buf2, draw_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Cấu hình I2C & cảm ứng */
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = TOUCH_I2C_HOST,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6336U_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &touch_i2c_dev_handle));

    /* Khởi tạo hàng đợi FreeRTOS (Lưu trữ tối đa 10 ngắt chưa kịp xử lý) */
    touch_queue = xQueueCreate(10, sizeof(uint32_t));

    /* Cấu hình chân Ngắt cho Touchpad */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .pin_bit_mask = (1ULL << TOUCH_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    /* Cài đặt dịch vụ Ngắt toàn cục của ESP32 */
    esp_err_t isr_res = gpio_install_isr_service(0);
    if (isr_res == ESP_OK || isr_res == ESP_ERR_INVALID_STATE)
    {
        gpio_isr_handler_add(TOUCH_INT, touch_isr_handler, (void*) TOUCH_INT);
    }

    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touch_read_cb);
    lv_indev_set_display(indev_touchpad, display);
    
    return ESP_OK;
}