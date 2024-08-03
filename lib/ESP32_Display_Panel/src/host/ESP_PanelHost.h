/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>
//#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"
#include "touch/base/esp_lcd_touch.h" //JMH ADDED

/*
 * I2C Host Default Configuration
 *
 */

#define ESP_PANEL_HOST_I2C_ID_DEFAULT (I2C_NUM_0)

/*JMH Legecy Driver entry; No longer needed*/
/*
#define ESP_PANEL_HOST_I2C_CONFIG_DEFAULT(scl_io, sda_io)       \
    {                                                           \
        .mode = I2C_MODE_MASTER,                                \
        .sda_io_num = sda_io,                                   \
        .scl_io_num = scl_io,                                   \
        .sda_pullup_en = GPIO_PULLUP_ENABLE,                    \
        .scl_pullup_en = GPIO_PULLUP_ENABLE,                    \
        .master = {                                             \
            .clk_speed = 400000,                                \
        },                                                      \
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,               \
    }
*/    
//i2c_master_bus_config_t i2c_mst_config = {
/*JMH ADD Note: I assumed port 0*/
#define ESP_PANEL_I2C_MASTER_BUS_CONFIG_DEFAULT(scl_io, sda_io) \
    {                                                           \
    .i2c_port = I2C_NUM_0,                                      \
    .sda_io_num = sda_io,                                       \
    .scl_io_num = scl_io,                                       \
    .clk_source = I2C_CLK_SRC_DEFAULT,                          \
    .glitch_ignore_cnt = 7,                                     \
    .intr_priority = 0,                                         \
    .trans_queue_depth = 4,                                    \
    .flags = {                                                  \
    .enable_internal_pullup = true,                             \
    },                                                          \
    }

/* i2c_device_config_t dev_cfg = { */
/*JMH ADD*/
#define ESP_PANEL_I2C_DEVICE_CONFIG_DEFAULT(dev_addr)           \
    {                                                           \
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,                      \
    .device_address = dev_addr,                                 \
    .scl_speed_hz =  400000,                                    \
}
/**
 * SPI & QSPI Host Default Configuration
 *
 */
#define ESP_PANEL_HOST_SPI_ID_DEFAULT (SPI2_HOST)
/* Refer to `hal/spi_ll.h` in SDK (ESP-IDF) */
#ifdef CONFIG_IDF_TARGET_ESP32
#define ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE   ((1 << 24) >> 3)
#elif CONFIG_IDF_TARGET_ESP32S2
#define ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE   ((1 << 23) >> 3)
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
#define ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE   ((1 << 18) >> 3)
#endif
#define ESP_PANEL_HOST_SPI_CONFIG_DEFAULT(sck_io, sda_io, sdo_io) \
    {                                                     \
        .mosi_io_num = sda_io,                            \
        .miso_io_num = sdo_io,                            \
        .sclk_io_num = sck_io,                            \
        .quadwp_io_num = -1,                              \
        .quadhd_io_num = -1,                              \
        .data4_io_num = -1,                               \
        .data5_io_num = -1,                               \
        .data6_io_num = -1,                               \
        .data7_io_num = -1,                               \
        .max_transfer_sz = ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE, \
        .flags = SPICOMMON_BUSFLAG_MASTER,                \
        .intr_flags = 0,                                  \
    }
#define ESP_PANEL_HOST_QSPI_CONFIG_DEFAULT(sck_io, d0_io, d1_io, d2_io, d3_io) \
    {                                             \
        .data0_io_num = d0_io,                    \
        .data1_io_num = d1_io,                    \
        .sclk_io_num = sck_io,                    \
        .data2_io_num = d2_io,                    \
        .data3_io_num = d3_io,                    \
        .data4_io_num = -1,                       \
        .data5_io_num = -1,                       \
        .data6_io_num = -1,                       \
        .data7_io_num = -1,                       \
        .max_transfer_sz = ESP_PANEL_HOST_SPI_MAX_TRANSFER_SIZE, \
        .flags = SPICOMMON_BUSFLAG_MASTER,        \
        .intr_flags = 0,                          \
    }

class ESP_PanelHost {
public:
    ESP_PanelHost();
    ~ESP_PanelHost();
    /*JMH Commented out*/
    // bool addHostI2C(const i2c_config_t &host_config, i2c_port_t host_id);
    /*JMH Added to support current I2C driver*/
    bool addHostI2C(const esp_lcd_touch_config_t &host_config, i2c_port_t host_id);
    // bool addHostI2C(int scl_io, int sda_io, i2c_port_t host_id);

    bool addHostSPI(const spi_bus_config_t &host_config, spi_host_device_t host_id);
    bool addHostSPI(int sck_io, int sda_io, int sdo_io, spi_host_device_t host_id);

    bool addHostQSPI(const spi_bus_config_t &host_config, spi_host_device_t host_id);
    bool addHostQSPI(int sck_io, int d0_io, int d1_io, int d2_io, int d3_io, spi_host_device_t host_id);

    bool begin(void);
    /*JMH ADDED*/
    //void SetupMasterI2C(gpio_num_t scl_io, gpio_num_t sda_io);

private:
    //std::map<i2c_port_t, i2c_config_t> _i2c_host_config_map;
    std::map<i2c_port_t, esp_lcd_touch_config_t> _i2c_host_config_map;
    std::map<spi_host_device_t, spi_bus_config_t> _spi_host_config_map;
        // i2c_master_bus_config_t i2c_mst_config = {
    //     .clk_source = I2C_CLK_SRC_DEFAULT,
    //     .i2c_port = TEST_I2C_PORT,
    //     .scl_io_num = I2C_MASTER_SCL_IO,
    //     .sda_io_num = I2C_MASTER_SDA_IO,
    //     .glitch_ignore_cnt = 7,
    //     .flags.enable_internal_pullup = true,
    // };
    /*JMH Added to support new/current I2C driver*/
    i2c_master_bus_config_t i2c_mst_config;
    // i2c_device_config_t dev_cfg = {
    //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,     /*!< Select the address length of the slave device. */
    //     .device_address = 0x58,                    /*!< I2C device raw address. (The 7/10 bit address without read/write bit) */
    //     .scl_speed_hz = 100000,                    /*!< I2C SCL line frequency. */
    // };
    /*JMH Added to support new/current I2C driver*/
    i2c_device_config_t dev_cfg;
    /*JMH Add the following 2 lines/properties*/
    i2c_master_bus_handle_t i2c_master_bus_handle;
    i2c_master_dev_handle_t dev_handle;

};
