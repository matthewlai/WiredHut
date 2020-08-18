#ifndef __AIR_SENSOR_H__
#define __AIR_SENSOR_H__

// This is the outside air sensor running on a soft I2C bus.
class AirSensor {
  public:
    static constexpr int kSenseIntervalMs = 30000;

    // This chip has a non-configurable I2C address.
    static constexpr byte kI2cAddr = 0x40;
    static constexpr uint32_t kMeasurementTimeoutMs = 200; // Datasheet says 29 and 85ms max.
    static constexpr int kRetryIntervalMs = 60 * 1000;

    AirSensor(TwoWire* i2c_bus)
      : i2c_bus_(i2c_bus), last_reading_humidity_(0.0f), last_reading_temperature_(0.0f),
        error_next_retry_time_(0), have_data_(false) {}

    Point MakeInfluxDbPoint() const {
      Point pt("env");
      pt.addField("oa_temp", last_reading_temperature_, 1);
      pt.addField("oa_humidity", last_reading_humidity_, 1);
      return pt;
    }

    bool HaveNewData() const { return have_data_; }
    void ClearNewDataFlag() { have_data_ = false; }

    void Handle() {
      if (millis() < error_next_retry_time_) {
        return;
      }

      update_limiter_.CallOrDrop([&]() {
        // Start temperature measurement without clock stretching.
        uint16_t temp_raw = ReadRegister(0xf3);
        if (HaveError()) { return; }
        last_reading_temperature_ = -46.85f + 175.72f * static_cast<float>(temp_raw) / 65536.0f;
        uint16_t hum_raw = ReadRegister(0xf5);
        if (HaveError()) { return; }
        last_reading_humidity_ = -6.0f + 125.0f * static_cast<float>(hum_raw) / 65536.0f;
        have_data_ = true;
      });
    }
  
  private:
    uint16_t ReadRegister(byte reg) {
      i2c_bus_->beginTransmission(kI2cAddr);
      i2c_bus_->write(reg);
      if (!HandleError(i2c_bus_->endTransmission(false))) {
        return 0;
      }
      // Now we keep trying to read until we get an ack.
      uint32_t start = millis();
      while (true) {
        i2c_bus_->requestFrom(kI2cAddr, 2);
        if (i2c_bus_->available() == 2) {
          // We have data!
          break;
        }
        if ((millis() - start) > kMeasurementTimeoutMs) {
          HandleError(4);
          break;
        }
      }

      if (i2c_bus_->available() != 2) {
        return 0;
      }
 
      uint16_t ret = 0;
      ret = i2c_bus_->read();
      ret <<= 8;
      ret |= i2c_bus_->read();
      return ret;
    }

    bool HandleError(int error) {
      if (error == 0) {
        error_next_retry_time_ = 0;
        return true;
      } else {
        error_next_retry_time_ = millis() + kRetryIntervalMs;
        log("Failed to read from air sensor.");
        return false;
      }
    }

    bool HaveError() const { return error_next_retry_time_ != 0; }

    TwoWire* i2c_bus_;
    float last_reading_humidity_;
    float last_reading_temperature_;
    uint32_t error_next_retry_time_;
    bool have_data_;
    RateLimiter<kSenseIntervalMs, 1> update_limiter_;
};

#endif // __AIR_SENSOR_H__
