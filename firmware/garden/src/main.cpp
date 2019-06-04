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
  pwr = on;
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
    float pressure_sensor_current = 0.0f;

    while (true) {
      if (send_update_throttle.ExecuteNow()) {
        SetPressureSensorPower(true);
        // Spec sheet says 20ms max startup time. We leave some more time for
        // the pressure sensor.
        DelayMilliseconds(30);
        pressure_sensor_current = ReadPressureSensorCurrent(&adc1);
        SetPressureSensorPower(false);

        if (!esp8266->SendData(Config::kEnvironmentControllerLinkId, 
            "TEMP " + Format(int(ReadTemperatureInput(&adc1) * 100)) +
            " garden_mcu\n")) {
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
            "PRES_I " + Format(int(pressure_sensor_current * 10000)) + "\n")) {
          break;
        }
      }

      //pump_current = ReadPumpCurrent(&adc1);
      pump_current = ReadPumpCurrent(&adc1);

      SetSw1(pump_on);
      
      DelayMilliseconds(100);
    }
  }
}