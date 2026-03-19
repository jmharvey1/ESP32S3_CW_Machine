/*20251222 JMH - This is an adaptation of the CH422G component for ESPHome*/
#pragma once

// #include "esphome/core/component.h"
// #include "esphome/core/hal.h"
// #include "esphome/components/i2c/i2c.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <string>
#include "driver/i2c.h"
#include "stdint.h" // need this to use char type
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "i2c_bus.h"

typedef const char *LogString;
#define LOG_STR(s) (reinterpret_cast<const LogString *>(s))
#define LOG_STR_ARG(s) (reinterpret_cast<const char *>(s))
#define LOG_STR_LITERAL(s) (s)
#define ESP_LOG_MSG_COMM_FAIL "Communication failed"
#define ESP_LOG_MSG_COMM_FAIL_FOR "Communication failed for '%s'"

extern const uint8_t COMPONENT_STATE_MASK;
// namespace setup_priority {

//extern const float BUS;
extern const float IO;
extern const float HARDWARE;
extern const float DATA;
extern const float HARDWARE_LATE;
extern const float PROCESSOR;
extern const float BLUETOOTH;
extern const float AFTER_BLUETOOTH;
extern const float WIFI;
extern const float ETHERNET;
extern const float BEFORE_CONNECTION;
extern const float AFTER_WIFI;
extern const float AFTER_CONNECTION;
extern const float LATE;

// namespace i2c {
// namespace ch422g{

enum Flags : uint8_t
{
  // Can't name these just INPUT because of Arduino defines :(
  FLAG_NONE = 0x00,
  FLAG_INPUT = 0x01,
  FLAG_OUTPUT = 0x02,
  FLAG_OPEN_DRAIN = 0x04,
  FLAG_PULLUP = 0x08,
  FLAG_PULLDOWN = 0x10,
};


// class CH422GComponent : public Component, public i2c::I2CDevice {
//class CH422GComponent : public I2CRegister, public I2CRegister16
class CH422GComponent
{
public:
  CH422GComponent() = default;
  //CH422GComponent() : I2CRegister(nullptr, 0), I2CRegister16(nullptr, 0) {}

  /// Check i2c availability and setup masks
  // void setup() override;
  void setup();
  /// Poll for input changes periodically
  // void loop() override;
  void loop();
  /// Helper function to read the value of a pin.
  bool digital_read(uint8_t pin);
  /// Helper function to write the value of a pin.
  void digital_write(uint8_t pin, bool value);
  /// Helper function to set the pin mode of a pin.
  // void pin_mode(uint8_t pin, gpio::Flags flags);
  void pin_mode(uint8_t pin, Flags flags);

  //   float get_setup_priority() const override;
  //   float get_loop_priority() const override;
  //   void dump_config() override;
  float get_setup_priority();
  float get_loop_priority();
  void dump_config();

  /*methods/properties from i2cdevice class*/

  // I2CDevice() = default;

  void set_i2c_address(uint8_t address) { address_ = address; }

  uint8_t get_i2c_address() const { return this->address_; }

  void set_i2c_bus(I2CBus *bus) { bus_ = bus; }

  //I2CRegister reg(uint8_t a_register) { return {this, a_register}; }

  //I2CRegister16 reg16(uint16_t a_register) { return {this, a_register}; }

