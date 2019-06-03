#!/usr/bin/env python3
""" Indoor environment controller.

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

import logging
import time
import threading

from dynamic_var import DynamicVar
from remote_handler import RemoteHandler

# Parameter list (sent periodically from MCU):
# TEMP <0.01 deg C> <location>
# HUMIDITY <0.1%> <location>

PORT=2939

class IndoorController():
  def __init__(self):
    self.logger = logging.getLogger('indoor')
    self.logger.setLevel(logging.DEBUG)
    self.logger.info("Indoor controller started")
    self.remote_handler = RemoteHandler(
      PORT,
      lambda addr: self.handle_remote_connected(addr),
      lambda line: self.handle_remote_receive(line),
      lambda: self.handle_remote_disconnected())

    # Measurements
    self.temperature = DynamicVar("Temperature", "TEMP", "/indoor/temperature", "C")
    self.humidity = DynamicVar("Humidity", "HUMIDITY", "/indoor/humidity", "%")

  def handle_remote_connected(self, addr):
    self.logger.info("Accepted connection from {}:{}".format(
                     addr[0], addr[1]))

  def handle_remote_receive(self, line):
    self.logger.debug("Received from remote controller: {}".format(line))
    parts = line.split(' ')
    if len(parts) != 3:
      self.logger.error("Invalid measurement from remote MCU: {}".format(line))
    measurement_type = parts[0]
    measurement = parts[1]
    location = parts[2]
    if measurement_type == 'TEMP':
      self.temperature.update(float(measurement) / 100)
    elif measurement_type == 'HUMIDITY':
      self.humidity.update(float(measurement) / 10)

  def handle_remote_disconnected(self):
    self.logger.info("Remote controller disconnected")

  def handle_http_get(self, path_elements, query_vars, authenticated):
    if len(path_elements) < 1:
      raise NameError()
    elif path_elements[0] == 'temperature':
      return self.temperature.get_value_str(), None
    elif path_elements[0] == 'humidity':
      return self.humidity.get_value_str(), None
    else:
      raise NameError()

  def handle_http_post(self, path_elements, data, authenticated):
    # No POST requests supported.
    raise NameError()

  def main_section_name(self):
    return 'Indoor Environment'

  def main_section_content(self):
    ret = ''
    ret += self.temperature.display_html()
    ret += self.humidity.display_html()
    return ret
