#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "hdc1080.h"

static const char *TAG = "HDC1080";

i2c_master_dev_handle_t hdc1080_device_create(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_cfg;

    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = HDC1080_ADDR;
    dev_cfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    return dev_handle;
}

esp_err_t hdc1080_start_measurement(i2c_master_dev_handle_t dev_handle)
{
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_err_t ret;
    uint8_t   config_reg[1] = { 0x02 };
    uint8_t   config_data[2] = { 0x00, 0x00 };  // Independente, 14 bits de resolução

    ret = i2c_master_transmit(dev_handle, config_reg, 1, -1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to the sensor");
        return ret;
    }

    i2c_master_transmit(dev_handle, config_data, 2, -1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to the sensor");
        return ret;
    }

    ESP_LOGI(TAG, "Sensor configuration successful");
    return ESP_OK;
}

esp_err_t hdc1080_read_measurement(i2c_master_dev_handle_t dev_handle, float* temperature)
{
    esp_err_t ret;

    uint8_t temp_reg[1] = { 0x00 };
    uint8_t temp_data[2] = { 0 };

    ret = i2c_master_transmit(dev_handle, temp_reg, 1, -1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to the sensor");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    ret = i2c_master_receive(dev_handle, temp_data, 2, -1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read from the sensor");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint16_t raw_temp = (temp_data[0] << 8) | temp_data[1];

    *temperature = 165.0f * (float) raw_temp / 65536.0f - 40.0f;

    return ESP_OK;
}











