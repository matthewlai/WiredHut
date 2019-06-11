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

from collections import namedtuple
import logging
import time
import threading

import config
from dynamic_var import DynamicVar
from remote_handler import RemoteHandler

# Parameter list (sent periodically from MCU):
# TEMP <0.01 deg C> <location>
# HUMIDITY <0.1%> <location>

LocationInfo = namedtuple('LocationInfo', ['display_name', 'has_temp',
                                           'has_hum', 'temp_offset'],
                                          defaults=(0,))

LOCATIONS={
  'indoor': LocationInfo(display_name='Indoor', has_temp=True, has_hum=True),
  'garden_mcu': LocationInfo(display_name='Garden MCU', has_temp=True, has_hum=False, temp_offset=3.8),
  'soil': LocationInfo(display_name='Soil', has_temp=True, has_hum=False)
}

class IndoorController():
  def __init__(self):
    self.logger = logging.getLogger('indoor')
    self.logger.setLevel(logging.DEBUG)
    self.logger.info("Indoor controller started")
    self.remote_handler = RemoteHandler(
      config.INDOOR_PORT,
      lambda addr: self.handle_remote_connected(addr),
      lambda addr, line: self.handle_remote_receive(addr, line),
      lambda addr: self.handle_remote_disconnected(addr))

    # Measurements
    self.temperatures = {}
    self.humidities = {}

    for location, info in LOCATIONS.items():
      if info.has_temp:
        self.temperatures[location] = DynamicVar("Temperature",
            "indoor_temp_{}".format(location), format_str='{0:.2f}°C')
      if info.has_hum:
        self.humidities[location] = DynamicVar("Humidity",
            "indoor_hum_{}".format(location), format_str='{0:.2f}%')

  def handle_remote_connected(self, addr):
    self.logger.info("Accepted connection from {}:{}".format(
                     addr[0], addr[1]))

  def handle_remote_receive(self, addr, line):
    self.logger.debug("Received from remote controller: {}".format(line))
    parts = line.split(' ')
    if len(parts) != 3:
      self.logger.error("Invalid measurement from remote MCU: {}".format(line))
      return
    measurement_type = parts[0]
    measurement = parts[1]
    location = parts[2]
    if not location in LOCATIONS:
      self.logger.error("Received environment update from unknown location: {}".format(location))
      return
    if measurement_type == 'TEMP' and LOCATIONS[location].has_temp:
      offset = LOCATIONS[location].temp_offset
      self.temperatures[location].update(float(measurement) / 100 + offset)
    elif measurement_type == 'HUMIDITY' and LOCATIONS[location].has_hum:
      self.humidities[location].update(float(measurement) / 10)
    else:
      self.logger.error("Received environment update with unknown field: {}".format(measurement_type))

  def handle_remote_disconnected(self, addr):
    self.logger.info("Remote controller disconnected")

  def handle_http_get(self, path_elements, query_vars, authenticated):
    raise NameError()

  def handle_http_post(self, path_elements, data, authenticated):
    # No POST requests supported.
    raise NameError()

  def main_section_name(self):
    return 'Environment'

  def main_section_content(self):
    ret = ''
    for location, info in LOCATIONS.items():
      ret += '<h3>{}</h3>'.format(info.display_name)
      if info.has_temp:
        ret += self.temperatures[location].display_html()
      if info.has_hum:
        ret += self.humidities[location].display_html()
    return ret

  def append_updates(self, updates):
    for location, info in LOCATIONS.items():
      if info.has_temp:
        self.temperatures[location].append_update(updates)
      if info.has_hum:
        self.humidities[location].append_update(updates)

  def append_all_variables(self, variables):
    for location, info in LOCATIONS.items():
      if info.has_temp:
        variables.append(self.temperatures[location])
      if info.has_hum:
        variables.append(self.humidities[location])
