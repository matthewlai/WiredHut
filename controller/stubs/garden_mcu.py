#!/usr/bin/env python3
""" Test script to simulate a garden microcontroller.

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

import math
import socket
import time

ADDR_PORT=('', 2938)


def SampleSine(min_val, max_val, period, t):
  dyn_range = max_val - min_val
  return dyn_range / 2 * math.sin(t * 2 * math.PI / period) + min_val


def SendVal(connection, var_name, val):
  connection.send('{} {}\n'.format(var_name, val).encode('utf-8'))


def listen():
  connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  connection.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  connection.bind(ADDR_PORT)
  connection.listen(10)
  while True:
    current_connection, address = connection.accept()
    t = 0
    while True:
      SendVal(current_connection, 'SOL_V', SampleSine(15, 22, 5))
      time.sleep(1)
      t += 1


def main():
  listen()


if __name__ == '__main__':
  main()
