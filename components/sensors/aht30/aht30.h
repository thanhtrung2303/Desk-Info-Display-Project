#ifndef AHT30_H
#define AHT30_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define AHT30_SDA_IO    1
#define AHT30_SCL_IO    2
#define AHT30_FREQ      100000
#define I2C_NUM         I2C_NUM_1

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

esp_err_t aht30_init(i2c_master_bus_handle_t bus_handle);
esp_err_t aht30_read_data(float *temp, float *hum);

#endif /* AHT30_H */