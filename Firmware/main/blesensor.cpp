#include "NimBLEDevice.h"
#include "hdc1080.h"
#include "driver/i2c_types.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include "esp_log.h"
#include <stdio.h>
#include <string>

extern "C"
{
    void app_main(void);
}


#define I2C_MASTER_SCL_IO GPIO_NUM_1
#define I2C_MASTER_SDA_IO GPIO_NUM_0

#define ADC_CHANNEL ADC_CHANNEL_1
#define ADC_UNIT ADC_UNIT_1
static int adc_raw;
static int voltage;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static const char *TAG = "HDC1080";
static const char* TAG1 = "Battery";
static const char* TAG2 = "BLE";

i2c_master_dev_handle_t   hdc1080_handle;
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t         cali_handle;

static NimBLEServer* pServer;
//static uint8_t phy = BLE_GAP_LE_PHY_2M_MASK;
//static uint16_t phyOpt = BLE_GAP_LE_PHY_CODED_ANY;
NimBLECharacteristic* pTxCharacteristic;
std::string rxValue;
bool               deviceConnected = false;
bool               oldDeviceConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks
{
public:
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override 
    { 
      deviceConnected = true;
      ESP_LOGI(TAG2, "Device connected");
      //pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 18); // args: conn handle, minInterval, maxInterval, peripheral latency, timeout;
      //pServer->updatePhy(connInfo.getConnHandle(), phy, phy, phyOpt);  // se necessario utilizar o 2M phy com o extended advertising para economizar bateria;
    };

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override { deviceConnected = false; }

} serverCallbacks;

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override
    {
        rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            printf("*********\n");
            printf("Received Value: ");
            for (int i = 0; i < rxValue.length(); i++)
                printf("%c", rxValue[i]);
            printf("\n*********\n");
        }
    }
} chrCallbacks;


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

esp_err_t battery_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config1;
    init_config1.unit_id = ADC_UNIT;
    init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;
    init_config1.clk_src = ADC_DIGI_CLK_SRC_DEFAULT;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config;
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    adc_cali_curve_fitting_config_t cali_config;
    cali_config.unit_id = ADC_UNIT_1;
    cali_config.atten = ADC_ATTEN_DB_12;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));

    return ESP_OK;
}

// Tarefa de monitoramento da conexão BLE e notificação de dados
void connectedTask(void* parameter)
{
    float temperature = 0;
    while (1)
    {
        if (deviceConnected)
        {
          if (rxValue == "t")
          {
            hdc1080_read_measurement(hdc1080_handle, &temperature);
            ESP_LOGI(TAG, "Temperature: %.2f°C\n", temperature);
            pTxCharacteristic->setValue(std::to_string(temperature));
            pTxCharacteristic->notify();
            rxValue = "";
          } 

          if (rxValue == "v")
          {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage));
            ESP_LOGI(TAG1, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_2 + 1, ADC_CHANNEL, voltage);
            pTxCharacteristic->setValue(std::to_string(voltage));
            pTxCharacteristic->notify();
            rxValue = "";
          }
        }

        if (!deviceConnected && oldDeviceConnected)
        {
            pServer->startAdvertising();
            ESP_LOGI(TAG2, "start advertising\n");
            oldDeviceConnected = deviceConnected;
        }

        if (deviceConnected && !oldDeviceConnected)
        {
            oldDeviceConnected = deviceConnected;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // Delay para resetar o watchdog timer
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    // Inicializar o dispositivo BLE
    NimBLEDevice::init("Sensor");

    battery_init();
    i2c_master_bus_handle_t bus_handle = i2c_bus_init();
    hdc1080_handle = hdc1080_device_create(bus_handle);

    hdc1080_start_measurement(hdc1080_handle);

    // Cria o servidor BLE 
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    // Criar o serviço BLE (no caso NORDIC UART) com duas características (TX e RX)
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY); // Cria a característica BLE para notificação (Tx);
    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE); // Cria a característica BLE para escrita (Rx);
    pRxCharacteristic->setCallbacks(&chrCallbacks);
    pService->start(); // Iniciar o serviço BLE

    // Instância de advertising 
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("Sensor");
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->enableScanResponse(false); // Se precisar transferir mais dados no pacote de advertising, setar como true e configurar com setScanResponseData;
    pAdvertising->setMinInterval(0); // Valor default, mudar dps para economizar bateria
    pAdvertising->setMaxInterval(0);

    pServer->start();                                                                            
    pServer->getAdvertising()->start(); 
    xTaskCreate(connectedTask, "connectedTask", 5000, NULL, 1, NULL);
    ESP_LOGI(TAG2, "Waiting a client connection to notify...\n");
}
