#ifndef __WATERING_CONTROLLER_H__
#define __WATERING_CONTROLLER_H__

#include <time.h>

String local_time_as_string();

class WateringController {
  public:
    // Water at 6:00am.
    static constexpr int kWaterTimeHour = 6;
    static constexpr int kWaterTimeMinute = 0;

    // Water every 40 hours (once 40 hours have elapsed since last watering,
    // the system will start watering the next time we hit the watering time
    // of day above).
    static constexpr int kWaterIntervalMs = 40 * 60 * 60 * 1000;
    static constexpr int kWaterDurationMs = 20 * 60 * 1000; // 20 minutes.

    // Stop watering when we have less than this much water left (so we don't
    // burn the pump).
    static constexpr float kLowWaterLevel = 100.0f;

    // Restart watering when we have more than this much water over kLowWaterLevel left.
    static constexpr float kRestartWateringHysteresis = 20.0f;

    WateringController(int watering_control_pin)
        : earliest_next_water_time_(0), watering_end_time_(0), watering_control_pin_(watering_control_pin),
          water_level_high_enough_(true) {}

    void TriggerWater() {
      watering_end_time_ = millis() + kWaterDurationMs;
    }

    void ResetTimer() {
      watering_end_time_ = 0;
      earliest_next_water_time_ = millis() + kWaterIntervalMs;
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
        earliest_next_water_time_ = now + kWaterIntervalMs;
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
          return true;
        } else {
          return false;
        }
      }
      return false;
    }

    uint32_t earliest_next_water_time_;
    uint32_t watering_end_time_;
    int watering_control_pin_;
    bool water_level_high_enough_;
};

#endif // __WATERING_CONTROLLER_H__
