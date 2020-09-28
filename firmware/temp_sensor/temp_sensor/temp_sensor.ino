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

#include <WiFi.h>

#include <PubSubClient.h>
#include <InfluxDbClient.h>
#include <DHT.h>

#include <RateLimiter.h>

#include "time.h"

#include "credentials.h"

//const char* device_hostname = "temp_workshop";
//const char* zone = "workshop";

const char* device_hostname = "temp_living";
const char* zone = "living";

// const char* device_hostname = "test";
// const char* zone = "test";

const int kLedPins[4] = { 23, 22, 21, 19 };

const int kDhtPin = 17;

const int kReadingPeriodMs = 30 * 1000;

InfluxDBClient influxdb_client(kInfluxDbUrl, kInfluxDbName);

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
DHT dht_sensor(kDhtPin, DHT22);

void log_to_influxdb(const String& line) {
  if (WiFi.status() == WL_CONNECTED) {
    Point pt("log");
    pt.addField("data", String("(") + device_hostname + ") " + line);
    pt.addTag("device", device_hostname);
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

  WiFi.begin(kSsid, kPass);
  Serial.println("Waiting for WiFi... ");
  do {
    Serial.print(".");
    delay(200);
  } while (WiFi.status() != WL_CONNECTED);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP().toString());
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  for (auto led_pin : kLedPins) {
    pinMode(led_pin, OUTPUT);
  }

  influxdb_client.setConnectionParamsV1(kInfluxDbUrl, kInfluxDbName, kInfluxDbUser, kInfluxDbPass);

  ensure_connected_to_wifi();

  dht_sensor.begin();
}

void loop() {
  // We will only be running one iteration of the loop before going into deep sleep.
  float temp = dht_sensor.readTemperature();
  float humidity = dht_sensor.readHumidity();
  if (isnan(temp) || isnan(humidity)) {
    log("DHT22 read failed. Retrying in 3 seconds");
    delay(3000);
    temp = dht_sensor.readTemperature();
    humidity = dht_sensor.readHumidity();
  }

  if (isnan(temp) || isnan(humidity)) {
    log("Retry failed. Not sending data");
  } else {
    Point pt("env");
    Serial.println(String("Temp: ") + temp + " Humidity: " + humidity);
    pt.addField(String(zone) + "_temp", temp);
    pt.addField(String(zone) + "_humidity", humidity);
    
    influxdb_client.writePoint(pt);
  }

  // Close all the things that need closing.
  influxdb_client.flushBuffer();
  mqtt_client.disconnect();
  WiFi.disconnect();

  // Go to sleep!
  Serial.println("Going to sleep");
  esp_sleep_enable_timer_wakeup(kReadingPeriodMs * 1000);
  esp_deep_sleep_start();

  // This should never be reached. Left here in case the deep sleep fails for whatever reason.
  delay(kReadingPeriodMs);
}
