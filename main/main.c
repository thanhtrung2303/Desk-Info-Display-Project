#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "wifi_drv.h"  

#include "lvgl.h"
#include "display_drv.h" 
#include "ui.h"          

#include "rtc_ds3231.h"
#include "led_brightness.h"
#include "aht30.h"

/*******************************************************************************
 * Variables
 ******************************************************************************/

static const char *TAG = "thanhtrung_main";

extern uint8_t min, hour, day, date, month;
extern uint16_t year;
extern bool rtc_data_ready;

/* Khai báo biến chứa nhiệt độ ẩm */
volatile float current_temp = 0.0f;
volatile float current_humid = 0.0f;
volatile bool aht30_data_ready = false;
volatile float current_outdoor_temp = 0.0f; 
volatile bool outdoor_data_ready = false;

/* Kéo biến cờ Switch từ file ui_events.c sang */
volatile bool is_fahrenheit = false;

const char* day_names_vn[] = {
                "", "Chủ nhật", "Thứ 2", "Thứ 3", "Thứ 4", "Thứ 5", "Thứ 6", "Thứ 7"
};

/* LVGL Timer: Chạy mỗi 1 giây để cập nhật UI an toàn */
static void ui_sync_timer_cb(lv_timer_t * timer) {
    ESP_LOGI("UI_TIMER", "Timer chạy! Data_ready: %d | Time: %02d:%02d", rtc_data_ready, hour, min);
    if(rtc_data_ready) 
    {
        if(ui_LabelTime != NULL) 
        {
            lv_label_set_text_fmt(ui_LabelTime, "%02d:%02d", hour, min);
        }
        if(ui_LabelDate != NULL) 
        {
            lv_label_set_text_fmt(ui_LabelDate, "%s, ngày %02d tháng %02d năm %04d", 
                                  day_names_vn[day], date, month, year);
        }
    }

    /* Cập nhật nhiệt độ trong nhà */
    if(aht30_data_ready) 
    {
        float display_temp = current_temp;
    
        /* Xử lý logic gạt công tắc C/F */
        if(is_fahrenheit) 
        {
            display_temp = (current_temp * 1.8f) + 32.0f;
        }

        ESP_LOGI(TAG, "Nhiệt độ: %.1f %s | Độ ẩm: %.1f %%", 
                     display_temp, is_fahrenheit ? "F" : "C", current_humid);

        char temp_string[32];
        char hum_string[32];

        if(ui_LabelHomeTemp)
        {
            snprintf(temp_string, sizeof(temp_string), "%.0f %s", display_temp, is_fahrenheit ? "F" : "C");
            lv_label_set_text(ui_LabelHomeTemp, temp_string);
        }
        
        if(ui_LabelHomeHum) 
        {
            snprintf(hum_string, sizeof(hum_string), "%.0f %%", current_humid);
            lv_label_set_text(ui_LabelHomeHum, hum_string);
        }
    }

    /* Cập nhật nhiệt độ ngoài trời */
    if(outdoor_data_ready) 
    {
        float display_out_temp = current_outdoor_temp;
        
        if(is_fahrenheit) 
        {
            display_out_temp = (current_outdoor_temp * 1.8f) + 32.0f;
        }

        char out_temp_string[32];
        
        if(ui_LabelOutTemp) 
        { 
            snprintf(out_temp_string, sizeof(out_temp_string), "%.0f %s", display_out_temp, is_fahrenheit ? "F" : "C");           
            lv_label_set_text(ui_LabelOutTemp, out_temp_string);
        }
    }
}


