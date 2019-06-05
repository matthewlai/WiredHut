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
  return (sampler.ReadNormalized() - offset) * Config::kFullScalePumpCurrent;
}

float ReadSw1Current(ADC1_Type* adc) {
  static auto sampler =adc->GetGPIOInput<8>();
  static float offset = sampler.ReadNormalized();
  return (sampler.ReadNormalized() - offset) * Config::kFullScaleSw1Current;
}

float ReadPressureSensorCurrent(ADC1_Type* adc) {
  static auto sampler =adc->GetGPIOInput<15>();
  static float offset = sampler.ReadNormalized();
  return (sampler.ReadNormalized() - offset) *
      Config::kPressureSensorFullScaleCurrent;
}

float ReadTemperatureInput(ADC1_Type* adc) {
  static auto sampler = adc->GetTemperatureInput();
  return sampler.ReadTempC();
}

float PressureSensorCurrentToHeightMm(float current) {
  return (current - 0.004f) / 0.02f * 5.0f;
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
  SetLED0(false);
  SetLED1(false);
  if (p->ConnectToAP(Config::kSSID, Config::kPass)) {
    SetLED0(true);
  } else {
    p.reset();
    return p;
  }
  if (p->ConnectToTCPServer(Config::kEnvironmentControllerLinkId,
                            Config::kHubHost,
                            Config::kEnvironmentControllerPort) &&
      p->ConnectToTCPServer(Config::kGardenControllerLinkId, Config::kHubHost,
                            Config::kGardenControllerPort)) {
    SetLED1(true);
  } else {
    p.reset();
    return p;
  }
  return p;
}

int main() {
  SingleConversionADC<ADC1> adc1;

  SetPump(false);
  SetSw1(false);
  SetPressureSensorPower(false);

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
  int water_time_seconds = 10 * 60;
  int time_between_watering = 48 * 60 * 60;
  float min_water_level_for_watering_m = 0.15f;

  bool watering_now = false;
  int64_t last_watering_time_seconds = 0;
  int64_t watering_start_time = 0;

  while (true) {
    DelayMilliseconds(5000);

    // For calibration.
    ReadPressureSensorCurrent(&adc1);
    ReadPumpCurrent(&adc1);
    ReadSw1Current(&adc1);

    auto esp8266 = ConnectToHub();

    if (!esp8266) {
      continue;
    }

    ThrottledExecutor send_update_throttle(1000);

    bool pump_on = false;
    float pump_current = 0.0f;
    float pressure_sensor_height = 0.0f;
    float soil_moisture = 0;
    float soil_temperature = 0;

    // Solar
    bool solar_data_ready = false;
    float batt_voltage = 0.0f;
    float batt_current = 0.0f;
    float solar_voltage = 0.0f;
    float solar_current = 0.0f;
    float load_current = 0.0f;
    std::string solar_mode;
    std::string mppt_mode;
    int solar_error_code = 0;

    while (true) {
      while (smartsolar.LineAvailable()) {
        std::string line = smartsolar.GetLine();
        StringStream<64> ss(line);
        std::string field_label;
        int value;
        ss >> field_label;
        ss >> value;

        usb_serial << "[Solar] Field: " << field_label << ", Value: " << value << std::endl;

        if (field_label == "V") {
          batt_voltage = float(value) / 1000.0f;
        } else if (field_label == "I") {
          batt_current = float(value) / 1000.0f;
        } else if (field_label == "VPV") {
          solar_voltage = float(value) / 1000.0f;
        } else if (field_label == "PPV") {
          // Not sure why we get PV power instead of current, but we can convert
          // it (PPV is always received after the corresponding VPV)
          float watts = value;
          solar_current = watts / solar_voltage;
        } else if (field_label == "IL") {
          load_current = float(value) / 1000.0f;
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
              break;
            default:
              solar_mode = Format(value);
          }
        } else if (field_label == "ERR") {
          solar_error_code = value;
        } else if (field_label == "Checksum") {
          // Checksum is guaranteed to be the last in a block.
          solar_data_ready = true;
        }
      }

      if (send_update_throttle.ExecuteNow()) {
        SetPressureSensorPower(true);
        // Spec sheet says 20ms max startup time. We leave some more time for
        // the pressure sensor.
        DelayMilliseconds(30);
        pressure_sensor_height =
            PressureSensorCurrentToHeightMm(ReadPressureSensorCurrent(&adc1));
        SetPressureSensorPower(false);

        soil_moisture = I2CReadRegister(&moisture_i2c,
                                        Config::kMoistureRegister);
        soil_temperature = float(I2CReadRegister(&moisture_i2c,
                                        Config::kTemperatureRegister)) / 10.0f;

        soil_moisture -= Config::kSoilMoistureMin;
        soil_moisture /= Config::kSoilMoistureMax - Config::kSoilMoistureMin;
        soil_moisture *= 100;

        pump_current = ReadPumpCurrent(&adc1);

        if (!esp8266->SendData(Config::kEnvironmentControllerLinkId, 
            "TEMP " + Format(int(ReadTemperatureInput(&adc1) * 100)) +
            " garden_mcu\n")) {
          break;
        }

        if (!esp8266->SendData(Config::kEnvironmentControllerLinkId, 
            "TEMP " + Format(int(soil_temperature * 100.0f)) +
            " soil\n")) {
          break;
        }

        if (!esp8266->SendData(Config::kGardenControllerLinkId, 
            "PUMP_ON " + Format(int(pump_on)) + "\n")) {
          break;
        }

        if (!esp8266->SendData(Config::kGardenControllerLinkId, 
            "PUMP_I " + Format(int(pump_current * 1000)) + "\n")) {
          break;
        }

        if (!esp8266->SendData(Config::kGardenControllerLinkId, 
            "WATER_LEVEL " + Format(int(pressure_sensor_height * 1000)) + "\n")) {
          break;
        }

        if (!esp8266->SendData(Config::kGardenControllerLinkId, 
            "SOIL_MOISTURE " + Format(int(soil_moisture)) + "\n")) {
          break;
        }

        if (solar_data_ready) {
          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "SOL_V " + Format(int(solar_voltage * 1000)) + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "SOL_I " + Format(int(solar_current * 1000)) + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "BATT_V " + Format(int(batt_voltage * 1000)) + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "BATT_I " + Format(int(batt_current * 1000)) + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "LOAD_I " + Format(int(load_current * 1000)) + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "SOLAR_MODE " + solar_mode + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "MPPT_MODE " + mppt_mode + "\n")) {
            break;
          }

          if (!esp8266->SendData(Config::kGardenControllerLinkId, 
              "SOL_ERR " + Format(solar_error_code) + "\n")) {
            break;
          }

          solar_data_ready = false;
        }
      }

      int64_t time_now_seconds = GetTimeMilliseconds() / 1000LL;
      if (watering_now) {
        SetPump(true);

        if (pressure_sensor_height < min_water_level_for_watering_m ||
            (time_now_seconds - watering_start_time) > water_time_seconds) {
          last_watering_time_seconds = time_now_seconds;
          watering_now = false;
          SetPump(false);
        }
      } else {
        SetPump(false);

        int64_t time_since_last_water =
            time_now_seconds - last_watering_time_seconds;
        if (time_since_last_water > time_between_watering && 
            pressure_sensor_height > min_water_level_for_watering_m) {
          watering_now = true;
          watering_start_time = time_now_seconds;
          SetPump(true);
        }
      }
      
      DelayMilliseconds(100);
    }
  }
}