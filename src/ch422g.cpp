/*20251222 JMH - This is an adaptation of the CH422G component for ESPHome*/
#include <cstdio>
#include "ch422g.h"
#include "i2c_bus.h"
//#include "esphome/core/log.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "i2cdevice.h"
//namespace i2c {
//namespace ch422g {

static const uint8_t CH422G_REG_MODE = 0x24;
static const uint8_t CH422G_MODE_OUTPUT = 0x01;      // enables output mode on 0-7
static const uint8_t CH422G_MODE_OPEN_DRAIN = 0x04;  // enables open drain mode on 8-11
static const uint8_t CH422G_REG_IN = 0x26;           // read reg for input bits
static const uint8_t CH422G_REG_OUT = 0x38;          // write reg for output bits 0-7
static const uint8_t CH422G_REG_OUT_UPPER = 0x23;    // write reg for output bits 8-11

// Component state uses bits 0-2 (8 states, 5 used)
const uint8_t COMPONENT_STATE_MASK = 0x07;
const uint8_t COMPONENT_STATE_CONSTRUCTION = 0x00;
const uint8_t COMPONENT_STATE_SETUP = 0x01;
const uint8_t COMPONENT_STATE_LOOP = 0x02;
const uint8_t COMPONENT_STATE_FAILED = 0x03;
const uint8_t COMPONENT_STATE_LOOP_DONE = 0x04;
// Status LED uses bits 3-4
const uint8_t STATUS_LED_MASK = 0x18;
const uint8_t STATUS_LED_OK = 0x00;
const uint8_t STATUS_LED_WARNING = 0x08;  // Bit 3
const uint8_t STATUS_LED_ERROR = 0x10;    // Bit 4

const float IO = 1.0f;  // Adjust the setup priority as needed
static const char *const TAG = "ch422g";

void CH422GComponent::setup() {
  // set outputs before mode
  this->write_outputs_();
  ESP_LOGV(TAG, "Setting outputs complete\n");
  // Set mode and check for errors
  if (!this->set_mode_(this->mode_value_) || !this->read_inputs_()) {
    ESP_LOGE(TAG, "CH422G not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  //ESP_LOGCONFIG(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
  //              this->status_has_error());
}

void CH422GComponent::loop() {
  // Clear all the previously read flags.
  this->pin_read_flags_ = 0x00;
}

void CH422GComponent::dump_config() {
  //ESP_LOGCONFIG(TAG, "CH422G:");
  //LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void CH422GComponent::pin_mode(uint8_t pin, Flags flags) {
  if (pin < 8) {
    if (flags & FLAG_OUTPUT) {
      this->mode_value_ |= CH422G_MODE_OUTPUT;
    }
  } else {
    if (flags & FLAG_OPEN_DRAIN) {
      this->mode_value_ |= CH422G_MODE_OPEN_DRAIN;
    }
  }
}

bool CH422GComponent::digital_read(uint8_t pin) {
  if (this->pin_read_flags_ == 0 || this->pin_read_flags_ & (1 << pin)) {
    // Read values on first access or in case it's being read again in the same loop
    this->read_inputs_();
  }

  this->pin_read_flags_ |= (1 << pin);
  return (this->input_bits_ & (1 << pin)) != 0;
}

void CH422GComponent::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->output_bits_ |= (1 << pin);//this->output_bits_ |= (1 << pin);
  } else {
    this->output_bits_ &= ~(1 << pin);
  }
  this->write_outputs_();
}

bool CH422GComponent::read_inputs_() {
  if (this->is_failed()) {
    return false;
  }
  uint8_t result;
  // reading inputs requires the chip to be in input mode, possibly temporarily.
  if (this->mode_value_ & CH422G_MODE_OUTPUT) {
    this->set_mode_(this->mode_value_ & ~CH422G_MODE_OUTPUT);
    result = this->read_reg_(CH422G_REG_IN);
    this->set_mode_(this->mode_value_);
  } else {
    result = this->read_reg_(CH422G_REG_IN);
  }
  this->input_bits_ = result;
  this->status_clear_warning();
  return true;
}

