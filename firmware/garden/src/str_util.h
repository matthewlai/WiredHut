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

#ifndef __STR_UTIL_H__
#define __STR_UTIL_H__

#include <string>
#include <vector>

std::vector<std::string> Split(const std::string& s, char delim) {
  std::vector<std::string> ret;
  std::string current_field;
  for (const auto &c : s) {
    if (c == delim) {
      ret.push_back(current_field);
      current_field.clear();
    } else {
      current_field.push_back(c);
    }
  }

  ret.push_back(current_field);
  return ret;
}

std::string RemoveAll(const std::string& s, char to_remove) {
  std::string ret;
  for (const char& c : s) {
    if (c != to_remove) {
      ret.push_back(c);
    }
  }
  return ret;
}

#endif // __STR_ UTIL_H__
