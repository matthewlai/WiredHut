/*
 * This file is part of the WiredHut project.
 *
 * Copyright (C) 2020 Matthew Lai <m@matthewlai.ca>
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

#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>

#include <esp_task_wdt.h>

#include <DHT.h>
#include <InfluxDbClient.h>
#include <PubSubClient.h>

#include <RateLimiter.h>

void log(const String& line);

#include "air_sensor.h"
#include "credentials.h"
#include "ina226.h"
#include "pressure_sensor.h"
#include "soil_moisture_sensor.h"
#include "solar.h"
#include "time_checker.h"
#include "watering_controller.h"

const int kWdtTimeoutSeconds = 60;

const char* kDeviceHostname = "garden";
const char* kEnvZone = "garden_mcu";

const char* kNtpServer = "pool.ntp.org";
const char* kTimezoneString = "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00";

const int kDhtPin = 25;

const int kLedPins[4] = { 19, 21, 22, 23 };
const int kSw0Pin = 4;
const int kSw1Pin = 2;
const int kPressureSensorChargePumpLedcChannel = 0;
const int kPressureSensorChargePumpPin = 13;
const int kPressureSensorChargePumpFrequency = 1000000;
const int kPressureSensorGatePin = 12;
const byte kPressureSensorAddress = 0x44;

const float kFullBatteryCapacity = 60.0f;

TwoWire I2CLocal = TwoWire(0); // I2C bus for current sensors, 400kHz.
const int kI2cSdaPin = 33;
const int kI2cSclPin = 32;
const int kI2cSpeed = 400000;
const int kCurrentSensorsAlertPin = 34;
const byte kMainBatterySensorAddress = 0x40;

TwoWire I2CExt = TwoWire(1); // I2C bus for external sensors at 50 kHz -
// soil moisture sensor at 0x20.
// air temperature/humidity sensor at 0x40.
const int kI2cExtSdaPin = 27;
const int kI2cExtSclPin = 26;
const int kI2cExtSpeed = 50000;

// We periodically store current battery SoC estimate in NVS (through the Preferences library),
// and read on boot.
// According to this: https://esp32.com/viewtopic.php?t=3990
// There is wear-levelling on NVS, so writing every minute is plenty slow enough.
const char* kBatterySocNvsFieldName = "battery_soc";
const int kBatterySocEepromSaveFrequencyMs = 60 * 1000;

const int kUpdatePeriodMs = 5 * 1000;
const uint32_t kMaxMqttPayloadLength = 32;

// Command can be "trigger_water", "reset_timer".
const char* kMqttWaterModeCommandTopic = "garden/water_command/set";

InfluxDBClient influxdb_client(kInfluxDbUrl, kInfluxDbName);

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
DHT dht_sensor(kDhtPin, DHT22);

HardwareSerial solar_controller_serial(1);

void log_to_influxdb(const String& line) {
  if (WiFi.status() == WL_CONNECTED) {
    Point pt("log");
    pt.addField("data", String("(") + kDeviceHostname + ") " + line);
    pt.addTag("device", kDeviceHostname);
    influxdb_client.writePoint(pt);
  }
}

void log(const String& line) {
  Serial.println(line);

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  static auto *log_limiter = []() {
    auto* limiter = new RateLimiter<10000, 10>;
    limiter->SetDroppedCallCallback([&](unsigned int dropped_calls) {
      log_to_influxdb(String("... dropped ") + dropped_calls + " calls...");
    });
    return limiter;
  }();

  log_limiter->CallOrDrop([&]() {
    log_to_influxdb(line);
  });
}

void ensure_connected_to_wifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Waiting for WiFi... ");
  WiFi.begin(kSsid, kPass);
  do {
    Serial.print(".");
    delay(500);
  } while (WiFi.status() != WL_CONNECTED);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP().toString());
}

void ensure_connected_to_mqtt() {
  if (mqtt_client.connected()) {
    return;
  }

  log("Connecting to MQTT... ");
  while (!mqtt_client.connected()) {
    Serial.print(".");
    if (mqtt_client.connect(kDeviceHostname, kMqttUser, kMqttPass)) {
      log("MQTT connected");
      mqtt_client.subscribe((String(kDeviceHostname) + "/#").c_str());
    }
    delay(500);
  }
}

String local_time_as_string(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "getLocalTime failed";
  }
  char output[80];
  strftime(output, 80, "%d-%b-%y, %H:%M:%S", &timeinfo);
  return output;
}

void setup()
{
  // We need to increase the frequency if we want to use the DHT sensor, because it can only be bit-banged at >= 160 MHz.
  setCpuFrequencyMhz(80);

  // Set up WDT to reboot.
  esp_task_wdt_init(kWdtTimeoutSeconds, true);
  esp_task_wdt_add(NULL); // Add current thread.
  
  delay(10);
  Serial.begin(115200);

  for (auto led_pin : kLedPins) {
    pinMode(led_pin, OUTPUT);
    // We have active low LEDs.
    digitalWrite(led_pin, 1);
  }

  pinMode(kSw0Pin, OUTPUT);
  pinMode(kSw1Pin, OUTPUT);

  pinMode(kCurrentSensorsAlertPin, INPUT);
  pinMode(kPressureSensorGatePin, OUTPUT);
  digitalWrite(kPressureSensorGatePin, 1);

  digitalWrite(kPressureSensorChargePumpPin, 0);

  // We don't actually need this. The sensor seems to work fine at 12V.
  // ledcSetup(kPressureSensorChargePumpLedcChannel, kPressureSensorChargePumpFrequency, 1);
  // ledcAttachPin(kPressureSensorChargePumpPin, 0);
  // ledcWrite(kPressureSensorChargePumpLedcChannel, 1);

  influxdb_client.setConnectionParamsV1(kInfluxDbUrl, kInfluxDbName, kInfluxDbUser, kInfluxDbPass);

  // Set high batch size. We will flush manually.
  influxdb_client.setWriteOptions(/*precision=*/WritePrecision::NS, /*batchSize=*/8, /*bufferSize=*/64);

  ensure_connected_to_wifi();

  ArduinoOTA.setHostname(kDeviceHostname);

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    log("Start updating " + type);
  })
  .onEnd([]() {
    log("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    log(String("Error ") + (uint32_t) error + ": ");
    if (error == OTA_AUTH_ERROR) log("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) log("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) log("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) log("Receive Failed");
    else if (error == OTA_END_ERROR) log("End Failed");
  });

  ArduinoOTA.begin();

  mqtt_client.setServer(kMqttHost, kMqttPort);
  ensure_connected_to_mqtt();

  dht_sensor.begin();

  I2CLocal.begin(kI2cSdaPin, kI2cSclPin, kI2cSpeed);
  I2CExt.begin(kI2cExtSdaPin, kI2cExtSclPin, kI2cExtSpeed);

  solar_controller_serial.begin(19200, SERIAL_8N1, 17, 16);

  // This starts a background task syncing time every 60 minutes.
  configTzTime(kTimezoneString, kNtpServer);
  log(String("NTP sync complete. Local time: ") + local_time_as_string());

  // 30 seconds delayed start for OTA in case we messed something up.
  while (millis() < 30000) {
    ArduinoOTA.handle();
  }
}

void write_battery_stats(Ina226* sensor) {
  if (sensor->HaveNewData()) {
    float voltage = sensor->BusVoltage();
    float current = sensor->ShuntCurrent();
    float q = sensor->AccumulatedChargeAh();
    Point pt("garden");
    pt.addField("batt_v", voltage, 4);
    pt.addField("batt_i", current, 4);
    pt.addField("batt_q_ah", q, 4);
    influxdb_client.writePoint(pt);
    sensor->ClearNewData();
  }
}

void loop() {
  static uint32_t last_loop_end_time = millis();

  uint32_t loop_start_time = millis();

  if ((loop_start_time - last_loop_end_time) > 1000) {
    log(String("Loop start was delayed by ") + (loop_start_time - last_loop_end_time) + "ms");
  }

  esp_task_wdt_reset();

  static Ina226 battery_sensor(&I2CLocal, kMainBatterySensorAddress, 0.012f, kCurrentSensorsAlertPin);
  static Solar solar_controller(&solar_controller_serial);
  static Ina226 pressure_current_sensor(&I2CLocal, kPressureSensorAddress, 2.2f, kCurrentSensorsAlertPin);
  static PressureSensor pressure_sensor(&pressure_current_sensor, kPressureSensorGatePin);
  static SoilMoistureSensor soil_sensor(&I2CExt);
  static WateringController watering_controller(kSw0Pin);
  static AirSensor air_sensor(&I2CExt);

  auto now = millis();

  static Preferences preferences;
  static bool first_iteration = true;

  if (first_iteration) {
    preferences.begin(kDeviceHostname, /*read_only=*/false);
    // Try to read last stored battery SoC.
    battery_sensor.SetRawAccumulatedCharge(preferences.getLong64(kBatterySocNvsFieldName, /*default=*/0));
    first_iteration = false;
  }

  static RateLimiter<kBatterySocEepromSaveFrequencyMs, 1> battery_soc_store_limiter;

  if (battery_sensor.HaveNewData()) {
    battery_soc_store_limiter.CallOrDrop([&]() {
      preferences.putLong64(kBatterySocNvsFieldName, battery_sensor.GetRawAccumulatedCharge());
    });
  }
  
  bool have_wifi = WiFi.status() == WL_CONNECTED;
  static bool last_have_wifi = false;
  static uint32_t lost_wifi_time = 0;

  if (have_wifi && !last_have_wifi) {
    if (lost_wifi_time == 0) {
      log("Wifi connected");
    } else {
      log(String("Wifi connected (lost for ") + (now - lost_wifi_time) + "ms");
    }
  }

  if (!have_wifi && last_have_wifi) {
    lost_wifi_time = now;
  }

  last_have_wifi = have_wifi;

  if (have_wifi) {
    CheckCallDuration([&]() { ArduinoOTA.handle(); }, "OTA Handling", 250);
    CheckCallDuration([&]() { ensure_connected_to_mqtt(); }, "MQTT Connection", 1000);

    mqtt_client.setCallback([&](char* topic, byte* payload, unsigned int length) {
      char payload_cs[kMaxMqttPayloadLength + 1];
      length = min(length, kMaxMqttPayloadLength);
      memcpy(payload_cs, payload, length);
      payload_cs[length] = '\0';
      if (String(topic) == kMqttWaterModeCommandTopic) {
        if (String(payload_cs) == "trigger_water") {
          watering_controller.TriggerWater();
          log("Manual watering started");
        } else if (String(payload_cs) == "reset_timer") {
          watering_controller.ResetTimer();
          log("Watering timer manually reset");
        } else {
          log(String("Unknown command: ") + payload_cs);
        }
      } else {
        log(String("Unknown topic: ") + topic);
      }
    });
    CheckCallDuration([&]() { mqtt_client.loop(); }, "MQTT loop", 250);
  }

  bool power_led_on = (now % 2000) < 50;
  digitalWrite(kLedPins[0], !power_led_on);

  CheckCallDuration([&]() { battery_sensor.Handle(); }, "Battery sensor", 250);
  CheckCallDuration([&]() { solar_controller.Handle(); }, "Solar controller", 250);

  if (solar_controller.IsFloating()) {
    battery_sensor.ResetAccumulatedCharge(kFullBatteryCapacity);
  }

  CheckCallDuration([&]() { pressure_current_sensor.Handle(); }, "Pressure current sensor", 250);
  CheckCallDuration([&]() { pressure_sensor.Handle(); }, "Pressure sensor", 250);
  CheckCallDuration([&]() { soil_sensor.Handle(); }, "Soil sensor", 250);
  CheckCallDuration([&]() { air_sensor.Handle(); }, "Air sensor", 250);
  CheckCallDuration([&]() { watering_controller.Handle(); }, "Watering controller", 250);

  static RateLimiter<kUpdatePeriodMs, 1> update_limiter;
  update_limiter.CallOrDrop([&]() {
    write_battery_stats(&battery_sensor);
    if (solar_controller.HaveNewData()) {
      Point solar_point = solar_controller.MakeInfluxDbPoint();
      influxdb_client.writePoint(solar_point);
      solar_controller.ClearNewDataFlag();
    }
    if (pressure_sensor.HaveNewData()) {
      Point water_point = pressure_sensor.MakeInfluxDbPoint();
      influxdb_client.writePoint(water_point);
      pressure_sensor.ClearNewDataFlag();
    }
    if (soil_sensor.HaveNewData()) {
      Point soil_point = soil_sensor.MakeInfluxDbPoint();
      influxdb_client.writePoint(soil_point);
      soil_sensor.ClearNewDataFlag();
    }
    if (air_sensor.HaveNewData()) {
      Point air_point = air_sensor.MakeInfluxDbPoint();
      influxdb_client.writePoint(air_point);
      air_sensor.ClearNewDataFlag();
    }
  });

  // Flush if we added any data in this iteration.
  if (!influxdb_client.isBufferEmpty()) {
    if (have_wifi) {
      CheckCallDuration([&]() { influxdb_client.flushBuffer(); }, "Flush influxdb buffer", 500);
    } else {
      Serial.println(String("No WiFi, skipping InfluxDB flush. Buffer full: ") + influxdb_client.isBufferFull());
    }
  }

  last_loop_end_time = millis();
  if ((last_loop_end_time - loop_start_time) > 1000) {
    log(String("Loop took ") + (last_loop_end_time - loop_start_time) + "ms");
  }

  delay(10);
}