/* Task GUI (Chạy trên Core 1) */
void gui_task(void *pvParameter) {
    ESP_LOGI(TAG, "GUI Task đang chạy trên Core %d", xPortGetCoreID());
    
    while (1) {
        lv_lock();
        uint32_t time_till_next = lv_timer_handler(); 
        lv_unlock();
        
        // Chuyển đổi mili-giây sang Tick của FreeRTOS
        TickType_t delay_ticks = pdMS_TO_TICKS(time_till_next);
        
        /* Bắt buộc ngủ tối thiểu 1 Tick (10ms) để chống lỗi tràn Watchdog */
        if (delay_ticks == 0) {
            delay_ticks = 1; 
        }
        
        vTaskDelay(delay_ticks);
        lv_tick_inc(10);
    }
}

/* Task đọc cảm biến AHT30 */
void aht30_task(void *pvParameters) {
    float temp = 0.0f, humid = 0.0f;
    for(;;){
        if (aht30_read_data(&temp, &humid) == ESP_OK) {
            // Chỉ nạp số vào biến, không đụng chạm đến giao diện
            current_temp = temp;
            current_humid = humid;
            aht30_data_ready = true;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Đang khởi động hệ thống IoT Dashboard ESP32-S3...");

    /* Khởi tạo bộ nhớ NVS cho WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Khởi động WiFi */
    wifi_init_sta();

    /* Khởi tạo ngoại vi và UI */
    display_drv_init(); 
    ESP_LOGI(TAG, "Khởi tạo LCD & Touch thành công.");

    /* Lock hệ thống trước khi nạp giao diện SquareLine */
    lv_lock();
    ui_init();
    lv_timer_create(ui_sync_timer_cb, 1000, NULL);
    lv_unlock();
    ESP_LOGI(TAG, "Đã nạp giao diện UI.");

    /* Khởi chạy Task giao diện vào Core 1, cấp Stack 8KB, Priority 5 (Mức rất cao để chống lag UI) */
    xTaskCreatePinnedToCore(gui_task, "GUI_Task", 1024 * 8, NULL, 5, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Khởi tạo Bus I2C cho cảm biến */
    i2c_master_bus_config_t sensor_bus_config = {
        .i2c_port = I2C_NUM_1,   
        .sda_io_num = 2,         
        .scl_io_num = 1,        
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_master_bus_handle_t sensor_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&sensor_bus_config, &sensor_bus_handle));

    /* Công cụ quét địa chỉ I2C (I2C Scanner) */
    ESP_LOGW(TAG, "--- BẮT ĐẦU QUÉT I2C BUS 1 (Chân 1 & 2) ---");
    for(uint8_t i = 1; i < 127; i++) 
    {
        /* Probe thử từng địa chỉ xem có ai trả lời (ACK) không */
        if(i2c_master_probe(sensor_bus_handle, i, 100) == ESP_OK) 
        {
            ESP_LOGI(TAG, ">> TÌM THẤY THIẾT BỊ TẠI ĐỊA CHỈ: 0x%02X", i);
        }
    }
    ESP_LOGW(TAG, "--- QUÉT XONG ---");
    
    /* Khởi tạo DS3231 */
    esp_err_t rtc_status = rtc_ds3231_init(sensor_bus_handle);
    if(rtc_status == ESP_OK) 
    {
        ESP_LOGI(TAG, "Đã khởi tạo DS3231 trên I2C_NUM_1.");
        xTaskCreatePinnedToCore(rtc_update_task, "RTC_Task", 1024 * 4, NULL, 3, NULL, 0);
    } 
    else 
    {
        ESP_LOGE(TAG, "Lỗi kết nối DS3231!");
    }

    /* Khởi tạo AHT30 */
    if(aht30_init(sensor_bus_handle) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Đã khởi tạo AHT30 trên I2C_NUM_1.");
        xTaskCreatePinnedToCore(aht30_task, "aht30_task", 4096, NULL, 5, NULL, 1);
    } 
    else 
    {
        ESP_LOGE(TAG, "Lỗi kết nối AHT30!");
    }

    lcd_brightness_init();
    lcd_set_brightness(100);
    ESP_LOGI(TAG, "Hệ thống khởi động hoàn tất! Nhường main thread cho FreeRTOS.");
}
