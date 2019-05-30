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
### We include a duration to make sure we don't flood the garden if
### communication fails while watering is in progress.
# START_WATER <seconds>
# STOP_WATER

### Fallback
### If we lose communication, we want the firmware to take over sensing and
### watering. These functions set the algorithm parameters.
# SET_FALLBACK_WATER_THRESHOLD <moisture %>
# SET_FALLBACK_WATER_TIME <seconds>
# SET_FALLBACK_MIN_TIME_BETWEEN_WATERING <seconds>
# SET_FALLBACK_MAX_TIME_BETWEEN_WATERING <seconds>
# SET_FALLBACK_MIN_WATER_LEVEL <mm>


class GardenController(threading.Thread):
  def __init__(self, addr_port):
    threading.Thread.__init__(self)
    self.addr_port = addr_port
    self.start()

  def run(self):
    self.mcu_socket = (socket.AF_INET, socket.SOCK_STREAM)
    self.mcu_socket.connect(self.addr_port)

    while True:
      time.sleep(1)

  def handle_http_get(self, path_elements, authenticated):
    if len(path_elements) == 0:
      return "Garden main", None
    else:
      raise NameError()