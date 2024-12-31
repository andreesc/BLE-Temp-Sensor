#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c_master.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_PORT_NUM -1

#define HDC1080_ADDR 0x40
#define HDC1080_TEMP_REG 0x00

i2c_master_dev_handle_t hdc1080_device_create(i2c_master_bus_handle_t bus_handle);

esp_err_t hdc1080_start_measurement(i2c_master_dev_handle_t dev_handle);  

esp_err_t hdc1080_read_measurement(i2c_master_dev_handle_t dev_handle, float* temperature); 

#ifdef __cplusplus
}
#endif
