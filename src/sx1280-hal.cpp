/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2016 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Matthieu Verdy
*/
#include "sx1280-hal.h"
#include "freertos/task.h"
#define BUSYIO GPIO_NUM_25
#define RESETIO GPIO_NUM_27

/*!
 * \brief Helper macro to avoid duplicating code for setting dio pins parameters
 */
#define DioAssignCallback(dio, callback)                              \
    if (dio != -1)                                                    \
    {                                                                 \
        gpio_isr_handler_add(dio, (gpio_isr_t)callback, (void *)dio); \
    }
/*!
 * \brief Used to block execution waiting for low state on radio busy pin.
 *        Essentially used in SPI communications
 */
#define WaitOnBusy()                       \
    while (gpio_get_level(BUSYIO) == 1)    \
    {                                      \
        vTaskDelay(10 / portTICK_RATE_MS); \
    }

// This code handles cases where assert_param is undefined
#ifndef assert_param
#define assert_param(...)
#endif

SX1280Hal::SX1280Hal(spi_device_handle_t spi_handle, gpio_num_t busy, gpio_num_t dio1, gpio_num_t dio2, gpio_num_t dio3, gpio_num_t rst, RadioCallbacks_t *callbacks)
    : SX1280(callbacks)
{
    printf("\nConstructor\n");
    RadioSpi = spi_handle;
    BUSY = busy;
    DIO1 = dio1;
    DIO2 = dio2;
    DIO3 = dio3;
    RadioReset = rst;
    SpiInit();
}

SX1280Hal::~SX1280Hal(void){

};

void SX1280Hal::SpiInit(void)
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
    printf("\nSPI_ initialized\n");
    spi_bus_add_device(SPI2_HOST, &devcfg, &RadioSpi);
    memset(&SpiTrans, 0, sizeof(SpiTrans));
    SpiTrans.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
}

void SX1280Hal::IoIrqInit(DioIrqHandler irqHandler)
{
    gpio_set_pull_mode(BUSYIO, GPIO_FLOATING);
    DioAssignCallback(DIO1, irqHandler);
    DioAssignCallback(DIO2, irqHandler);
    DioAssignCallback(DIO3, irqHandler);
}

void SX1280Hal::Reset(void)
{
    vTaskDelay(20 / portTICK_RATE_MS);
    gpio_set_direction(RESETIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(RESETIO, 0);

    vTaskDelay(50 / portTICK_RATE_MS);
    gpio_set_level(RESETIO, 1);
    vTaskDelay(20 / portTICK_RATE_MS);
}

void SX1280Hal::Wakeup(void)
{
    //__disable_irq();

    // Don't wait for BUSY here

    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)RADIO_GET_STATUS,
            .addr = 0x00,
            .length = 0,
            .rxlength = 0,
            .user = NULL,
            .tx_buffer = NULL,
            .rx_buffer = NULL,
        },
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);

    // Wait for chip to be ready.
    WaitOnBusy();

    // __enable_irq();
}

void SX1280Hal::WriteCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();

    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)command,
            .length = (size_t)(size * 8),
            .tx_buffer = buffer,
            .rx_buffer = NULL,
        },
        .command_bits = 8,
        .address_bits = 0,
        .dummy_bits = 0,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
}

void SX1280Hal::ReadCommand(RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();

    if (command == RADIO_GET_STATUS)
    {
        SpiTrans = (spi_transaction_ext_t){
            .base = {
                .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
                .cmd = (u_int8_t)RADIO_GET_STATUS,
                .addr = 0x00,
                .length = 8,
                .rxlength = 0,
                .user = NULL,
                .tx_buffer = &command,
                .rx_buffer = buffer,
            },
            .command_bits = 0,
            .address_bits = 0,
            .dummy_bits = 0,
        };
    }
    else
    {
        SpiTrans = (spi_transaction_ext_t){
            .base = {
                .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
                .cmd = (u_int8_t)command,
                .length = (size_t)(size * 8),
                .tx_buffer = NULL,
                .rx_buffer = buffer,
            },
            .command_bits = 8,
            .address_bits = 0,
            .dummy_bits = 8,
        };
    }
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
    for (int i = 0; i < 32; i++)
    {
        if (i + 0 < size)
        {
            printf("%02X ", buffer[0 + i]);
        }
    }
}

void SX1280Hal::WriteRegister(uint16_t address, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();
    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)RADIO_WRITE_REGISTER,
            .addr = address,
            .length = (size_t)(size * 8),
            .tx_buffer = buffer,
            .rx_buffer = NULL,
        },
        .command_bits = 8,
        .address_bits = 16,
        .dummy_bits = 0,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
}

void SX1280Hal::WriteRegister(uint16_t address, uint8_t value)
{
    WriteRegister(address, &value, 1);
}

void SX1280Hal::ReadRegister(uint16_t address, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();

    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)RADIO_READ_REGISTER,
            .addr = address,
            .length = (size_t)(size * 8),
            .tx_buffer = NULL,
            .rx_buffer = buffer,
        },
        .command_bits = 8,
        .address_bits = 16,
        .dummy_bits = 0,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
}

uint8_t SX1280Hal::ReadRegister(uint16_t address)
{
    uint8_t data;

    ReadRegister(address, &data, 1);
    return data;
}

void SX1280Hal::WriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    WaitOnBusy();

    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)RADIO_WRITE_BUFFER,
            .addr = offset,
            .length = (size_t)(size * 8),
            .tx_buffer = buffer,
            .rx_buffer = NULL,
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 0,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
}

void SX1280Hal::ReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    WaitOnBusy();

    SpiTrans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY,
            .cmd = (u_int8_t)RADIO_READ_BUFFER,
            .addr = offset,
            .length = (size_t)(size * 8),
            .tx_buffer = NULL,
            .rx_buffer = buffer,
        },
        .command_bits = 8,
        .address_bits = 8,
        .dummy_bits = 8,
    };
    spi_device_transmit(RadioSpi, (spi_transaction_t *)&SpiTrans);
}

uint8_t SX1280Hal::GetDioStatus(void)
{
    return (gpio_get_level(DIO3) << 3) | (gpio_get_level(DIO2) << 2) | (gpio_get_level(DIO1) << 1) | (gpio_get_level(BUSYIO) << 0);
}
