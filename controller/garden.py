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

import time
import threading

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

import logging
from io import StringIO
import socket
import time

from remote_handler import RemoteHandler

PORT=2938

class GardenController(threading.Thread):
  def __init__(self):
    threading.Thread.__init__(self)
    self.daemon = True
    self.logger = logging.getLogger('garden')
    self.logger.setLevel(logging.DEBUG)
    self.logger.info("Garden controller started")
    self.remote_handler = RemoteHandler(
      PORT,
      lambda addr: self.handle_remote_connected(addr),
      lambda line: self.handle_remote_receive(line),
      lambda: self.handle_remote_disconnected())
    self.start()

  def run(self):
    while True:
      time.sleep(1)

  def handle_remote_connected(self, addr):
    self.logger.info("Garden controller accepted connection from {}:{}".format(
                     addr[0], addr[1]))

  def handle_remote_receive(self, line):
    self.logger.info("Garden controller received {}".format(line))

  def handle_remote_disconnected(self):
    self.logger.info("Garden controller remote controller disconnected")

  def handle_http_get(self, path_elements, authenticated):
    if len(path_elements) == 0:
      return "Garden main", None
    else:
      raise NameError()