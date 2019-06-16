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

import math
import socket
import time

ADDR_PORT=('localhost', 2938)


def SampleSine(min_val, max_val, period, t):
  dyn_range = max_val - min_val
  return dyn_range / 2 * math.sin(t * 2 * math.pi / period) + min_val


def SendVal(connection, var_name, val):
  to_send = '{} {}\n'.format(var_name, val).encode('utf-8')
  print(to_send)
  connection.sendall(to_send)


def main():
  while True:
    connection = socket.create_connection(ADDR_PORT, 10)
    connection.settimeout(0.05)

    t = 0
    while True:
      try:
        recv_data = None
        recv_data = connection.recv(4096)
        if recv_data:
          print("Received: {}".format(recv_data.decode('utf-8')))
        else:
          # Connection closed normally.
          break
      except socket.timeout:
        pass
      except:
        # Connection closed not so normally.
        break
      #SendVal(connection, 'SOL_V', SampleSine(15, 22, 5, t))
      #SendVal(connection, 'SOL_I', SampleSine(2, 20, 20, t))
      #SendVal(connection, 'BATT_V', SampleSine(11, 13, 3, t))
      SendVal(connection, 'MPPT_MODE', 'MPPT')
      t += 1
      time.sleep(1)

if __name__ == '__main__':
  main()
