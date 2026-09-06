#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "ui.h"
#include "config.h" 

/*******************************************************************************
 * Variables
 ******************************************************************************/

extern volatile float current_outdoor_temp; 
extern volatile bool outdoor_data_ready;

esp_err_t esp_crt_bundle_attach(void *conf);
static const char *TAG = "API_SERVICE";

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define NEWS_URL "http://api.rss2json.com/v1/api.json?rss_url=https%3A%2F%2Fvnexpress.net%2Frss%2Fthe-thao.rss"

/*******************************************************************************
 * Code
 ******************************************************************************/

static char* http_get_request(const char* url) {
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(esp_http_client_open(client, 0) != ESP_OK) 
    {
        esp_http_client_cleanup(client);
        return NULL;
    }
    esp_http_client_fetch_headers(client);
    
    char *buffer = malloc(32768); 
    if(buffer == NULL) 
    {
        esp_http_client_cleanup(client);
        return NULL;
    }
    
    int read_len = 0;
    for(;;)
    {
        int ret = esp_http_client_read(client, buffer + read_len, 32768 - read_len - 1);
        if (ret <= 0) break;
        read_len += ret;
    }
    buffer[read_len] = '\0'; 
    esp_http_client_cleanup(client);
    return buffer;
}

void api_weather_news_task(void *pvParameters) 
{
    char waqi_url[256];
    snprintf(waqi_url, sizeof(waqi_url), "https://api.waqi.info/feed/@1583/?token=%s", WAQI_TOKEN);

    char owm_url[512];
    snprintf(owm_url, sizeof(owm_url), "https://api.openweathermap.org/data/2.5/weather?q=Hanoi&appid=%s&units=metric", OPENWEATHER_API_KEY);
    for(;;)
    {
        ESP_LOGI(TAG, "Đang tải dữ liệu từ 3 máy chủ API...");

        /* Tải chỉ số AQI từ WAQI */
        char *aqi_json = http_get_request(waqi_url);
        if (aqi_json) {
            cJSON *root = cJSON_Parse(aqi_json);
            if (root) {
                cJSON *data = cJSON_GetObjectItem(root, "data");
                if (data) {
                    int aqi = cJSON_GetObjectItem(data, "aqi")->valueint;
                    lv_lock();
                    if(ui_LabelAQI) lv_label_set_text_fmt(ui_LabelAQI, "%d", aqi);
                    lv_unlock();
                }
                cJSON_Delete(root); 
            }
            free(aqi_json); 
        }

        /* Tải thời tiết từ OpenWeatherMap */
        char *weather_json = http_get_request(owm_url);
        if (weather_json) {
            cJSON *root = cJSON_Parse(weather_json);
            if (root) {
                cJSON *main_obj = cJSON_GetObjectItem(root, "main");
                if (main_obj) {
                    /* Lấy số liệu chuẩn (đã tự động cấu hình độ C qua tham số units=metric) */
                    double temp = cJSON_GetObjectItem(main_obj, "temp")->valuedouble;
                    double humid = cJSON_GetObjectItem(main_obj, "humidity")->valuedouble;
                    current_outdoor_temp = temp;
                    outdoor_data_ready = true; /* Bật cờ cho phép UI bắt đầu hiển thị */
                    
                    char temp_buf[16], humid_buf[16];
                    snprintf(temp_buf, sizeof(temp_buf), "%.0f C", temp);
                    snprintf(humid_buf, sizeof(humid_buf), "%.0f %%", humid);

                    lv_lock();
                    if(ui_LabelOutTemp) lv_label_set_text(ui_LabelOutTemp, temp_buf);
                    if(ui_LabelOutHum) lv_label_set_text(ui_LabelOutHum, humid_buf);
                    lv_unlock();
                }
                cJSON_Delete(root); 
            }
            free(weather_json); 
        }

        /* Tải dữ liệu tin tức (VNExpress) */
        char *news_json = http_get_request(NEWS_URL);
        if (news_json) {
            cJSON *root = cJSON_Parse(news_json);
            if (root) {
                /* VNExpress trả về mảng "items" */
                cJSON *items = cJSON_GetObjectItem(root, "items");
                if (cJSON_IsArray(items)) {
                    int num_news = cJSON_GetArraySize(items);
                    const char *title1 = "Chưa có tin mới...";
                    const char *title2 = "Chưa có tin mới...";
                    const char *title3 = "Chưa có tin mới...";

                    /* Trích tiêu đề của 3 bài báo mới nhất */
                    if (num_news > 0) {
                        cJSON *t1 = cJSON_GetObjectItem(cJSON_GetArrayItem(items, 0), "title");
                        if(t1 && t1->valuestring) title1 = t1->valuestring;
                    }
                    if (num_news > 1) {
                        cJSON *t2 = cJSON_GetObjectItem(cJSON_GetArrayItem(items, 1), "title");
                        if(t2 && t2->valuestring) title2 = t2->valuestring;
                    }
                    if (num_news > 2) {
                        cJSON *t3 = cJSON_GetObjectItem(cJSON_GetArrayItem(items, 2), "title");
                        if(t3 && t3->valuestring) title3 = t3->valuestring;
                    }

                    /* Đẩy lên màn hình */
                    lv_lock();
                    if(ui_Label15) {
                        lv_label_set_long_mode(ui_Label15, LV_LABEL_LONG_CLIP);
                        lv_label_set_text(ui_Label15, title1);
                        lv_label_set_long_mode(ui_Label15, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    }
                    if(ui_Label1) {
                        lv_label_set_long_mode(ui_Label1, LV_LABEL_LONG_CLIP);
                        lv_label_set_text(ui_Label1, title2);
                        lv_label_set_long_mode(ui_Label1, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    }
                    if(ui_Label16) {
                        lv_label_set_long_mode(ui_Label16, LV_LABEL_LONG_CLIP);
                        lv_label_set_text(ui_Label16, title3);
                        lv_label_set_long_mode(ui_Label16, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    }
                    lv_unlock();
                }
                cJSON_Delete(root);
            }
            free(news_json);
        }

        vTaskDelay(pdMS_TO_TICKS(15 * 60 * 1000));
    }
}