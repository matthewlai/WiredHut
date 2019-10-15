/*
 * This file is part of the WiredHut project.
 *
 * Copyright (C) 2019 Matthew Lai <m@matthewlai.ca>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>

#include <adc.h>
#include <buffered_stream.h>
#include <gpio.h>
#include <gpio_defs.h>
#include <i2c.h>
#include <ostrich.h>
#include <systick.h>
#include <usart.h>
#include <usb/serial.h>

#include "config.h"
#include "esp8266.h"
#include "utils.h"

using namespace Ostrich;

void SetLED0(bool on) {
  static OutputPin<PIN_B4> led;
  led = !on;
}

void SetLED1(bool on) {
  static OutputPin<PIN_B5> led;
  led = !on;
}

void SetLED2(bool on) {
  static OutputPin<PIN_B6> led;
  led = !on;
}

void SetLED3(bool on) {
  static OutputPin<PIN_B7> led;
  led = !on;
}

void SetLEDBinary(uint8_t val) {
  SetLED0(val & 1);
  SetLED1(val & (1 << 1));
  SetLED2(val & (1 << 2));
  SetLED3(val & (1 << 3));
}

void SetPump(bool on) {
  static OutputPin<PIN_G8> sw;
  sw = on;
}

void SetSw1(bool on) {
  static OutputPin<PIN_G7> sw;
  sw = on;
}

void SetPressureSensorPower(bool on) {
  static OutputPin<PIN_B11> pwr;
  pwr = 1;
  if (on) {
    pwr.SetOutputOptions(GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ);
    DelayMilliseconds(50);
  } else {
    pwr.SetOutputOptions(GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ);
  }
}

using ADC1_Type = SingleConversionADC<ADC1>;

// The current sensor readings must be first called when expected currents are
// 0 for offset calibration.
float ReadPumpCurrent(ADC1_Type* adc) {
  static auto sampler = adc->GetGPIOInput<9>();
  static float offset = sampler.ReadNormalized();
  [[maybe_unused]] static bool init_once = []() -> bool {
    sampler.SetSamplingTime(10000000);
    return true;
  }();
  return (sampler.ReadNormalized() - offset) * Config::kFullScalePumpCurrent;
}

float ReadSw1Current(ADC1_Type* adc) {
  static auto sampler =adc->GetGPIOInput<8>();
  static float offset = sampler.ReadNormalized();
  [[maybe_unused]] static bool init_once = []() -> bool {
    sampler.SetSamplingTime(10000000);
    return true;
  }();
  return (sampler.ReadNormalized() - offset) * Config::kFullScaleSw1Current;
}

float ReadPressureSensorCurrent(ADC1_Type* adc) {
  static auto sampler =adc->GetGPIOInput<15>();
  [[maybe_unused]] static bool init_once = []() -> bool {
    sampler.SetSamplingTime(10000000);
    return true;
  }();
  return (sampler.ReadNormalized()) * Config::kPressureSensorFullScaleCurrent;
}

float ReadTemperatureInput(ADC1_Type* adc) {
  static auto sampler = adc->GetTemperatureInput();
  [[maybe_unused]] static bool init_once = []() -> bool {
    sampler.SetSamplingTime(10000000);
    return true;
  }();

  return sampler.ReadTempC();
}

float PressureSensorCurrentToHeightMm(float current) {
  return (current - 0.004f) / (0.02f - 0.004f) * 5.0f;
}

template <typename I2CType>
uint16_t I2CReadRegister(I2CType* i2c, uint8_t register_addr) {
  uint8_t buf[2] = {};
  i2c->SendReceive(Config::kMoistureSensorAddress, &register_addr, 1, buf, 2);
  return (static_cast<uint8_t>(buf[0]) << 8) | buf[1];
}

using ESP8266_Type = ESP8266<USART6, PIN_G14, PIN_G9, PIN_G10, PIN_G13, PIN_G11>;

std::unique_ptr<ESP8266_Type> ConnectToHub() {
  auto p = std::make_unique<ESP8266_Type>(Config::kEsp8266BaudRate);
  SetLEDBinary(0);
  if (p->ConnectToAP(Config::kSSID, Config::kPass)) {
    SetLEDBinary(1);
  } else {
    SetLEDBinary(3);
    DelayMilliseconds(1000);
    p.reset();
    return p;
  }
  if (p->ConnectToTCPServer(Config::kEnvironmentControllerLinkId,
                            Config::kHubHost,
                            Config::kEnvironmentControllerPort) &&
      p->ConnectToTCPServer(Config::kGardenControllerLinkId, Config::kHubHost,
                            Config::kGardenControllerPort)) {
    SetLEDBinary(2);
  } else {
    SetLEDBinary(4);
    DelayMilliseconds(1000);
    p.reset();
    return p;
  }
  return p;
}

int main() {
  StartWDG();

  SingleConversionADC<ADC1> adc1;

  SetLEDBinary(0xf);
  DelayMilliseconds(1000);

  SetPump(false);
  SetSw1(false);
  SetPressureSensorPower(true);

  USBSerial usb_serial;

  SetErrorHandler([&usb_serial](const std::string& error) {
    usb_serial << error << std::endl;
  });

  SetLoggingHandler([&usb_serial](const std::string& log) {
    usb_serial << log << std::endl;
  });

  if (Config::kWaitForPortOpen) {
    while (!usb_serial.PortOpen()) {}
  }

  I2C<I2C4, PIN_F15, PIN_F14> moisture_i2c(I2CSpeed::Speed100kHz);

  USART<UART8, PIN_E1, PIN_E0> smartsolar(Config::kSmartSolarBaudRate);

  // Watering algorithm settings.
  int water_time_seconds = Config::kDefaultWaterTimeSeconds;
  int time_between_watering = Config::kDefaultTimeBetweenWatering;
  float min_water_level_for_watering_m = Config::kDefaultMinWaterLevelM;
  float min_water_level_for_watering_restart_m = Config::kDefaultMinWaterLevelRestartM;

  bool watering_now = false;
  int64_t last_watering_time_seconds = 0;
  int64_t watering_start_time = 0;

  bool low_water_level_lockout = false;

  int64_t last_connection_attempt_time = 0;

  int64_t force_state_end = 0;
  bool force_state = false;

  auto esp8266 = ConnectToHub();

  // For calibration.
  ReadPressureSensorCurrent(&adc1);
  ReadPumpCurrent(&adc1);
  ReadSw1Current(&adc1);

  ThrottledExecutor send_update_throttle(1000);
  ThrottledExecutor read_water_level_throttle(2000);

  WindowFilteredValue<16> pump_current;
  WindowFilteredValue<8> pressure_sensor_height;
  WindowFilteredValue<16> soil_moisture;
  WindowFilteredValue<16> soil_temperature;
  WindowFilteredValue<16> mcu_temperature;

  // Solar
  bool solar_data_ready = false;
  WindowFilteredValue<4> batt_voltage;
  WindowFilteredValue<4> batt_current;
  WindowFilteredValue<4> solar_voltage;
  WindowFilteredValue<4> solar_current;
  WindowFilteredValue<4> load_current;
  std::string solar_mode;
  std::string mppt_mode;

  // in Ah
  float state_of_charge_estimation = 0.0f;
  int64_t last_battery_current_reading_time_ms = 0;

  // We also store an unfiltered version of solar voltage for computing
  // current.
  float solar_voltage_raw = 0.0f;
  int solar_error_code = 0;

  // Receive data processing.
  std::string partial_line = "";
  std::vector<std::string> command_queue;

  while (true) {
    int64_t time_now_seconds = GetTimeMilliseconds() / 1000LL;

    if (!esp8266 &&
        time_now_seconds >
        (last_connection_attempt_time + Config::kConnectionRetryTime)) {
      last_connection_attempt_time = time_now_seconds;
      esp8266 = ConnectToHub();
    }

    if (esp8266) {
      std::string received;
      do {
        received = esp8266->ReceiveData(
            Config::kGardenControllerLinkId);
        for (char c : received) {
          if (c == '\n') {
            command_queue.push_back(std::move(partial_line));
            partial_line.clear();
          } else {
            partial_line.push_back(c);
          }
        }
      } while (!received.empty());
    }

    for (const auto& command : command_queue) {
      std::vector<std::string> parts = Split(command, ' ');
      if (parts.size() < 2) {
        continue;
      } else {
        std::string& command_str = parts[0];
        int arg = Parse<int>(parts[1]);
        if (command_str == "PING") {
          if (esp8266) {
            if (!esp8266->SendData(Config::kGardenControllerLinkId, 
                "PONG " + Format(arg) + "\n")) {
              esp8266.reset();
            }
          }
        } else if (command_str == "SET_WATER_TIME") {
          water_time_seconds = arg;
        } else if (command_str == "SET_TIME_BETWEEN_WATERING") {
          time_between_watering = arg;
        } else if (command_str == "SET_FORCE_STATE") {
          if (parts.size() < 3) {
            continue;
          }
          int state = arg;
          int time = Parse<int>(parts[2]);
          force_state_end = time_now_seconds + time;
          force_state = state;
        }
      }
    }

    command_queue.clear();

    while (smartsolar.LineAvailable() && smartsolar.DataAvailable() > 10) {
      SetLEDBinary(5);
      usb_serial << "[Solar] Data available: " << smartsolar.DataAvailable() << std::endl;

      std::string line = smartsolar.GetLine();
      usb_serial << "[Solar] Read: " << line << " (" << line.size() << ")" << std::endl;
      if (line.find("\t") == std::string::npos) {
        break;
      }

      std::vector<std::string> parts = Split(line, '\t');
      std::string field_label = parts[0];

      if (field_label == "Checksum") {
        // Special handling for checksum because if we are lucky, the checksum
        // value may turn out to be '\n', and we will hang below.
        solar_data_ready = true;
        break;
      }

      int value = Parse<int>(RemoveAll(parts[1], '\r'));

      usb_serial << "[Solar] Label: " << field_label << ", Value: " << value << std::endl;

      if (field_label == "V") {
        batt_voltage.AddValue(float(value) / 1000.0f);
      } else if (field_label == "I") {
        batt_current.AddValue(float(value) / 1000.0f);
        int64_t time_now_ms = GetTimeMilliseconds();
        if (last_battery_current_reading_time_ms != 0) {
          float elapsed_time =
              (time_now_ms - last_battery_current_reading_time_ms) / 1000.0f;
          state_of_charge_estimation +=
              float(value) / 1000.0f / 60.0f / 60.0f * elapsed_time;
        }
        last_battery_current_reading_time_ms = time_now_ms;
      } else if (field_label == "VPV") {
        solar_voltage.AddValue(float(value) / 1000.0f);
        solar_voltage_raw = float(value) / 1000.0f;
      } else if (field_label == "PPV") {
        // Not sure why we get PV power instead of current, but we can convert
        // it (PPV is always received after the corresponding VPV)
        float watts = value;
        solar_current.AddValue(watts / solar_voltage_raw);
      } else if (field_label == "IL") {
        load_current.AddValue(float(value) / 1000.0f);
      } else if (field_label == "MPPT") {
        switch (value) {
          case 0:
            mppt_mode = "OFF";
            break;
          case 1:
            mppt_mode = "CVCI";
            break;
          case 2:
            mppt_mode = "MPPT";
            break;
          default:
            mppt_mode = Format(value);
        }
      } else if (field_label == "CS") {
        switch (value) {
          case 0:
            solar_mode = "OFF";
            break;
          case 2:
            solar_mode = "FAULT";
            break;
          case 3:
            solar_mode = "BULK";
            break;
          case 4:
            solar_mode = "ABSORPTION";
            break;
          case 5:
            solar_mode = "FLOAT";
            state_of_charge_estimation = Config::kFullBatteryChargeAh;
            break;
          default:
            solar_mode = Format(value);
        }
      } else if (field_label == "ERR") {
        solar_error_code = value;
      }
    }

    SetLEDBinary(6);

    soil_temperature.AddValue(float(I2CReadRegister(&moisture_i2c,
                                    Config::kTemperatureRegister)) / 10.0f);

    int soil_moisture_raw = I2CReadRegister(&moisture_i2c,
                                    Config::kMoistureRegister);
    SetLEDBinary(7);

    soil_moisture.AddValue(
        static_cast<float>(soil_moisture_raw - Config::kSoilMoistureMin) /
        (Config::kSoilMoistureMax - Config::kSoilMoistureMin) * 100.0f);

    pump_current.AddValue(ReadPumpCurrent(&adc1));
    mcu_temperature.AddValue(ReadTemperatureInput(&adc1));

    SetLEDBinary(8);

    read_water_level_throttle.MaybeExecute([&]() {
      SetPressureSensorPower(true);
      pressure_sensor_height.AddValue(
          PressureSensorCurrentToHeightMm(ReadPressureSensorCurrent(&adc1)));
      SetPressureSensorPower(false);
    });

    if (esp8266) {
      send_update_throttle.MaybeExecute([&]() {
        SetLEDBinary(9);

        TrySend(&esp8266, Config::kEnvironmentControllerLinkId, 
            "TEMP " + Format(int(mcu_temperature.AvgValue() * 100)) +
            " garden_mcu\n");

        TrySend(&esp8266, Config::kEnvironmentControllerLinkId, 
            "TEMP " + Format(int(soil_temperature.AvgValue() * 100.0f)) +
            " soil\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "PUMP_ON " + Format(int(watering_now)) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "PUMP_I " + Format(int(pump_current.AvgValue() * 1000)) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "WATER_LEVEL " +
            Format(int(pressure_sensor_height.AvgValue() * 1000)) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "SOIL_MOISTURE " + Format(int(soil_moisture.AvgValue())) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "UPTIME " + Format(GetTimeMilliseconds() / 1000ULL) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "SOC " + Format(int(state_of_charge_estimation * 1000)) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "FORCE_STATE " + Format(int(time_now_seconds < force_state_end)) +
            "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "WATER_TIME " + Format(water_time_seconds) + "\n");

        TrySend(&esp8266, Config::kGardenControllerLinkId, 
            "TIME_BETWEEN_WATERING " + Format(time_between_watering) + "\n");

        if (solar_data_ready) {
          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "SOL_V " + Format(int(solar_voltage.AvgValue() * 1000)) + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "SOL_I " + Format(int(solar_current.AvgValue() * 1000)) + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "BATT_V " + Format(int(batt_voltage.AvgValue() * 1000)) + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "BATT_I " + Format(int(batt_current.AvgValue() * 1000)) + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "LOAD_I " + Format(int(load_current.AvgValue() * 1000)) + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "SOL_MODE " + solar_mode + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "MPPT_MODE " + mppt_mode + "\n");

          TrySend(&esp8266, Config::kGardenControllerLinkId, 
              "SOL_ERR " + Format(solar_error_code) + "\n");

          solar_data_ready = false;
        }

        SetLEDBinary(9);
      });
    }

    if (time_now_seconds < force_state_end) {
      watering_now = force_state;
    } else {
      if (watering_now) {
        if (low_water_level_lockout ||
            (time_now_seconds - watering_start_time) > water_time_seconds) {
          watering_now = false;
        }
      } else {
        int64_t time_since_last_water =
            time_now_seconds - last_watering_time_seconds;
        if ((time_since_last_water > time_between_watering) &&
            !low_water_level_lockout) {
          watering_now = true;
          watering_start_time = time_now_seconds;
          last_watering_time_seconds = time_now_seconds;
        }
      }
    }

    SetPump(watering_now);

    if (pressure_sensor_height.AvgValue() < min_water_level_for_watering_m) {
      low_water_level_lockout = true;
    }

    if (pressure_sensor_height.AvgValue() > min_water_level_for_watering_restart_m) {
      low_water_level_lockout = false;
    }

    StrokeWDG();

    SetLEDBinary(0);
    
    DelayMilliseconds(100);
  }
}
