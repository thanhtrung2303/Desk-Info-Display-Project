#include "aht30.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define AHT30_ADDR 0x38

static const char *TAG = "AHT30";

static i2c_master_dev_handle_t aht30_dev_handle;

esp_err_t aht30_init(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT30_ADDR,
        .scl_speed_hz = AHT30_FREQ
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_config, &aht30_dev_handle);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Loi gan thiet bi AHT30 vao Bus!");
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(40));

    /* Kiểm tra trạng thái và hiệu chuẩn trên AHT30 */
    uint8_t status_byte = 0;
    err = i2c_master_receive(aht30_dev_handle, &status_byte, 1, -1);
    if(err == ESP_OK)
    {
        if((status_byte & 0x08) == 0)
        {
            ESP_LOGW(TAG, "AHT30 chưa được hiệu chuẩn! Đang hiệu chuẩn...");
            uint8_t reset_cmd = 0xBA;
            i2c_master_transmit(aht30_dev_handle, &reset_cmd, 1, -1);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        else
        {
            ESP_LOGI(TAG, "AHT30 đã được hiệu chuẩn! Sẵn sàng!");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Không thể đọc byte trạng thái của AHT30!");
        return err;
    }

    return ESP_OK;
}

esp_err_t aht30_read_data(float *temp, float *hum)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    uint8_t data[6];
    esp_err_t err = i2c_master_transmit(aht30_dev_handle, cmd, sizeof(cmd), -1);
    if(err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(120));

    err = i2c_master_receive(aht30_dev_handle, data, sizeof(data), -1);

    if((data[0] & 0x80) != 0)
    {
        ESP_LOGW(TAG, "Thiết bị báo bận! Chưa xử lý xong dữ liệu!");
        return ESP_FAIL;
    }

    uint32_t hum_raw = (((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4));
    uint32_t temp_raw = ((((uint32_t) data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5]);
    
    *hum = ((float)hum_raw / 1048576.0f) * 100.0f;
    *temp = ((float)temp_raw / 1048576.0f) * 200.0f - 50.0f;

    return ESP_OK;
}