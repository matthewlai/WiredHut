/*
 * This file is part of the WiredHut project.
 *
 * Copyright (C) 2021 Matthew Lai <m@matthewlai.ca>
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

#ifndef __FIELDS_H__
#define __FIELDS_H__

const char* kValidKey = "valid";

const char* kWifiSsidKey = "wifi_ssid";
const char* kWifiPassKey = "wifi_pass";

const char* kInfluxDbUrlKey = "influxdb_url";
const char* kInfluxDbNameKey = "influxdb_name";
const char* kInfluxDbUserKey = "influxdb_user";
const char* kInfluxDbPassKey = "influxdb_pass";

const char* kMqttHostKey = "mqtt_host";
const char* kMqttPortKey = "mqtt_port";
const char* kMqttClientIdKey = "mqtt_client_id";
const char* kMqttUserKey = "mqtt_user";
const char* kMqttPassKey = "mqtt_pass";

const char* kHostnameKey = "hostname";
const char* kZoneKey = "zone";

#endif // __FIELDS_H__
