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

#ifndef __DATA_H__
#define __DATA_H__

#define NVS_DATA \
  ENTRY(kWifiSsidKey, "m"); \
  ENTRY(kWifiPassKey, "asdf1234"); \
  ENTRY(kInfluxDbUrlKey, "http://192.168.5.150:8086"); \
  ENTRY(kInfluxDbNameKey, "home"); \
  ENTRY(kInfluxDbUserKey, "access"); \
  ENTRY(kInfluxDbPassKey, "Z0TtAB1i61fypBf5weudo7fa3c4qoM"); \
  ENTRY(kMqttHostKey, "192.168.5.150"); \
  ENTRY(kMqttPortKey, 1883); \
  ENTRY(kMqttClientIdKey, "mqtt_client"); \
  ENTRY(kMqttUserKey, "access"); \
  ENTRY(kMqttPassKey, "Z0TtAB1i61fypBf5weudo7fa3c4qoM"); \
  ENTRY(kHostnameKey, "pwr"); \
  ENTRY(kZoneKey, "pwr");

#endif // __DATA_H__