// Write a register. Can't use the standard write_byte() method because there is no single pre-configured i2c address.
bool CH422GComponent::write_reg_(uint8_t reg, uint8_t value) {
  ErrorCode err = this->bus_->write_readv(reg, &value, 1, nullptr, 0);
  if (err != ERROR_OK) {
    //this->status_set_warning(str_sprintf("write failed for register 0x%X, error %d", reg, err).c_str());
    char str[50];
    sprintf(str, "write failed for register 0x%X, error %d", reg, err);
    this->status_set_warning(std::string(str).c_str());
    return false;
  } else {
    ESP_LOGV(TAG, "write_reg_ complete\n");
  }
  this->status_clear_warning();
  return true;
}

uint8_t CH422GComponent::read_reg_(uint8_t reg) {
  uint8_t value;
  auto err = this->bus_->write_readv(reg, nullptr, 0, &value, 1);
  if (err != ERROR_OK) {
    char str[50];
    sprintf(str, "read failed for register 0x%X, error %d", reg, err);
    this->status_set_warning(std::string(str).c_str());
    return 0;
  }
  this->status_clear_warning();
  return value;
}

bool CH422GComponent::set_mode_(uint8_t mode) { return this->write_reg_(CH422G_REG_MODE, mode); ESP_LOGV(TAG, "Set mode complete\n");}

bool CH422GComponent::write_outputs_() {
  return this->write_reg_(CH422G_REG_OUT, static_cast<uint8_t>(this->output_bits_)) &&
         this->write_reg_(CH422G_REG_OUT_UPPER, static_cast<uint8_t>(this->output_bits_ >> 8));
}

//float CH422GComponent::get_setup_priority() const { return setup_priority::IO; }
float CH422GComponent::get_setup_priority() const { return IO; }

// Run our loop() method very early in the loop, so that we cache read values
// before other components call our digital_read() method.
float CH422GComponent::get_loop_priority() const { return 9.0f; }  // Just after WIFI

void CH422GComponent::mark_failed() {
  ESP_LOGE(TAG, "%s was marked as failed", LOG_STR_ARG(this->get_component_log_str()));
  this->set_component_state_(COMPONENT_STATE_FAILED);
  this->status_set_error();
  // Also remove from loop since failed components shouldn't loop
  //App.disable_component_loop_(this);
}
const LogString *CH422GComponent::get_component_log_str() const {
  return this->component_source_ == nullptr ? LOG_STR("<unknown>") : this->component_source_;
}

