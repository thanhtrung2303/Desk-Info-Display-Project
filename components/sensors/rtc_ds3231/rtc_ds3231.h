#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define I2C_MASTER_SCL_IO 18
#define I2C_MASTER_SDA_IO 17
#define I2C_MASTER_FREQ_HZ 100000
#define DS3231_ADR 0x68

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

esp_err_t rtc_ds3231_init(i2c_master_bus_handle_t bus_handle);
void rtc_update_task(void *pvParameters);
esp_err_t rtc_ds3231_set_time(uint8_t year, uint8_t month, uint8_t date, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
#endif /* RTC_DS3231_H */