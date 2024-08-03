/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include "ESP_PanelLog.h"
#include "ESP_PanelHost.h"
#include "touch/base/esp_lcd_touch.h"
#include "bus/I2C.h"

using namespace std;

static const char *TAG = "ESP_PanelHost";

i2c_port_t host_id = ESP_PANEL_HOST_I2C_ID_DEFAULT;
//ESP_PanelBus_I2C *PnlBus_I2C = new ESP_PanelBus_I2C(host_id);

ESP_PanelHost::ESP_PanelHost()
{
}

ESP_PanelHost::~ESP_PanelHost()
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    ESP_LOGD(TAG, "Destory");
}
/*JMH Commented out*/
// bool ESP_PanelHost::addHostI2C(const i2c_config_t &host_config, i2c_port_t host_id)
// {
//     ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

//     auto ret = _i2c_host_config_map.find(host_id);
//     if (ret == _i2c_host_config_map.end()) {
//         _i2c_host_config_map.insert(pair<i2c_port_t, i2c_config_t>(host_id, host_config));
//         ESP_LOGD(TAG, "Add host I2C[%d]", (int)host_id);

//         return true;
//     }
//     ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(i2c_config_t)), false,
//                               "Host I2C[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
//     ESP_LOGD(TAG, "Host I2C[%d] is already exist", (int)host_id);

//     return true;
// }
/*JMH ADDED*/
bool ESP_PanelHost::addHostI2C(const esp_lcd_touch_config_t &host_config, i2c_port_t host_id)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    auto ret = _i2c_host_config_map.find(host_id);
    if (ret == _i2c_host_config_map.end()) {
       // _i2c_host_config_map.insert(pair<i2c_port_t, i2c_master_bus_config_t>(host_id, host_config));
        _i2c_host_config_map.insert({host_id, host_config});
        ESP_LOGD(TAG, "Add host I2C[%d]", (int)host_id);

        return true;
    }
    ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(i2c_master_bus_config_t)), false,
                              "Host I2C[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
    ESP_LOGD(TAG, "Host I2C[%d] is already exist", (int)host_id);

    return true;
}
// bool ESP_PanelHost::addHostI2C(int scl_io, int sda_io, i2c_port_t host_id)
// {
//     ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

//     i2c_config_t host_config = ESP_PANEL_HOST_I2C_CONFIG_DEFAULT(scl_io, sda_io);

//     auto ret = _i2c_host_config_map.find(host_id);
//     if (ret == _i2c_host_config_map.end()) {
//         _i2c_host_config_map.insert(pair<i2c_port_t, i2c_config_t>(host_id, host_config));
//         ESP_LOGD(TAG, "Add host I2C[%d]", (int)host_id);

//         return true;
//     }
//     ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(i2c_config_t)), false,
//                               "Host I2C[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
//     ESP_LOGD(TAG, "Host I2C[%d] is already exist", (int)host_id);

//     return true;
// }

bool ESP_PanelHost::addHostSPI(const spi_bus_config_t &host_config, spi_host_device_t host_id)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    auto ret = _spi_host_config_map.find(host_id);
    if (ret == _spi_host_config_map.end()) {
        _spi_host_config_map.insert(pair<spi_host_device_t, spi_bus_config_t>(host_id, host_config));
        ESP_LOGD(TAG, "Add host SPI[%d]", (int)host_id);

        return true;
    }
    ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(spi_bus_config_t)), false,
                              "Host SPI[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
    ESP_LOGD(TAG, "Host SPI[%d] is already exist", (int)host_id);

    return true;
}

bool ESP_PanelHost::addHostSPI(int sck_io, int sda_io, int sdo_io, spi_host_device_t host_id)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    spi_bus_config_t host_config = ESP_PANEL_HOST_SPI_CONFIG_DEFAULT(sck_io, sda_io, sdo_io);

    auto ret = _spi_host_config_map.find(host_id);
    if (ret == _spi_host_config_map.end()) {
        _spi_host_config_map.insert(pair<spi_host_device_t, spi_bus_config_t>(host_id, host_config));
        ESP_LOGD(TAG, "Add host SPI[%d]", (int)host_id);

        return true;
    }
    ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(spi_bus_config_t)), false,
                              "SPI[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
    ESP_LOGD(TAG, "Host SPI[%d] is already exist", (int)host_id);

    return true;
}

bool ESP_PanelHost::addHostQSPI(const spi_bus_config_t &host_config, spi_host_device_t host_id)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    auto ret = _spi_host_config_map.find(host_id);
    if (ret == _spi_host_config_map.end()) {
        _spi_host_config_map.insert(pair<spi_host_device_t, spi_bus_config_t>(host_id, host_config));
        ESP_LOGD(TAG, "Add host SPI[%d]", (int)host_id);

        return true;
    }
    ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(spi_bus_config_t)), false,
                              "Host SPI[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
    ESP_LOGD(TAG, "Host SPI[%d] is already exist", (int)host_id);

    return true;
}

bool ESP_PanelHost::addHostQSPI(int sck_io, int d0_io, int d1_io, int d2_io, int d3_io, spi_host_device_t host_id)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    spi_bus_config_t host_config = ESP_PANEL_HOST_QSPI_CONFIG_DEFAULT(sck_io, d0_io, d1_io, d2_io, d3_io);

    auto ret = _spi_host_config_map.find(host_id);
    if (ret == _spi_host_config_map.end()) {
        _spi_host_config_map.insert(pair<spi_host_device_t, spi_bus_config_t>(host_id, host_config));
        ESP_LOGD(TAG, "Add host SPI[%d]", (int)host_id);

        return true;
    }
    ESP_PANEL_CHECK_FALSE_RET(!memcmp(&ret->second, &host_config, sizeof(spi_bus_config_t)), false,
                              "SPI[%d] is already exist and attempt to add with a different configuartion", (int)host_id);
    ESP_LOGD(TAG, "Host SPI[%d] is already exist", (int)host_id);

    return true;
}

bool ESP_PanelHost::begin(void)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    // Initialize all I2C hosts
    ESP_LOGD(TAG, "Initialize host I2C[%d]", 0);
    // if (_i2c_host_config_map.size() > 0) {
    //     for (auto &it : _i2c_host_config_map) {
    //         //ESP_PANEL_CHECK_ERR_RET(i2c_param_config(it.first, &it.second), false, "I2C[%d] config param failed", it.first);
    //         //ESP_PANEL_CHECK_ERR_RET(i2c_driver_install(it.first, it.second.mode, 0, 0, 0), false, "I2C[%d] install driver failed",
    //         //                        it.first);
            
    //     }
    // }

    // Initialize all SPI hosts
    if (_spi_host_config_map.size() > 0) {
        for (auto &it : _spi_host_config_map) {
            ESP_PANEL_CHECK_ERR_RET(spi_bus_initialize(it.first, &it.second, SPI_DMA_CH_AUTO), false, "SPI[%d] initialize failed",
                                    it.first);
            ESP_LOGD(TAG, "Initialize host SPI[%d]", (int)it.first);
        }
    }

    return true;
}
// /*JMH Add the following method */
// void ESP_PanelHost::SetupMasterI2C(gpio_num_t scl_io, gpio_num_t sda_io)
// {
//     i2c_mst_config = ESP_PANEL_I2C_MASTER_BUS_CONFIG_DEFAULT(scl_io, sda_io);
//     dev_cfg = ESP_PANEL_I2C_DEVICE_CONFIG_DEFAULT(ESP_PANEL_TOUCH_I2C_ADDRESS);
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_master_bus_handle));
//     ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_master_bus_handle, &dev_cfg, &dev_handle));
// }
