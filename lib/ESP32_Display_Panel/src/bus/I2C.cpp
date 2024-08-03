/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include "ESP_PanelLog.h"
#include "I2C.h"

static const char *TAG = "ESP_PanelBus_I2C";
/*JMH Commentedt out*/
// ESP_PanelBus_I2C::ESP_PanelBus_I2C(int scl_io, int sda_io, const esp_lcd_panel_io_i2c_config_t &io_config):
//     ESP_PanelBus((int)ESP_PANEL_HOST_I2C_ID_DEFAULT, ESP_PANEL_BUS_TYPE_I2C, true),
//     host_config((i2c_config_t)ESP_PANEL_HOST_I2C_CONFIG_DEFAULT(scl_io, sda_io)),
//     io_config(io_config)
// {
// }

// ESP_PanelBus_I2C::ESP_PanelBus_I2C(void)
// {
//     //,
//     // i2c_mst_config(host_config),
//     // io_config(io_config)
// }



ESP_PanelBus_I2C::ESP_PanelBus_I2C(const i2c_master_bus_config_t &host_config, const esp_lcd_panel_io_i2c_config_t &io_config,
                                   i2c_port_t host_id):
        ESP_PanelBus((int)ESP_PANEL_HOST_I2C_ID_DEFAULT, ESP_PANEL_BUS_TYPE_I2C, true),                                   
        i2c_mst_config(host_config),
        io_config(io_config)                               
{
//                           
}
// /* - this executes when ESP_Panel::init(void) runs line: lib/ESP32_Display_Panel/src/ESP_Panel.cpp:395*/
// ESP_PanelBus_I2C::ESP_PanelBus_I2C(const esp_lcd_panel_io_i2c_config_t &io_config, i2c_port_t host_id):
//     ESP_PanelBus((int)host_id, ESP_PANEL_BUS_TYPE_I2C, false),
//     io_config(io_config)
// {
// }
// ESP_PanelBus_I2C::ESP_PanelBus_I2C(i2c_port_t host_id):
//     ESP_PanelBus((int)host_id, ESP_PANEL_BUS_TYPE_I2C, false)
// {
// }


ESP_PanelBus_I2C::~ESP_PanelBus_I2C()
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    if (handle == NULL) {
        goto end;
    }

    if (!del()) {
        ESP_LOGE(TAG, "Delete panel io failed");
    }

    if (host_need_init) {

        if (i2c_del_master_bus(i2c_master_bus_handle) != ESP_OK) {
        //if (i2c_driver_delete((i2c_port_t)host_id) != ESP_OK) {
            ESP_LOGE(TAG, "Delete host[%d] driver failed", host_id);
        } else {
            ESP_LOGD(TAG, "Delete host[%d] driver", host_id);
        }
    }

end:
    ESP_LOGD(TAG, "Destory");
}

void ESP_PanelBus_I2C::configI2cPullupEnable(bool sda_pullup_en, bool scl_pullup_en)
{
    //host_config.sda_pullup_en = sda_pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    //host_config.scl_pullup_en = scl_pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    i2c_mst_config.flags.enable_internal_pullup = sda_pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
}

void ESP_PanelBus_I2C::configI2cFreqHz(uint32_t hz)
{
    //host_config.master.clk_speed = hz;
    dev_cfg.scl_speed_hz = hz;
}

void ESP_PanelBus_I2C::configI2cAddress(uint32_t address)
{
    /*JMH not sure which is needed with current/new I2C driver; both are active & support adress entry*/
    /*legacy driver*/
    io_config.dev_addr = address;
    /*JMH added */
    //dev_cfg.device_address = address;
}

void ESP_PanelBus_I2C::configI2cCtrlPhaseBytes(uint32_t num)
{
    io_config.control_phase_bytes = num;
}

void ESP_PanelBus_I2C::configI2cDcBitOffset(uint32_t num)
{
    io_config.dc_bit_offset = num;
}

void ESP_PanelBus_I2C::configI2cCommandBits(uint32_t num)
{
    io_config.lcd_cmd_bits = num;
}

void ESP_PanelBus_I2C::configI2cParamBits(uint32_t num)
{
    io_config.lcd_param_bits = num;
}

void ESP_PanelBus_I2C::configI2cFlags(bool dc_low_on_data, bool disable_control_phase)
{
    io_config.flags.dc_low_on_data = dc_low_on_data;
    io_config.flags.disable_control_phase = disable_control_phase;
}
/*JMH Add the following method, to support new/current I2C driver model */
void ESP_PanelBus_I2C::SetupMasterI2C(void)
{
    printf("i2c_mst_config.clk_source: %d\n", (int)i2c_mst_config.clk_source);
    printf("i2c_mst_config.i2c_port: %d\n", (int)i2c_mst_config.i2c_port);
    printf("i2c_mst_config.scl_io_num: %d\n", (int)i2c_mst_config.scl_io_num);
    printf("i2c_mst_config.sda_io_num: %d\n", (int)i2c_mst_config.sda_io_num);
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &this->i2c_master_bus_handle));
    if(this->i2c_master_bus_handle != NULL)  printf("i2c_master_bus_handle: %d\n", (int)this->i2c_master_bus_handle);
    else  printf("ERROR: i2c_master_bus_handle is NULL !!!\n");
    ESP_ERROR_CHECK(i2c_master_bus_add_device(this->i2c_master_bus_handle, &dev_cfg, &this->dev_handle));
}
bool ESP_PanelBus_I2C::begin(void)
{
    ESP_PANEL_ENABLE_TAG_DEBUG_LOG();

    if(host_need_init) { //JMH -for ESP32Waveshare display, this will be true
        // ESP_PANEL_CHECK_ERR_RET(i2c_param_config((i2c_port_t)host_id, &host_config), false, "Configure host[%d] failed", host_id);
        // ESP_PANEL_CHECK_ERR_RET(i2c_driver_install((i2c_port_t)host_id, host_config.mode, 0, 0, 0), false,
        //                         "Install host[%d] failed", host_id);
        SetupMasterI2C(); //JMH add to support new/current I2C driver model
        ESP_LOGD(TAG, "Init host[%d]", (int)host_id);
    }

    // ESP_PANEL_CHECK_ERR_RET(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)host_id, &io_config, &handle), false,
    //                         "Create panel io failed");
    /*  !!!!!*********** VERY IMPORTANT FIX ***********!!!!!  */
    /*JMH created, as a new work around, for -Generic macro failling to compile */
    printf("ESP_PanelBus_I2C::begin() io_config.scl_speed_hz: %d\n", (int)io_config.scl_speed_hz);
    printf("ESP_PanelBus_I2C::begin() io_config.dev_addr: %d\n", (int)io_config.dev_addr);
    ESP_PANEL_CHECK_ERR_RET(esp_lcd_new_panel_io_i2c_v2(this->i2c_master_bus_handle, &this->io_config, &this->handle), false,
                            "Create panel io failed");
    /*JMH created, as a old work around, for -Generic macro failling to compile */
    // i2c_master_bus_handle_t host_idJMH = (i2c_master_bus_handle_t)host_id;
    // ESP_PANEL_CHECK_ERR_RET(esp_lcd_new_panel_io_i2c_v2(host_idJMH, &io_config, &handle), false,
    //                         "Create panel 'V2' io failed");                        
    // ESP_PANEL_CHECK_ERR_RET(esp_lcd_new_panel_io_i2c_v1((esp_lcd_i2c_bus_handle_t)host_id, &io_config, &handle), false,
    //                         "Create panel 'V1' io failed");                        
    ESP_LOGD(TAG, "Panel IO @%p created", handle);

    return true;
}
