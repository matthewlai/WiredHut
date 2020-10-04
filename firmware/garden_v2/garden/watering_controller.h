#ifndef __WATERING_CONTROLLER_H__
#define __WATERING_CONTROLLER_H__

#include <time.h>

String local_time_as_string();

class WateringController {
  public:
    // Water at 6:00am.
    static constexpr int kWaterTimeHour = 6;
    static constexpr int kWaterTimeMinute = 0;

    // Once 60 hours have elapsed since last watering,
    // the system will start watering the next time we hit the watering time
    // of day above, if the moisture level is lower than the threshold.
    static constexpr int kMinWaterIntervalMs = 60 * 60 * 60 * 1000;
    static constexpr float kMoistureThreshold = 20.0f;
    static constexpr int kWaterDurationMs = 15 * 60 * 1000; // 15 minutes.

    // Stop watering when we have less than this much water left (so we don't
    // burn the pump).
    static constexpr float kLowWaterLevel = 50.0f;

    // Restart watering when we have more than this much water over kLowWaterLevel left.
    static constexpr float kRestartWateringHysteresis = 20.0f;

    WateringController(int watering_control_pin, SoilMoistureSensor* soil_sensor)
        : soil_sensor_(soil_sensor), earliest_next_water_time_(0), watering_end_time_(0),
          watering_control_pin_(watering_control_pin), water_level_high_enough_(true) {}

    void TriggerWater() {
      watering_end_time_ = millis() + kWaterDurationMs;
    }

    void ResetTimer() {
      watering_end_time_ = 0;
      earliest_next_water_time_ = millis() + kMinWaterIntervalMs;
    }

    void Handle() {
      uint32_t now = millis();
      static bool last_is_watering = false;
      bool is_watering = now < watering_end_time_;
      if (is_watering && !water_level_high_enough_) {
        log("Terminating watering due to low water level.");
        watering_end_time_ = 0;
      }
      if (!last_is_watering && is_watering) {
        log(String("Started watering at ") + local_time_as_string());
      } else if (last_is_watering && !is_watering) {
        log(String("Finished watering at ") + local_time_as_string());
      }
      last_is_watering = is_watering;
      digitalWrite(watering_control_pin_, is_watering);
      if (is_watering) {
        earliest_next_water_time_ = now + kMinWaterIntervalMs;
      }
      if (!is_watering && ShouldStartWatering(now)) {
        watering_end_time_ = now + kWaterDurationMs;
      }
    }

    void SetWaterVolume(float vol) {
      if (water_level_high_enough_ && vol < kLowWaterLevel) {
        water_level_high_enough_ = false;
      } else if (!water_level_high_enough_ && vol > (kLowWaterLevel + kRestartWateringHysteresis)) {
        water_level_high_enough_ = true;
      }
    }

  private:
    bool ShouldStartWatering(uint32_t now) {
      if (now >= earliest_next_water_time_) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          log("Failed to get local time. Not watering.");
          earliest_next_water_time_ = now + 60 * 60 * 1000;
          return false;
        }

        if (!water_level_high_enough_) {
          log("Water level too low. Not watering.");
          earliest_next_water_time_ = now + 20 * 60 * 60 * 1000;
          return false;
        }

        if (timeinfo.tm_hour == kWaterTimeHour && timeinfo.tm_min == kWaterTimeMinute) {
          if (soil_sensor_->LastMoistureReading() > kMoistureThreshold) {
            log("Soil moisture level still high. Skipping watering.");
            earliest_next_water_time_ = now + 20 * 60 * 60 * 1000;
            return false;
          }
          return true;
        } else {
          return false;
        }
      }
      return false;
    }

    SoilMoistureSensor* soil_sensor_;

    uint32_t earliest_next_water_time_;
    uint32_t watering_end_time_;
    int watering_control_pin_;
    bool water_level_high_enough_;
};

#endif // __WATERING_CONTROLLER_H__