void CH422GComponent::status_set_error() { 
    this->status_set_error((const char *) nullptr); 
}
void CH422GComponent::status_set_error(const char *message)
{
    if ((this->component_state_ & STATUS_LED_ERROR) != 0)
        return;
    this->component_state_ |= STATUS_LED_ERROR;
    // App.app_state_ |= STATUS_LED_ERROR;
    ESP_LOGE(TAG, "%s set Error flag: %s", LOG_STR_ARG(this->get_component_log_str()),
             message ? message : ("unspecified"));
    //   if (message != nullptr) {
    //     store_component_error_message(this, message, false);
    //   }
}
bool CH422GComponent::is_failed() const { return (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED; }
bool CH422GComponent::is_ready() const {
  return (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP ||
         (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP_DONE ||
         (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_SETUP;
}

void CH422GComponent::status_clear_warning() {
  if ((this->component_state_ & STATUS_LED_WARNING) == 0)
    return;
  this->component_state_ &= ~STATUS_LED_WARNING;
  ESP_LOGW(TAG, "%s cleared Warning flag", LOG_STR_ARG(this->get_component_log_str()));
}
void CH422GComponent::status_clear_error() {
  if ((this->component_state_ & STATUS_LED_ERROR) == 0)
    return;
  this->component_state_ &= ~STATUS_LED_ERROR;
  ESP_LOGE(TAG, "%s cleared Error flag", LOG_STR_ARG(this->get_component_log_str()));
}

void CH422GComponent::set_component_state_(uint8_t state) {
  this->component_state_ &= ~COMPONENT_STATE_MASK;
  this->component_state_ |= state;
}

void CH422GComponent::status_set_warning(const char *message) {
  // Don't spam the log. This risks missing different warning messages though.
  if ((this->component_state_ & STATUS_LED_WARNING) != 0)
    return;
  this->component_state_ |= STATUS_LED_WARNING;
  // App.app_state_ |= STATUS_LED_WARNING;
  ESP_LOGW(TAG, "%s set Warning flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? message : ("unspecified"));
}
void CH422GComponent::status_set_warning(const LogString *message) {
  // Don't spam the log. This risks missing different warning messages though.
  if ((this->component_state_ & STATUS_LED_WARNING) != 0)
    return;
  this->component_state_ |= STATUS_LED_WARNING;
  // App.app_state_ |= STATUS_LED_WARNING;
  ESP_LOGW(TAG, "%s set Warning flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? LOG_STR_ARG(message) : ("unspecified"));
}
/*the following taken from i2c::i2cdevice class*/
 
ErrorCode CH422GComponent::read_register(uint8_t a_register, uint8_t *data, size_t len) {
  return bus_->write_readv(this->address_, &a_register, 1, data, len);
}
 
ErrorCode CH422GComponent::read_register16(uint16_t a_register, uint8_t *data, size_t len) {
  a_register = convert_big_endian(a_register);
  return bus_->write_readv(this->address_, reinterpret_cast<const uint8_t *>(&a_register), 2, data, len);
}
 
ErrorCode CH422GComponent::write_register(uint8_t a_register, const uint8_t *data, size_t len) const {
  SmallBufferWithHeapFallback<17> buffer_alloc;  // Most I2C writes are <= 16 bytes
  uint8_t *buffer = buffer_alloc.get(len + 1);
 
  buffer[0] = a_register;
  std::copy(data, data + len, buffer + 1);
  return this->bus_->write_readv(this->address_, buffer, len + 1, nullptr, 0);
}
 
ErrorCode CH422GComponent::write_register16(uint16_t a_register, const uint8_t *data, size_t len) const {
  SmallBufferWithHeapFallback<18> buffer_alloc;  // Most I2C writes are <= 16 bytes + 2 for register
  uint8_t *buffer = buffer_alloc.get(len + 2);
 
  buffer[0] = a_register >> 8;
  buffer[1] = a_register;
  std::copy(data, data + len, buffer + 2);
  return this->bus_->write_readv(this->address_, buffer, len + 2, nullptr, 0);
}
 
bool CH422GComponent::read_bytes_16(uint8_t a_register, uint16_t *data, uint8_t len) {
  if (read_register(a_register, reinterpret_cast<uint8_t *>(data), len * 2) != ERROR_OK)
    return false;
  for (size_t i = 0; i < len; i++)
    data[i] = i2ctohs(data[i]);
  return true;
}
 
bool CH422GComponent::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) const {
  // we have to copy in order to be able to change byte order
  std::unique_ptr<uint16_t[]> temp{new uint16_t[len]};
  for (size_t i = 0; i < len; i++)
    temp[i] = htoi2cs(data[i]);
  return write_register(a_register, reinterpret_cast<const uint8_t *>(temp.get()), len * 2) == ERROR_OK;
}


//////////////////////////////////////////////////////////////////////////////////

void CH422GGPIOPin::pin_mode(Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool CH422GGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) ^ this->inverted_; }

void CH422GGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value ^ this->inverted_); }
//std::string CH422GGPIOPin::dump_summary() const { return str_sprintf("EXIO%u via CH422G", pin_); }
std::string CH422GGPIOPin::dump_summary(){ 
    char str[50];
    sprintf(str, "EXIO%u via CH422G", this->pin_);
    return std::string(str);    
}
void CH422GGPIOPin::set_flags(Flags flags) {
  flags_ = flags;
  this->parent_->pin_mode(this->pin_, flags);
}

//}  // namespace ch422g
//}  // namespace i2c
