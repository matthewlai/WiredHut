#ifndef __PRESSURE_SENSOR_H__
#define __PRESSURE_SENSOR_H__

#include <InfluxDbClient.h>

#include <RateLimiter.h>

#include "ina226.h"

class PressureSensor {
  public:
    static constexpr int kSenseIntervalMs = 30000;
    static constexpr uint32_t kSenseDelayMs = 2000; // How long the sensor needs to stabilize after power on before we can read.
    static constexpr float kLowCurrent = 0.004f;
    static constexpr float kHighCurrent = 0.02f;
    static constexpr float kFullScalePressureHeight = 5.0f;
    static constexpr float kLitrePerM = 550.0f;
  
    PressureSensor(Ina226* sensor, int sw_pin)
      : sensor_(sensor), sw_pin_(sw_pin), pending_update_(false), read_delay_end_time_(0), last_reading_(0.0f), have_data_(false) {
      sensor->SetSamplesToAverage(5); // 256 samples (~260ms).
    }

    float WaterHeight() const {
      return (last_reading_ - kLowCurrent) / (kHighCurrent - kLowCurrent) * kFullScalePressureHeight;
    }

    float WaterVolume() const {
      return kLitrePerM * WaterHeight();
    }

    Point MakeInfluxDbPoint() const {
      Point pt("garden_water");
      pt.addField("water_volume", WaterVolume(), 1);
      return pt;    
    }

    bool HaveNewData() const { return have_data_; }
    void ClearNewDataFlag() { have_data_ = false; }

    void Handle() {
      if (pending_update_) {
        if (millis() >= read_delay_end_time_) {
          last_reading_ = -sensor_->ShuntCurrent();
          digitalWrite(sw_pin_, LOW);
          pending_update_ = false;
          have_data_ = true;
        }
      } else {
        update_limiter_.CallOrDrop([&]() {
          read_delay_end_time_ = millis() + kSenseDelayMs;
          pending_update_ = true;
          digitalWrite(sw_pin_, HIGH);
        });
      }
    }
  
  private:  
    Ina226* sensor_;
    int sw_pin_;
    bool pending_update_;
    uint32_t read_delay_end_time_;
    float last_reading_;
    bool have_data_;
    RateLimiter<kSenseIntervalMs, 1> update_limiter_;
};

#endif // __PRESSURE_SENSOR_H__
