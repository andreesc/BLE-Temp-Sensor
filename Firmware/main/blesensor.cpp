#include <string>
#include "NimBLEDevice.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
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

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer*         pServer = NULL;
BLECharacteristic* pTxCharacteristic;
bool               deviceConnected    = false;
bool               oldDeviceConnected = false;
uint8_t            txValue            = 0;

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

// Função para obter a temperatura do sensor HDC1080 via I2C
float hdc1080_get_temperature()
{
    i2c_master_bus_config_t i2c_mst_config;

    i2c_mst_config.clk_source                   = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port                     = I2C_PORT_NUM;
    i2c_mst_config.scl_io_num                   = I2C_MASTER_SCL_IO;
    i2c_mst_config.sda_io_num                   = I2C_MASTER_SDA_IO;
    i2c_mst_config.glitch_ignore_cnt            = 7;
    i2c_mst_config.flags.enable_internal_pullup = false;

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t dev_cfg;

    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = HDC1080_ADDR;
    dev_cfg.scl_speed_hz    = 100000;

    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    uint8_t temp_reg = 0x00;
    uint8_t temp_data[2];
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &temp_reg, 1, -1));
    vTaskDelay(15 / portTICK_PERIOD_MS);  // Delay necessário para a leitura
    ESP_ERROR_CHECK(i2c_master_receive(dev_handle, temp_data, 2, -1));

    uint16_t raw_temp    = (temp_data[0] << 8) | temp_data[1];
    float    temperature = ((float) raw_temp / 65536) * 165 - 40;
    return temperature;
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
