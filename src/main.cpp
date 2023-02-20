/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "periph_init.h"
#include "driver/i2c.h"
#include "esp_pm.h"

#include "max1730x.h"
#include "Wifi.h"
#include "mqtt.h"

#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "pinDef.h"

#include "radio.h"
#include "sx1280-hal.h"

esp_pm_config_esp32_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,
    .light_sleep_enable = true};

// #define configGENERATE_RUN_TIME_STATS 1;
// #define configUSE_STATS_FORMATTING_FUNCTIONS 1;
//  static const char *TAG = "i2c-example";
#define I2C_SLAVE_ADDR 0x36
#define TIMEOUT_MS 1000
#define DELAY_MS 1000
#define MPU9250_WHO_AM_I_REG_ADDR 0x1C

#define BUSYIO GPIO_NUM_25
#define RESETIO GPIO_NUM_27

max1730x BMS(I2C_NUM_0);

static const char *TAG = "wifi station_main";

int num_bytes = 5;
char *sendbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
char *recvbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
esp_err_t ret;
spi_device_handle_t handle;

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void);

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone(void);

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout(void);

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError(IrqErrorCode_t);

/*!
 * \brief All the callbacks are stored in a structure
 */
RadioCallbacks_t callbacks =
    {
        NULL, // txDone
        NULL, // rxDone
        NULL, // syncWordDone
        NULL, // headerDone
        NULL, // txTimeout
        NULL, // rxTimeout
        NULL, // rxError
        NULL, // rangingDone
        NULL, // cadDone
};

void spi_init()
{
    ESP_LOGI(TAG, "Initializing bus SPI...");

    spi_bus_config_t buscfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    // Initialize the SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Attach to main flash bus...");
}

void Stats_task(void *pvParameter)
{
    while (1)
    {
        char buf[2048];
        vTaskGetRunTimeStats(buf);
        printf("\n");
        printf("Task\t Abs Time \t Time\n");
        printf(buf);
        printf("\n");

        vTaskList(buf);
        printf("Name\t\tState Priority Stack\tNum\n");
        printf(buf);
        printf("\n");

        fflush(stdout);
        vTaskDelay(10000 / portTICK_RATE_MS);
    }

    /*
    printf("Hello world!\n");
    for (int i = 10; i >= 0; i--)
    {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();*/
}

void SPI_test_task(void *pvParameter)
{
    while (1)
    {
        spi_device_interface_config_t devcfg = {
            .command_bits = 8,
            .address_bits = 0,
            .dummy_bits = 0,
            .mode = 0,
            .duty_cycle_pos = 128,
            .clock_speed_hz = 80000,
            .spics_io_num = 5,
            .queue_size = 3,
        };

        spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
        // 55 57 57 57 0A 4D 34 68 13 63 Printed.
        int num_bytes = 10;
        // char *sendbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
        // char *recvbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
        char sendbuf[15];
        char recvbuf[15];
        for (int x = 0; x < num_bytes; x++)
        {
            sendbuf[x] = 0x00;
            recvbuf[x] = 0x00;
        }
        // sendbuf[0] = 0x19;
        // sendbuf[1] = 0x08;
        // sendbuf[2] = 0x9E;
        sendbuf[0] = 0x19;
        sendbuf[0] = 0x08;
        sendbuf[1] = 0x9E;

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        // t.length = 10 * 8;
        t.length = 8;
        t.tx_buffer = sendbuf;
        t.rx_buffer = recvbuf;
        t.addr = 0x00;
        t.cmd = 0x19;

        printf("Transmitting %d bytes...\n", num_bytes);
        ret = spi_device_transmit(handle, &t);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("Transmitted..\n");
        for (int i = 0; i < 32; i++)
        {
            if (i + 0 < num_bytes)
            {
                printf("%02X ", recvbuf[0 + i]);
            }
        }
        printf("Printed..\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
        // free(sendbuf);
        // free(recvbuf);
    }
}
void read_status(void)
{
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 128,
        .clock_speed_hz = 80000,
        .spics_io_num = 5,
        .queue_size = 3,
    };

    spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    int num_bytes = 10;
    // char *sendbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
    // char *recvbuf = (char *)heap_caps_malloc((num_bytes + 3) & (~3), MALLOC_CAP_DMA);
    char sendbuf[15];
    char recvbuf[15];
    for (int x = 0; x < num_bytes; x++)
    {
        sendbuf[x] = 0x00;
        recvbuf[x] = 0x00;
    }
     sendbuf[0] = 0xC0;
     sendbuf[1] = 0x00;
     sendbuf[2] = 0x00;
    //sendbuf[0] = 0x19;
    //sendbuf[0] = 0x08;
    //sendbuf[1] = 0x9E;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    // t.length = 10 * 8;
    t.length = 2*8;
    t.tx_buffer = sendbuf;
    t.rx_buffer = recvbuf;
    t.addr = 0x00;
    t.cmd = 0x19;

    printf("Transmitting %d bytes...\n", num_bytes);
    ret = spi_device_transmit(handle, &t);
    vTaskDelay(1000 / portTICK_RATE_MS);
    printf("Transmitted..\n");
    for (int i = 0; i < 32; i++)
    {
        if (i + 0 < num_bytes)
        {
            printf("%02X ", recvbuf[0 + i]);
        }
    }
    printf("Printed..\n");
}

extern "C" void app_main()
{
    periph_init();
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    ets_timer_init();
    // xTaskCreate(&Stats_task, "stats_task", 10240, NULL, 4, NULL);
    vTaskDelay(4000 / portTICK_RATE_MS);
    BMS.begin();

    spi_init();

   /* vTaskDelay(20 / portTICK_RATE_MS);
    gpio_set_direction(RESETIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(RESETIO, 0);

    vTaskDelay(50 / portTICK_RATE_MS);
    gpio_set_level(RESETIO, 1);
    vTaskDelay(20 / portTICK_RATE_MS);

    //  xTaskCreate(&SPI_test_task, "spi_test_task", 10240, NULL, 3, NULL);
    for (int i = 0; i < 15; i++)
    {
        read_status();
    }*/

    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    RadioStatus_t RadStatus;
    SX1280Hal Radio(handle, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_27, &callbacks);

    Radio.Init();
    vTaskDelay(1500 / portTICK_RATE_MS);
    RadStatus = Radio.GetStatus();
    printf("status Value: %x \n", RadStatus.Value);

    for (int i = 0; i < 15; i++)
    {
        vTaskDelay(100 / portTICK_RATE_MS);
        //if(i==10){ Radio.SetFs();}
        RadStatus = Radio.GetStatus();
        printf("status Value: %x \n", RadStatus.Value);
    }
    /*
    for (int i = 0; i < 15; i++)
    {
        read_status();
    }*/
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    mqtt_app_start();
    // while (1);
    return;
}
