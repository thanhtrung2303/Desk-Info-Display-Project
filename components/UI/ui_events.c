#include "ui.h"
#include "rtc_ds3231.h"
#include "led_brightness.h"
#include "esp_log.h"

extern volatile bool is_fahrenheit;
// Hàm thuật toán Sakamoto để tự động tính "Thứ" từ Ngày/Tháng/Năm
uint8_t calculate_day_of_week(uint16_t y, uint8_t m, uint8_t d) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    y -= m < 3;
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    return dow + 1; // Chuẩn DS3231: 1=CN, 2=T2, 3=T3...
}

// Hàm này được gọi từ giao diện SquareLine khi có Event "Call function"
void action_save_time_to_rtc(lv_event_t * e) {
    ESP_LOGI("UI_EVENT", "=============================");
    ESP_LOGI("UI_EVENT", "Nút LƯU đã được bấm!");
    
    // 1. Lấy chỉ số (Index) từ các con lăn
    uint16_t hour_idx  = lv_roller_get_selected(ui_RollerHour);
    uint16_t min_idx   = lv_roller_get_selected(ui_RollerMin);
    uint16_t date_idx  = lv_roller_get_selected(ui_RollerDate);
    uint16_t month_idx = lv_roller_get_selected(ui_RollerMonth);
    uint16_t year_idx  = lv_roller_get_selected(ui_RollerYear);

    // 2. Chuyển đổi thành giá trị thực
    uint8_t hour = hour_idx; 
    uint8_t min = min_idx;   
    uint8_t date = date_idx + 1; 
    uint8_t month = month_idx + 1; 
    
    // Thay số 2024 bằng năm bắt đầu trong cấu hình Roller của bạn
    uint16_t year = year_idx + 2026; 

    // 3. Tính toán "Thứ" & Nạp vào DS3231
    uint8_t day_of_week = calculate_day_of_week(year, month, date);
    uint8_t short_year = year % 100;

    ESP_LOGI("UI_EVENT", "Dữ liệu nạp: %02d:%02d | Ngày: %02d/%02d/%d | Thứ: %d", 
             hour, min, date, month, year, day_of_week);
    
    esp_err_t err = rtc_ds3231_set_time(short_year, month, date, day_of_week, hour, min, 0);

    if (err == ESP_OK) {
        ESP_LOGI("UI_EVENT", "=> Đã nạp thành công xuống chip DS3231!");
    } else {
        ESP_LOGE("UI_EVENT", "=> LỖI I2C! Không thể ghi vào chip.");
    }
    ESP_LOGI("UI_EVENT", "=============================");
}
void ui_event_slider_brightness(lv_event_t * e)
{
	// Your code here
    lv_obj_t *slider = lv_event_get_target(e);

    /* Doc gia tri % hien tai cua thanh truot (0-100) */
    int brightness_val = (int)lv_slider_get_value(slider);

    lcd_set_brightness(brightness_val);

}

void change_unit_action(lv_event_t * e)
{
	// Your code here
    lv_obj_t *switch_obj = lv_event_get_target(e);

    /* Kiem tra trang thai: Neu dang bat thi hien thi do F */
    if(lv_obj_has_state(switch_obj, LV_STATE_CHECKED))
    {
        is_fahrenheit = true;
        ESP_LOGI("UI_EVENT", ">>> Công tắc BẬT: Đổi sang độ F");
    }
    else
    {
        is_fahrenheit = false;
        ESP_LOGI("UI_EVENT", ">>> Công tắc TẮT: Đổi về độ C");
    }
}
