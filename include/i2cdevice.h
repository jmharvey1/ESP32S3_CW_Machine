/*20251222 JMH - This is an adaptation of the I2CDEVICE component for ESPHome*/
#pragma once

// #include "esphome/core/component.h"
// #include "esphome/core/hal.h"
// #include "esphome/components/i2c/i2c.h"
#include <stdio.h>
#include <string>
#include "driver/i2c.h"
#include "stdint.h" // need this to use char type
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "i2c_bus.h"
////////////////////////////////////////////////////////////////////////////////////
template <typename T>
constexpr T byteswap(T n)
{
  T m;
  for (size_t i = 0; i < sizeof(T); i++)
    reinterpret_cast<uint8_t *>(&m)[i] = reinterpret_cast<uint8_t *>(&n)[sizeof(T) - 1 - i];
  return m;
}
template <typename T>
constexpr T convert_big_endian(T val)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return byteswap(val);
#else
  return val;
#endif
}

// 

// like ntohs/htons but without including networking headers.
// ("i2c" byte order is big-endian)
inline uint16_t i2ctohs(uint16_t i2cshort) { return convert_big_endian(i2cshort); }
inline uint16_t htoi2cs(uint16_t hostshort) { return convert_big_endian(hostshort); }


////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
class I2CDevice;  // forward declaration
class I2CRegister
{
public:
  I2CRegister &operator=(uint8_t value);

  I2CRegister &operator&=(uint8_t value);

  I2CRegister &operator|=(uint8_t value);

  explicit operator uint8_t() const { return get(); }

  uint8_t get() const;
  
  
protected:
  friend class I2CDevice;
  I2CRegister(I2CDevice *parent, uint8_t a_register) : parent_(parent), register_(a_register) {}
  I2CDevice *parent_;
  uint8_t register_;
};

class I2CRegister16
{
public:
  I2CRegister16 &operator=(uint8_t value);

  I2CRegister16 &operator&=(uint8_t value);

  I2CRegister16 &operator|=(uint8_t value);

  explicit operator uint8_t() const { return get(); }

  uint8_t get() const;
    
protected:
  friend class I2CDevice;
  I2CRegister16(I2CDevice *parent, uint16_t a_register) : parent_(parent), register_(a_register) {}
  I2CDevice *parent_;
  uint16_t register_;
};

/////////////////////////////////////////////////////////////////////////////////////

class I2CDevice{
 public:
  I2CDevice() = default;
 
  void set_i2c_address(uint8_t address) { address_ = address; }
 
  uint8_t get_i2c_address() const { return this->address_; }
 
  void set_i2c_bus(I2CBus *bus) { bus_ = bus; }
 
  I2CRegister reg(uint8_t a_register) { return {this, a_register}; }
 
  I2CRegister16 reg16(uint16_t a_register) { return {this, a_register}; }
 
  ErrorCode read(uint8_t *data, size_t len) const { return bus_->write_readv(this->address_, nullptr, 0, data, len); }
 
  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len);
 
  ErrorCode read_register16(uint16_t a_register, uint8_t *data, size_t len);
 
  ErrorCode write(const uint8_t *data, size_t len) const {
    return bus_->write_readv(this->address_, data, len, nullptr, 0);
  }
 
  ErrorCode write_read(const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len) const {
    return bus_->write_readv(this->address_, write_data, write_len, read_data, read_len);
  }
 
  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len) const;
 
  ErrorCode write_register16(uint16_t a_register, const uint8_t *data, size_t len) const;
 
 
  bool read_bytes(uint8_t a_register, uint8_t *data, uint8_t len) {
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
 
  bool write_bytes(uint8_t a_register, const uint8_t *data, uint8_t len) const {
    return write_register(a_register, data, len) == ERROR_OK;
  }
 
  bool write_bytes(uint8_t a_register, const std::vector<uint8_t> &data) const {
    return write_bytes(a_register, data.data(), data.size());
  }
 
  template<size_t N> bool write_bytes(uint8_t a_register, const std::array<uint8_t, N> &data) {
    return write_bytes(a_register, data.data(), data.size());
  }
 
  bool write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) const;
 
  bool write_byte(uint8_t a_register, uint8_t data) const { return write_bytes(a_register, &data, 1); }
 
  bool write_byte_16(uint8_t a_register, uint16_t data) const { return write_bytes_16(a_register, &data, 1); }
 
  // Deprecated functions
 
  //ESPDEPRECATED("The stop argument is no longer used. This will be removed from ESPHome 2026.3.0", "2025.9.0")
  ErrorCode read_register(uint8_t a_register, uint8_t *data, size_t len, bool stop) {
    return this->read_register(a_register, data, len);
  }
 
  //ESPDEPRECATED("The stop argument is no longer used. This will be removed from ESPHome 2026.3.0", "2025.9.0")
  ErrorCode read_register16(uint16_t a_register, uint8_t *data, size_t len, bool stop) {
    return this->read_register16(a_register, data, len);
  }
 
  //ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //              "removed from ESPHome 2026.3.0",
  //              "2025.9.0")
  ErrorCode write(const uint8_t *data, size_t len, bool stop) const { return this->write(data, len); }
 
  //ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //              "removed from ESPHome 2026.3.0",
  //              "2025.9.0")
  ErrorCode write_register(uint8_t a_register, const uint8_t *data, size_t len, bool stop) const {
    return this->write_register(a_register, data, len);
  }
 
  //ESPDEPRECATED("The stop argument is no longer used; use write_read() for consecutive write and read. This will be "
  //              "removed from ESPHome 2026.3.0",
  //              "2025.9.0")
  ErrorCode write_register16(uint16_t a_register, const uint8_t *data, size_t len, bool stop) const {
    return this->write_register16(a_register, data, len);
  }
 
 protected:
  uint8_t address_{0x00};  
  I2CBus *bus_{nullptr};   
};


