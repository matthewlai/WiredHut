#!/usr/bin/env python3
""" Garden controller.

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
from io import StringIO
import time

from remote_handler import RemoteHandler

# Parameter list (sent periodically from MCU):
# SOL_V (solar voltage, millivolts)
# SOL_I (solar current, milliamps)
# BATT_V (battery voltage, millivolts)
# BATT_I (battery current, milliamps, discharge is negative)
# LOAD_I (total load current, milliamps)
# PUMP_I (pump current, milliamps)
# WATER_LEVEL (water level, mm)
# SOIL_MOISTURE (soil moisture, %)
# SOIL_TEMPERATURE (soil temperature, 0.1C)
# WIFI_SIGNAL_STRENGTH (dBm)

# Commands:
### Watering
### Watering is normally controlled by the MCU firmware, but we include some
### overrides here. They both have timeouts so if we lose communication we won't
### flood the garden or starve the plants. Time is ignored for auto mode (but
### still must be sent for easier parsing on the firmware side).
# SET_MODE <AUTO/ON/OFF> <seconds>

### These functions set auto-watering algorithm parameters.
# SET_WATER_THRESHOLD <moisture %>
# SET_WATER_TIME <seconds>
# SET_MIN_TIME_BETWEEN_WATERING <seconds>
# SET_MAX_TIME_BETWEEN_WATERING <seconds>
# SET_MIN_WATER_LEVEL <mm>

PORT=2938

class GardenController():
  def __init__(self):
    self.logger = logging.getLogger('garden')
    self.logger.setLevel(logging.DEBUG)
    self.logger.info("Garden controller started")
    self.remote_handler = RemoteHandler(
      PORT,
      lambda addr: self.handle_remote_connected(addr),
      lambda line: self.handle_remote_receive(line),
      lambda: self.handle_remote_disconnected())

  def handle_remote_connected(self, addr):
    self.logger.info("Accepted connection from {}:{}".format(
                     addr[0], addr[1]))

  def handle_remote_receive(self, line):
    self.logger.debug("Received from remote controller: {}".format(line))

  def handle_remote_disconnected(self):
    self.logger.info("Remote controller disconnected")

  def handle_http_get(self, path_elements, query_vars, authenticated):
    raise NameError()

  def handle_http_post(self, path_elements, data, authenticated):
    # All POST require authentication
    if not authenticated:
      raise PermissionError()
    if len(path_elements) == 1 and path_elements[0] == 'send_remote':
      self.logger.info("Received {}".format(data))
      return '', None
    else:
      raise NameError()

  def main_section_name(self):
    return 'Garden'

  def main_section_content(self):
    return 'Garden content'

  def append_updates(self, updates):
    pass