  ErrorCode read(uint8_t *data, size_t len) const { return bus_->write_readv(this->address_, nullptr, 0, data, len); }

  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len);

  ErrorCode read_register16(uint16_t a_register, uint8_t *data, size_t len);

  ErrorCode write(const uint8_t *data, size_t len) const
  {
    return bus_->write_readv(this->address_, data, len, nullptr, 0);
  }

  ErrorCode write_read(const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len) const
  {
    return bus_->write_readv(this->address_, write_data, write_len, read_data, read_len);
  }

  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len) const;

  ErrorCode write_register16(uint16_t a_register, const uint8_t *data, size_t len) const;

  bool read_bytes(uint8_t a_register, uint8_t *data, uint8_t len)
  {
    return read_register(a_register, data, len) == ERROR_OK;
  }

  bool read_bytes_raw(uint8_t *data, uint8_t len) const { return read(data, len) == ERROR_OK; }

  // template<size_t N> optional<std::array<uint8_t, N>> read_bytes(uint8_t a_register) {
  //   std::array<uint8_t, N> res;
  //   if (!this->read_bytes(a_register, res.data(), N)) {
  //     return {};
  //   }
  //   return res;
  // }
  // template<size_t N> optional<std::array<uint8_t, N>> read_bytes_raw() {
  //   std::array<uint8_t, N> res;
  //   if (!this->read_bytes_raw(res.data(), N)) {
  //     return {};
  //   }
  //   return res;
  // }

  bool read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len);

  bool read_byte(uint8_t a_register, uint8_t *data) { return read_register(a_register, data, 1) == ERROR_OK; }

  // optional<uint8_t> read_byte(uint8_t a_register) {
  //   uint8_t data;
  //   if (!this->read_byte(a_register, &data))
  //     return {};
  //   return data;
  // }

  bool read_byte_16(uint8_t a_register, uint16_t *data) { return read_bytes_16(a_register, data, 1); }

  bool write_bytes(uint8_t a_register, const uint8_t *data, uint8_t len) const
  {
    return write_register(a_register, data, len) == ERROR_OK;
  }

  bool write_bytes(uint8_t a_register, const std::vector<uint8_t> &data) const
  {
    return write_bytes(a_register, data.data(), data.size());
  }

  template <size_t N>
  bool write_bytes(uint8_t a_register, const std::array<uint8_t, N> &data)
  {
    return write_bytes(a_register, data.data(), data.size());
  }

  bool write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) const;

  bool write_byte(uint8_t a_register, uint8_t data) const { return write_bytes(a_register, &data, 1); }

  bool write_byte_16(uint8_t a_register, uint16_t data) const { return write_bytes_16(a_register, &data, 1); }

  // Deprecated functions

  // ESPDEPRECATED("The stop argument is no longer used. This will be removed from ESPHome 2026.3.0", "2025.9.0")
  // ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len, bool stop) {
  //   return this->read_register(a_register, data, len);
  // }

  // ESPDEPRECATED("The stop argument is no longer used. This will be removed from ESPHome 2026.3.0", "2025.9.0")
  // ErrorCode read_register16(uint16_t a_register, uint8_t *data, size_t len, bool stop) {
  //   return this->read_register16(a_register, data, len);
  // }

  // ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //               "removed from ESPHome 2026.3.0",
  //               "2025.9.0")
  // ErrorCode write(const uint8_t *data, size_t len, bool stop) const { return this->write(data, len); }

  // ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //               "removed from ESPHome 2026.3.0",
  //               "2025.9.0")
  // ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len, bool stop) const {
  //   return this->write_register(a_register, data, len);
  // }

  // ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //               "removed from ESPHome 2026.3.0",
  //               "2025.9.0")
  // ErrorCode write_register16(uint16_t a_register, const uint8_t *data, size_t len, bool stop) const {
  //   return this->write_register16(a_register, data, len);
  //}

protected:
  bool write_reg_(uint8_t reg, uint8_t value);
  uint8_t read_reg_(uint8_t reg);
  bool set_mode_(uint8_t mode);
  bool read_inputs_();
  bool write_outputs_();
  virtual float get_setup_priority() const;
  virtual float get_loop_priority() const;
  void mark_failed();
  void status_set_error();
  void status_set_error(const char *message);
  //void status_set_error(const LogString *message);
  void set_component_source(const LogString *source) { component_source_ = source; }
  const LogString *get_component_log_str() const;
  bool is_failed() const;
  bool is_ready() const;
  void status_clear_warning();
  void status_clear_error();
  void set_component_state_(uint8_t state);
  void status_set_warning(const char *message = nullptr);
  void status_set_warning(const LogString *message);

  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_bits_{0x00};
  /// Flags to check if read previously during this loop
  uint8_t pin_read_flags_ = {0x00};
  /// Copy of last read values
  uint8_t input_bits_ = {0x00};
  /// Copy of the mode value
  uint8_t mode_value_{};
  uint8_t address_{0x00};         ///< store the address of the device on the bus
  uint8_t component_state_{0x00}; ///< store the state of the component
  const LogString *component_source_{nullptr};
  I2CBus *bus_{nullptr}; ///< pointer to I2CBus instance
};
/// Helper class to expose a CH422G pin as a GPIO pin.
// class CH422GGPIOPin : public GPIOPin {
class CH422GGPIOPin
{
public:
  //   void setup() override{};
  //   void pin_mode(gpio::Flags flags) override;
  //   bool digital_read() override;
  //   void digital_write(bool value) override;
  //   std::string dump_summary() const override;
  void setup();
  void pin_mode(Flags flags);
  // bool digital_read() override;
  bool digital_read();
  void digital_write(bool value);
  std::string dump_summary();
  void set_parent(CH422GComponent *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(Flags flags);

  // Flags get_flags() const override { return this->flags_; }
  Flags get_flags() { return this->flags_; }

protected:
  CH422GComponent *parent_{};
  uint8_t pin_{};
  bool inverted_{};
  Flags flags_{};
};

//} // namespace ch422g
//} // namespace i2c
