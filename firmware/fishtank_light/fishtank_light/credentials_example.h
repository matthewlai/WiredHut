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

#ifndef __CREDENTIALS_H__
#define __CREDENTIALS_H__

const char* kSsid = "x";
const char* kPass = "x";

// InfluxDB
const char* kInfluxDbUrl = "http://x:8086";
const char* kInfluxDbName = "x";
const char* kInfluxDbUser = "x";
const char* kInfluxDbPass = "x";

// MQTT
const char* kMqttHost = "192.168.5.150";
const int kMqttPort = 1883;
const char* kMqttClientId = "fishtank_light";
const char* kMqttUser = "x";
const char* kMqttPass = "x";

#endif // __CREDENTIALS_H__
