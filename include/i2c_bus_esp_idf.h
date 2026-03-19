/*20251222 JMH - This is an adaptation of the i2c_bus_esp_idf component for ESPHome*/
#pragma once
//#ifdef USE_ESP32

//#include "esphome/core/component.h"
#include "i2c_bus.h"
#include "ch422g.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_err.h>
//namespace i2c {
//namespace ch422g {    
const float BUS = 1.0f;
enum RecoveryCode {
  RECOVERY_FAILED_SCL_LOW,
  RECOVERY_FAILED_SDA_LOW,
  RECOVERY_COMPLETED,
};

//class IDFI2CBus : public InternalI2CBus, public Component {
//class IDFI2CBus : public I2CBus , public CH422GComponent{
class IDFI2CBus : public I2CBus , public CH422GComponent{
 public:
  //IDFI2CBus() = default;
  //void setup() override;
  void setup();
  //void dump_config() override;
  void dump_config();
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  //float get_setup_priority() const override { return setup_priority::BUS; }
  float get_setup_priority() const { return BUS; }

  void set_scan(bool scan) { this->scan_ = scan; }
  void set_sda_pin(uint8_t sda_pin) { this->sda_pin_ = sda_pin; }
  void set_sda_pullup_enabled(bool sda_pullup_enabled) { this->sda_pullup_enabled_ = sda_pullup_enabled; }
  void set_scl_pin(uint8_t scl_pin) { this->scl_pin_ = scl_pin; }
  void set_scl_pullup_enabled(bool scl_pullup_enabled) { this->scl_pullup_enabled_ = scl_pullup_enabled; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void get_bus_handle(i2c_master_bus_handle_t *bus)  { *bus = this->bus_; }
  
#if SOC_LP_I2C_SUPPORTED
  void set_lp_mode(bool lp_mode) { this->lp_mode_ = lp_mode; }
#endif

  //int get_port() const override { return this->port_; }
  int get_port() const { return this->port_; }

 private:
  void recover_();
  RecoveryCode recovery_result_{};

 protected:
  i2c_master_dev_handle_t dev_{};
  i2c_master_bus_handle_t bus_{};
  i2c_port_t port_{};
  uint8_t sda_pin_{};
  bool sda_pullup_enabled_{};
  uint8_t scl_pin_{};
  bool scl_pullup_enabled_{};
  uint32_t frequency_{};
  uint32_t timeout_ = 0;
  bool initialized_ = false;
#if SOC_LP_I2C_SUPPORTED
  bool lp_mode_ = false;
#endif
};
//}  // namespace ch422g
//}  // namespace i2c
