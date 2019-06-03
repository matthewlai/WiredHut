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
  # Internal name must be unique
  def __init__(self, display_name, internal_name, format_str = '{}'):
    self.lock = threading.Lock()
    self.display_name = display_name
    self.internal_name = internal_name
    self.format_str = format_str

  def update(self, new_value):
    with self.lock:
      self.value = new_value

  def get_value(self):
    with self.lock:
      if hasattr(self, 'value'):
        return self.value
      else:
        return 'No value'

  def get_value_str(self):
    with self.lock:
      if hasattr(self, 'value'):
        return self.format_str.format(self.value)
      else:
        return 'No value'

  def append_update(self, updates):
    updates.append('update_field_by_id("{internal_name}_display_span", "{value_str}");'.format(
        internal_name=self.internal_name, value_str=self.get_value_str()))

  def display_html(self):
    ret = ''
    value = self.get_value()
    ret += '<p>{display_name}: <span id="{internal_name}_display_span">No value</span></p>'.format(
        display_name=self.display_name, internal_name=self.internal_name)
    return ret