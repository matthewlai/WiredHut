#ifndef __INA226_H__
#define __INA226_H__

class Ina226 {
  public:
    static constexpr float kShuntVoltageLSB = 2.5e-6f;
    static constexpr float kBusVoltageLSB = 1.25e-3f;
    static constexpr float kMillisecondsInAnHour = 1000.0f * 60.0f * 60.0f;

    Ina226(TwoWire* i2c_bus, byte i2c_addr, float shunt_resistance, int alert_pin)
      : i2c_bus_(i2c_bus), last_reading_time_(millis()), i2c_addr_(i2c_addr), alert_pin_(alert_pin),
        new_data_(false), current_multiplier_(kShuntVoltageLSB / shunt_resistance),
        q_multiplier_(kShuntVoltageLSB / shunt_resistance / kMillisecondsInAnHour){
      auto config_reg = ReadRegister(0x00);
      config_reg |= static_cast<uint16_t>(0x4) << 9; // 128 samples average.
      WriteRegister(0x00, config_reg);
      WriteRegister(0x06, 0x1 << 10); // Alert on conversion ready.
    }

    bool HaveNewData() const { return new_data_; }
    void ClearNewData() { new_data_ = false; }

    float BusVoltage() const { return voltage_raw_ * kBusVoltageLSB; }
    float ShuntCurrent() const { return current_raw_ * current_multiplier_; }
    float AccumulatedChargeAh() const { return q_raw_ * current_multiplier_ / kMillisecondsInAnHour; }
    void ResetAccumulatedCharge() { q_raw_ = 0; }

    void Handle() {
      if (digitalRead(alert_pin_) == LOW) {
        if (ReadRegister(0x06) & (0x1 << 3)) {
          // Conversion ready.
          new_data_ = true;
          voltage_raw_ = static_cast<int16_t>(ReadRegister(0x02));
          current_raw_ = static_cast<int16_t>(ReadRegister(0x01));
          uint32_t time_now = millis();
          q_raw_ += static_cast<int64_t>(time_now - last_reading_time_) * static_cast<int64_t>(current_raw_);
          last_reading_time_ = time_now;
        }
      }
    }

  private:
    uint16_t ReadRegister(byte reg) {
      i2c_bus_->beginTransmission(i2c_addr_);
      i2c_bus_->write(reg);
      i2c_bus_->endTransmission();
      i2c_bus_->requestFrom(i2c_addr_, 2);
      if (i2c_bus_->available() != 2) {
        Serial.println(String("Read failed. Expected 2 bytes, got ") + i2c_bus_->available());
        while (i2c_bus_->available()) {
          i2c_bus_->read();
        }
        return 0;
      }
      uint16_t ret = 0;
      ret = i2c_bus_->read();
      ret <<= 8;
      ret |= i2c_bus_->read();
      return ret;
    }
    
    void WriteRegister(byte reg, uint16_t val) {
      i2c_bus_->beginTransmission(i2c_addr_);
      i2c_bus_->write(reg);
      i2c_bus_->write((val & 0xFF00) >> 8);
      i2c_bus_->write(val & 0xFF);
      i2c_bus_->endTransmission();
    }

    TwoWire* i2c_bus_;
    uint32_t last_reading_time_;
    byte i2c_addr_;
    int alert_pin_;
    bool new_data_;
    float current_multiplier_;
    float q_multiplier_;
    int16_t voltage_raw_;
    int16_t current_raw_;

    // We accumulate Q in raw shunt reading * milliseconds, and
    // only convert to useful units when read. This preserves
    // long term accuracy.
    int64_t q_raw_;
};

#endif // __INA226_H__
