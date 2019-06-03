#!/usr/bin/env python3
""" Convenience wrapper for a dynamic variable.

Copyright (C) 2019 Matthew Lai <m@matthewlai.ca>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
"""

import threading

class DynamicVar():
  def __init__(self, display_name, internal_name, load_path, unit, decimal_places = 3):
    self.lock = threading.Lock()
    self.display_name = display_name
    self.internal_name = internal_name
    self.load_path = load_path
    self.unit = unit
    self.decimal_places = decimal_places
    self.value = 'invalid'

  def update(self, new_value):
    with self.lock:
      self.value = new_value

  def get_value(self):
    with self.lock:
      return self.value

  def get_value_str(self):
    with self.lock:
      return ('{:.' + str(self.decimal_places) + 'f}').format(self.value)

  def display_html(self):
    ret = ''
    value = self.get_value()
    ret += '<script>load_val("{load_path}", "{internal_name}_display_span", 1);</script>'.format(
        load_path=self.load_path, internal_name = self.internal_name)
    ret += '<p>{display_name}: <span id="{internal_name}_display_span">No val</span>{unit}</p>'.format(
        display_name=self.display_name, internal_name=self.internal_name, unit=self.unit)
    return ret