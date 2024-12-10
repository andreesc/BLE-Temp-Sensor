#include <string>
#include "NimBLEDevice.h"
#include "driver/i2c_types.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include <stdio.h>

extern "C"
{
    void app_main(void);
}

#define I2C_MASTER_SCL_IO GPIO_NUM_22
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_PORT_NUM -1

#define HDC1080_ADDR 0x40
#define HDC1080_TEMP_REG 0x00

#define ADC_CHANNEL ADC_CHANNEL_9
static int adc_raw[2][10];
static int voltage[2][10];

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static const char*        TAG = "HDC1080";
i2c_master_dev_handle_t   hdc1080_handle;
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t         cali_handle;

BLEServer*         pServer = NULL;
BLECharacteristic* pTxCharacteristic;
bool               deviceConnected = false;
bool               oldDeviceConnected = false;
uint8_t            txValue = 0;

class MyServerCallbacks : public BLEServerCallbacks
{
public:
    void onConnect(BLEServer* pServer, BLEConnInfo& connInfo) { deviceConnected = true; };
    void onDisconnect(BLEServer* pServer, BLEConnInfo& connInfo, int reason) { deviceConnected = false; }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* pCharacteristic, BLEConnInfo& connInfo)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            printf("*********\n");
            printf("Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
                printf("%c", rxValue[i]);
            printf("\n*********\n");
        }
    }
};

i2c_master_bus_handle_t i2c_bus_init()
{
    i2c_master_bus_config_t i2c_mst_config;

    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = I2C_PORT_NUM;
    i2c_mst_config.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_mst_config.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = false;

    i2c_master_bus_handle_t bus_handle;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_LOGI(TAG, "I2C master bus created");

    return bus_handle;
}

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

// Função para obter a temperatura do sensor HDC1080 via I2C
float hdc1080_get_temperature()
{
    uint8_t temp_reg = 0x00;
    uint8_t temp_data[2];
    ESP_ERROR_CHECK(i2c_master_transmit(hdc1080_handle, &temp_reg, 1, -1));
    vTaskDelay(pdMS_TO_TICKS(15));  // Delay necessário para a leitura
    ESP_ERROR_CHECK(i2c_master_receive(hdc1080_handle, temp_data, 2, -1));
    uint16_t raw_temp = (temp_data[0] << 8) | temp_data[1];
    float    temperature = ((float) raw_temp / 65536) * 165 - 40;
    return temperature;
}

esp_err_t battery_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1;
    init_config1.unit_id = ADC_UNIT_2;
    init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
    init_config1.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config;
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    adc_cali_line_fitting_config_t cali_config;
    cali_config.unit_id = ADC_UNIT_2;
    cali_config.atten = ADC_ATTEN_DB_12;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle));

    return ESP_OK;
}

// Tarefa de monitoramento da conexão BLE e notificação de dados
void connectedTask(void* parameter)
{
    for (;;)
    {
        if (deviceConnected)
        {
            pTxCharacteristic->setValue(&txValue, 1);
            pTxCharacteristic->notify();
            txValue++;
        }

        if (!deviceConnected && oldDeviceConnected)
        {
            pServer->startAdvertising();
            printf("start advertising\n");
            oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected)
        {
            oldDeviceConnected = deviceConnected;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay para resetar o watchdog timer
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    // Inicializar o dispositivo BLE
    BLEDevice::init("UART Service");

    esp_err_t err = battery_init();

    while (1)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw[0][0]));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_raw[0][0], &voltage[0][0]));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, ADC_CHANNEL, voltage[0][0]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


    i2c_master_bus_handle_t bus_handle = i2c_bus_init();
    hdc1080_handle = hdc1080_device_create(bus_handle);

    // Criar o servidor BLE
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Criar o serviço BLE
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Criar a característica BLE para notificação
    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY);

    // Criar a característica BLE para escrita
    BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE);
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Iniciar o serviço BLE
    pService->start();
    xTaskCreate(connectedTask, "connectedTask", 5000, NULL, 1, NULL);
    pServer->getAdvertising()->start();
    printf("Waiting a client connection to notify...\n");

    // Leitura da temperatura (exemplo de uso da função)
    float temperature = hdc1080_get_temperature();
    printf("Temperature: %.2f°C\n", temperature);
}
