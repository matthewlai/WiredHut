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

#include <esp_task_wdt.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>

#include "Adafruit_SHT31.h"

#include <InfluxDbClient.h>

#include <RateLimiter.h>

#include "time.h"

const int kReadingPeriodMs = 60 * 1000;

const int kWdtTimeoutSeconds = 15;

const int kSensorSdaPin = 4;
const int kSensorSclPin = 16;

InfluxDBClient* influxdb_client;

WiFiClient wifi_client;

Preferences* pref = nullptr;

void log_to_influxdb(const String& line) {
  if (WiFi.status() == WL_CONNECTED) {
    Point pt("log");
    pt.addField("data", String("(") + pref->getString("hostname", "") + ") " + line);
    pt.addTag("device", pref->getString("hostname", ""));
    influxdb_client->writePoint(pt);
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
  WiFi.begin(pref->getString("wifi_ssid", "").c_str(), pref->getString("wifi_pass", "").c_str());
  int dot = 0;
  do {
    Serial.print(".");
    ++dot;
    if (dot % 200 == 0) {
      Serial.print("\n");
    }
    delay(200);
  } while (WiFi.status() != WL_CONNECTED);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP().toString());
}

void setup()
{
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(10);

  esp_task_wdt_init(kWdtTimeoutSeconds, true);
  esp_task_wdt_add(nullptr);

  pref = new Preferences;
  pref->begin("pref", /*read_only=*/true);
  if (!pref->getBool("valid", false)) {
    while (true) {
      Serial.println("Preferences not initialised");
      delay(1000);
    }
  }

  influxdb_client = new InfluxDBClient(pref->getString("influxdb_url", "").c_str(), pref->getString("influxdb_name", "").c_str());

  influxdb_client->setConnectionParamsV1(pref->getString("influxdb_url", "").c_str(),
                                         pref->getString("influxdb_name", "").c_str(),
                                         pref->getString("influxdb_user", "").c_str(),
                                         pref->getString("influxdb_pass", "").c_str());

  TwoWire tw(0);
  tw.begin(kSensorSdaPin, kSensorSclPin, 400000);
  Adafruit_SHT31 sht31 = Adafruit_SHT31(&tw);

  sht31.begin();
  
  float temp = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  Serial.println(String("Temp: ") + temp + "C  Humidity: " + humidity + "%");

  // We do the reading before enabling wifi, so it's less affected by heating of the chip.
  ensure_connected_to_wifi();
  
  Point pt("env");
  pt.addField(pref->getString("zone", "") + "_temp", temp);
  pt.addField(pref->getString("zone", "") + "_humidity", humidity);
  
  influxdb_client->writePoint(pt);

  // Close all the things that need closing.
  influxdb_client->flushBuffer();
  WiFi.disconnect();

  // Go to sleep!
  Serial.println("Going to sleep");
  esp_sleep_enable_timer_wakeup(kReadingPeriodMs * 1000);
  esp_task_wdt_delete(nullptr);
  esp_deep_sleep_start();
}

void loop() {
  // This should never be reached. Left here in case the deep sleep fails for whatever reason.
  delay(kReadingPeriodMs);
}
