#include "wifi_drv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "api_service.h"
#include "config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MAXIMUM_RETRY  5

/*******************************************************************************
 * Variables
 ******************************************************************************/

static const char *TAG = "thanhtrung_wifi";
static int s_retry_num = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Hàm xử lý các sự kiện mạng (Event Handler) */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Đang bắt đầu kết nối WiFi...");
    } 
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if(s_retry_num < MAXIMUM_RETRY) 
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Mất kết nối! Đang thử kết nối lại lần %d...", s_retry_num);
        } 
        else 
        {
            ESP_LOGE(TAG, "Không thể kết nối tới WiFi. Vui lòng kiểm tra lại pass.");
        }
    } 
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "========== KẾT NỐI THÀNH CÔNG ==========");
        ESP_LOGI(TAG, "IP ADDRESS: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "========================================");
        xTaskCreatePinnedToCore(api_weather_news_task, "API_Task", 1024 * 8, NULL, 3, NULL, 0);
        s_retry_num = 0;
    }
}

void wifi_init_sta(void) 
{
    /* Khởi tạo giao thức TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* Khởi tạo cấu hình WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Đăng ký nhận sự kiện WiFi và IP */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    /* Cấu hình thông số WiFi */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}