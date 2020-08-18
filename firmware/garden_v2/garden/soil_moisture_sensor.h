#ifndef __SOIL_MOISTURE_SENSOR_H__
#define __SOIL_MOISTURE_SENSOR_H__

#include <InfluxDbClient.h>

#include <RateLimiter.h>

class SoilMoistureSensor {
  public:
    static constexpr int kSenseIntervalMs = 30000;
    static constexpr byte kSoilSensorI2cAddress = 0x20;
    static constexpr float kSoilMoistureMin = 200;
    static constexpr float kSoilMoistureMax = 600;
    static constexpr int kRetryIntervalMs = 10 * 60 * 1000;

    SoilMoistureSensor(TwoWire* i2c_bus)
      : i2c_bus_(i2c_bus), error_next_retry_time_(0), have_data_(false) {}

    Point MakeInfluxDbPoint() const {
      Point pt("garden_soil");
      pt.addField("moisture_percent", last_reading_moisture_, 1);
      pt.addField("soil_temp", last_reading_temperature_, 1);
      return pt;
    }

    bool HaveNewData() const { return have_data_; }
    void ClearNewDataFlag() { have_data_ = false; }

    void Handle() {
      if (millis() < error_next_retry_time_) {
        return;
      }
      update_limiter_.CallOrDrop([&]() {
        uint16_t capacitance_raw = ReadRegister(0);
        int16_t temperature_raw = static_cast<int16_t>(ReadRegister(5));
        
        last_reading_moisture_ = (capacitance_raw - kSoilMoistureMin) / (kSoilMoistureMax - kSoilMoistureMin) * 100.0f;
        last_reading_temperature_ = temperature_raw / 10.0f;
        have_data_ = true;
      });
    }
  
  private:
    uint16_t ReadRegister(byte reg) {
      i2c_bus_->beginTransmission(kSoilSensorI2cAddress);
      i2c_bus_->write(reg);
      if (!HandleError(i2c_bus_->endTransmission())) {
        return 0;
      }
      i2c_bus_->requestFrom(kSoilSensorI2cAddress, 2);
      if (i2c_bus_->available() != 2) {
        HandleError(1);
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
      i2c_bus_->beginTransmission(kSoilSensorI2cAddress);
      i2c_bus_->write(reg);
      i2c_bus_->write((val & 0xFF00) >> 8);
      i2c_bus_->write(val & 0xFF);
      HandleError(i2c_bus_->endTransmission());
    }

    bool HandleError(int error) {
      if (error == 0) {
        error_next_retry_time_ = 0;
        return true;
      } else {
        error_next_retry_time_ = millis() + kRetryIntervalMs;
        log("Failed to read from soil sensor.");
        return false;
      }
    }

    bool HaveError() const { return error_next_retry_time_ != 0; }

    TwoWire* i2c_bus_;
    float last_reading_moisture_;
    float last_reading_temperature_;
    uint32_t error_next_retry_time_;
    bool have_data_;
    RateLimiter<kSenseIntervalMs, 1> update_limiter_;
};

#endif // __SOIL_MOISTURE_SENSOR_H__
