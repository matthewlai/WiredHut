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

#include <Preferences.h>

#include "fields.h"
#include "data.h"

void WritePrefValue(Preferences* pref, const char* key, int64_t x) {
  pref->putLong64(key, x);
}

void WritePrefValue(Preferences* pref, const char* key, const char* x) {
  pref->putString(key, x);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Preferences pref;
  pref.begin("pref", /*read_only=*/false);
  pref.clear();

  Serial.println("Starting writes");

  #define ENTRY(k, v) WritePrefValue(&pref, k, v); Serial.println(String(k) + ": " + v);
  NVS_DATA
  #undef ENTRY

  pref.putBool(kValidKey, true);
  
  pref.end();
  Serial.println("All done!");
}

void loop() {}
