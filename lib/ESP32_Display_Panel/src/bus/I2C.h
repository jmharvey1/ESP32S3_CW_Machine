/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lcd_panel_io.h"
// #include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "host/ESP_PanelHost.h"
#include "ESP_PanelBus.h"

/**
 * @brief I2C bus object class
 *
 * @note  This class is a derived class of `ESP_PanelBus`, user can use it directly
 */
class ESP_PanelBus_I2C: public ESP_PanelBus
{
public:
    /**
     * @brief Construct a I2C bus object in a common way, the host will be initialized by the driver
     *
     * @note  The `init()` function should be called after this function
     *
     * @param scl_io    I2C SCL pin
     * @param sda_io    I2C SDA pin
     * @param io_config I2C panel IO configuration
     */
    /*JMH Commentedt out*/
    // ESP_PanelBus_I2C(int scl_io, int sda_io, const esp_lcd_panel_io_i2c_config_t &io_config);

    /**
     * @brief Construct a I2C bus object in a complex way, the host will be initialized by the driver
     *
     * @note  The `init()` function should be called after this function
     *
     * @param host_config I2C host configuration
     * @param io_config   I2C panel IO configuration
     * @param host_id   I2C host ID, default is `ESP_PANEL_HOST_I2C_ID_DEFAULT`
     */
    /*JMH modified to support current I2C driver - this executes when ESP_Panel::init(void) runs line: lib/ESP32_Display_Panel/src/ESP_Panel.cpp:395 */
    //ESP_PanelBus_I2C(void);
    ESP_PanelBus_I2C(const i2c_master_bus_config_t &host_config, const esp_lcd_panel_io_i2c_config_t &io_config,
                     i2c_port_t host_id = ESP_PANEL_HOST_I2C_ID_DEFAULT);

    /**
     * @brief Construct a I2C bus object in a complex way, the host needs to be initialized by the user
     *
     * @note  The `init()` function should be called after this function
     *
     * @param io_config I2C panel IO configuration
     * @param host_id   I2C host ID, default is `ESP_PANEL_HOST_I2C_ID_DEFAULT`
     */
    //ESP_PanelBus_I2C(const esp_lcd_panel_io_i2c_config_t &io_config, i2c_port_t host_id = ESP_PANEL_HOST_I2C_ID_DEFAULT);
    //ESP_PanelBus_I2C(i2c_port_t host_id = ESP_PANEL_HOST_I2C_ID_DEFAULT);
    
    

    /**
     * @brief Destroy the I2C bus object
     *
     */
    ~ESP_PanelBus_I2C() override;

    /**
     * @brief Here are some functions to configure the I2C bus object. These functions should be called before `begin()`
     *
     */
    void configI2cPullupEnable(bool sda_pullup_en, bool scl_pullup_en);
    void configI2cFreqHz(uint32_t hz);
    void configI2cAddress(uint32_t address);
    void configI2cCtrlPhaseBytes(uint32_t num);
    void configI2cDcBitOffset(uint32_t num);
    void configI2cCommandBits(uint32_t num);
    void configI2cParamBits(uint32_t num);
    void configI2cFlags(bool dc_low_on_data, bool disable_control_phase);
    
    /*JMH ADDED*/
    void SetupMasterI2C(void);

    /**
     * @brief Startup the bus
     *
     * @note  This function should be called after `init()`
     *
     * @return true if success, otherwise false
     */
    bool begin(void) override;

private:
    /**
     * @brief I2C initialization parameters
     */

    // typedef struct{
    //     i2c_mode_t mode;     /*!< I2C mode */
    //     int sda_io_num;      /*!< GPIO number for I2C sda signal */
    //     int scl_io_num;      /*!< GPIO number for I2C scl signal */
    //     bool sda_pullup_en;  /*!< Internal GPIO pull mode for I2C sda signal*/
    //     bool scl_pullup_en;  /*!< Internal GPIO pull mode for I2C scl signal*/

    //     union {
    //         struct {
    //             uint32_t clk_speed;      /*!< I2C clock frequency for master mode, (no higher than 1MHz for now) */
    //         } master;                    /*!< I2C master config */
    // #if SOC_I2C_SUPPORT_SLAVE
    //         struct {
    //             uint8_t addr_10bit_en;   /*!< I2C 10bit address mode enable for slave mode */
    //             uint16_t slave_addr;     /*!< I2C address for slave mode */
    //             uint32_t maximum_speed;  /*!< I2C expected clock speed from SCL. */
    //         } slave;                     /*!< I2C slave config */
    // #endif // SOC_I2C_SUPPORT_SLAVE
    //     };
    //     uint32_t clk_flags;              /*!< Bitwise of ``I2C_SCLK_SRC_FLAG_**FOR_DFS**`` for clk source choice*/
    // } i2c_config_t;
    /*JMH Removed*/
    // i2c_config_t host_config;

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
    /*JMH Added for reference*/
    // typedef struct {
    //     uint32_t dev_addr; /*!< I2C device address */
    //     esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done; /*!< Callback invoked when color data transfer has finished */
    //     void *user_ctx; /*!< User private data, passed directly to on_color_trans_done's user_ctx */
    //     size_t control_phase_bytes; /*!< I2C LCD panel will encode control information (e.g. D/C selection) into control phase, in several bytes */
    //     unsigned int dc_bit_offset; /*!< Offset of the D/C selection bit in control phase */
    //     int lcd_cmd_bits;           /*!< Bit-width of LCD command */
    //     int lcd_param_bits;         /*!< Bit-width of LCD parameter */
    //     struct {
    //         unsigned int dc_low_on_data: 1;  /*!< If this flag is enabled, DC line = 0 means transfer data, DC line = 1 means transfer command; vice versa */
    //         unsigned int disable_control_phase: 1; /*!< If this flag is enabled, the control phase isn't used */
    //     } flags; /*!< Extra flags to fine-tune the I2C device */
    //     uint32_t scl_speed_hz; /*!< I2C LCD SCL frequency (hz) */
    // } esp_lcd_panel_io_i2c_config_t;
    /*for GT911 setting are:
    { .dev_addr = (0x5D), 
        .control_phase_bytes = 1, 
        .dc_bit_offset = 0, .
        lcd_cmd_bits = 16, 
        .flags = { .disable_control_phase = 1, } 
    }*/
    esp_lcd_panel_io_i2c_config_t io_config;
    
    /*JMH Add the following 2 lines/properties*/
    i2c_master_bus_handle_t i2c_master_bus_handle = NULL;
    i2c_master_dev_handle_t dev_handle = NULL;
};
