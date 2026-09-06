#include "rtc_ds3231.h"
#include "esp_log.h"
#include "freertos/task.h"

/*******************************************************************************
 * Variables
 ******************************************************************************/

static const char *TAG1 = "RTC_DS3231";

static i2c_master_dev_handle_t rtc_handle;

uint8_t min = 0;
uint8_t hour = 0;
uint8_t day = 0;
uint8_t date = 1;
uint8_t month = 1;
uint16_t year = 0;
bool rtc_data_ready = false;

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Hàm đổi Binary sang Decimal */
static uint8_t bcd_to_dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

/* Hàm ghi đè thời gian thực vào chip DS3231 */
esp_err_t rtc_ds3231_set_time(uint8_t year, uint8_t month, uint8_t date, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    uint8_t data[8];
    data[0] = 0x00;               /* Trỏ vào thanh ghi địa chỉ 0x00 (Giây) */
    data[1] = dec_to_bcd(sec);
    data[2] = dec_to_bcd(min);
    data[3] = dec_to_bcd(hour);   /* Tự động lưu ở chế độ 24h */
    data[4] = day;                /* Quy ước: 1=CN, 2=Thứ Hai, ..., 7=Thứ Bảy */
    data[5] = dec_to_bcd(date);
    data[6] = dec_to_bcd(month);
    data[7] = dec_to_bcd(year);   /* Chỉ lấy 2 số cuối (VD: 2026 thì nạp 26) */
    
    /* Gửi chuỗi 8 byte xuống I2C */
    return i2c_master_transmit(rtc_handle, data, 8, 100);
}

/* Khởi tạo RTC DS3231 */
esp_err_t rtc_ds3231_init(i2c_master_bus_handle_t bus_handle)
{
    /* Khai ba device DS3231 tren bus */
    i2c_device_config_t i2c_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    return i2c_master_bus_add_device(bus_handle, &i2c_dev_cfg, &rtc_handle);
}

/* Task xử lý thời gian */
void rtc_update_task(void *pvParameters)
{
    uint8_t reg_adr = 0x00; /* Thanh ghi giây (seconds) */
    uint8_t data[7];
    ESP_LOGI(TAG1, "Bắt đầu tiến trình đọc RTC DS3231...");
    for(;;)
    {
        esp_err_t err = i2c_master_transmit_receive(rtc_handle, &reg_adr, 1, data, 7, 1000);
        if(err == ESP_OK)
        {
            min = bcd_to_dec(data[1]);
            hour = bcd_to_dec(data[2] & 0x3F);
            day = bcd_to_dec(data[3]);
            date = bcd_to_dec(data[4]);
            month = bcd_to_dec(data[5] & 0x7F);
            year = bcd_to_dec(data[6]) + 2000;

            if((day < 1) || (day > 7))
            {
                day = 1;
            }

            rtc_data_ready = true;

        }
        else
        {
            ESP_LOGE(TAG1, "Lỗi đọc DS3231: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    } 
}