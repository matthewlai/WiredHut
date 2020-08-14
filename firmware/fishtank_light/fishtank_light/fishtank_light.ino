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
#include <WiFiMulti.h>
#include <ArduinoOTA.h>
#include <RateLimiter.h>

#include <PubSubClient.h>
#include <InfluxDbClient.h>

#include "time.h"

#include "credentials.h"

WiFiMulti WiFiMulti;

const char* device_hostname = "fishtank_light";

const char* kNtpServer = "pool.ntp.org";
const long kGmtOffsetSeconds = 0;
const int kDaylightOffsetSeconds = 3600;

const int kLedPins[4] = { 23, 22, 21, 19 };
const int kLightPins[8] = { 25, 26, 27, 14, 13, 2, 4, 16 };

const int kPwmFreq = 240;
const int kPwmResolution = 10;

// Below this duty cycle the lights seem to draw current in short bursts, quickly
// heating up the FETs.
const int kMinDutyCycle = 256;

const float kLightStartTimes[8] = { 7.0f, 8.0f, 9.0f, 10.0f, 10.5f, 11.0f, 11.5f, 12.0f };
const float kLightEndTimes[8] = { 17.0f, 17.5f, 18.0f, 18.5f, 19.5f, 20.5f, 22.5f, 23.5f };
const bool kLightDimmable[8] = { true, true, false, false, false, false, true, true };

const int kTimeSyncIntervalSeconds = 3600;

const int kMaxMqttPayloadLength = 32;

const char* kMqttDemoModeNotifyTopic = "fishtank_light/demo_mode";
const char* kMqttDemoModeSetTopic = "fishtank_light/demo_mode/set";

InfluxDBClient influxdb_client(kInfluxDbUrl, kInfluxDbName);

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

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

float tanh(float x)
{
  float e_x = exp(x);
  float e_nx = 1.0 / e_x;

  return ((e_x - e_nx) / (e_x + e_nx));
}

float light_intensity(float hour, float light_start_time, float light_end_time) {
  return 0.5f * (tanh(2.0f * (hour - light_start_time)) - tanh(2.0f * (hour - light_end_time)));
}

bool demo_mode = false;

float get_hour() {
  tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    log("Failed to get time");
    return 0.0f;
  }

  if (demo_mode) {
    return (millis() % 24000ULL) / 1000.0f;
  } else {
    return timeinfo.tm_hour + (timeinfo.tm_min / 60.0f) + (timeinfo.tm_sec / 60.0f / 60.0f);
  }
}

void ensure_connected_to_wifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  log("Waiting for WiFi... ");
  digitalWrite(kLedPins[1], 1);
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  digitalWrite(kLedPins[1], 0);
  log("WiFi connected");
  log("IP address: ");
  log(WiFi.localIP());
}

void ensure_mqtt_connected() {
  if (mqtt_client.connected()) {
    return;
  }

  log("Connecting to MQTT... ");
  while (!mqtt_client.connected()) {
    Serial.print(".");
    if (mqtt_client.connect(kMqttClientId, kMqttUser, kMqttPass)) {
      log("MQTT connected");
      mqtt_client.subscribe("fishtank_light/#");
    }
    delay(500);
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char payload_cs[kMaxMqttPayloadLength + 1];
  if (length > kMaxMqttPayloadLength) {
    length = kMaxMqttPayloadLength;
  }
  memcpy(payload_cs, payload, length);
  payload_cs[length] = '\0';
  if (String(topic) == kMqttDemoModeSetTopic) {
    demo_mode = true;
    if (String(payload_cs) == "1") {
      demo_mode = true;
      mqtt_client.publish(kMqttDemoModeNotifyTopic, "1", /*retained=*/true);
      log("Demo mode on");
    } else {
      demo_mode = false;
      mqtt_client.publish(kMqttDemoModeNotifyTopic, "0", /*retained=*/true);
      log("Demo mode off");
    }
  }
}

void setup()
{
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  delay(10);

  for (auto led_pin : kLedPins) {
    pinMode(led_pin, OUTPUT);
  }

  int ledc_channel = 0;
  for (auto light_pin : kLightPins) {
    pinMode(light_pin, OUTPUT);
    ledcSetup(ledc_channel, kPwmFreq, kPwmResolution);
    ledcAttachPin(light_pin, ledc_channel);
    ++ledc_channel;
  }

  // We start by connecting to a WiFi network
  WiFiMulti.addAP(kSsid, kPass);

  influxdb_client.setConnectionParamsV1(kInfluxDbUrl, kInfluxDbName, kInfluxDbUser, kInfluxDbPass);

  ensure_connected_to_wifi();

  configTime(kGmtOffsetSeconds, kDaylightOffsetSeconds, kNtpServer);

  ArduinoOTA.setHostname(device_hostname);

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
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) log("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) log("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) log("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) log("Receive Failed");
    else if (error == OTA_END_ERROR) log("End Failed");
  });

  ArduinoOTA.begin();

  mqtt_client.setServer(kMqttHost, kMqttPort);
  mqtt_client.setCallback(mqtt_callback);

  ensure_mqtt_connected();

  mqtt_client.publish(kMqttDemoModeNotifyTopic, "0", /*retained=*/true);
}

void loop() {
  static RateLimiter<kTimeSyncIntervalSeconds * 1000, 1> ntp_update_limiter;
  static RateLimiter<60000, 1> send_data_limiter;

  ensure_connected_to_wifi();
  ensure_mqtt_connected();
  ArduinoOTA.handle();
  mqtt_client.loop();

  ntp_update_limiter.CallOrDrop([&]() {
    configTime(kGmtOffsetSeconds, kDaylightOffsetSeconds, kNtpServer);
  });

  tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    log("Failed to obtain time");
    return;
  }

  float hour = get_hour();
  static int last_lights_on = 0;
  int lights_on = 0;

  for (int ledc_channel = 0; ledc_channel < 8; ++ledc_channel) {
    int duty = round(1024.0f * light_intensity(hour, kLightStartTimes[ledc_channel], kLightEndTimes[ledc_channel]));
    if (duty < kMinDutyCycle) {
      duty = 0;
    }
    if (!kLightDimmable[ledc_channel]) {
      if (duty >= 512) {
        duty = 1023;
      } else {
        duty = 0;
      }
    }
    if (duty > 0) {
      ++lights_on;
    }
    ledcWrite(ledc_channel, duty);
  }

  if (lights_on != last_lights_on) {
    last_lights_on = lights_on;
    log(String(lights_on) + " lights on now");
  }

  send_data_limiter.CallOrDrop([&]() {
    Point pt("fishtank_light");
    pt.addField("Lights On", lights_on);
    influxdb_client.writePoint(pt);
  });

  delay(50);
}
