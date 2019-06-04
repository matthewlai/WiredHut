#!/usr/bin/env python3
""" Test script to simulate an indoor environment microcontroller.

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
# TEMP <0.01 deg C> <location>
# HUMIDITY <0.1%> <location>

import math
import socket
import time

ADDR_PORT=('localhost', 2939)


def SampleSine(min_val, max_val, period, t):
  dyn_range = max_val - min_val
  return dyn_range / 2 * math.sin(t * 2 * math.pi / period) + min_val


def SendVal(connection, var_name, val, location):
  to_send = '{} {} {}\n'.format(var_name, val, location).encode('utf-8')
  print(to_send)
  connection.sendall(to_send)


def main():
  while True:
    try:
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
        SendVal(connection, 'TEMP', SampleSine(2000, 2500, 5, t), 'indoor')
        SendVal(connection, 'HUMIDITY', SampleSine(500, 1000, 3, t), 'indoor')
        t += 1
        time.sleep(1)
    except ConnectionRefusedError:
      pass
    time.sleep(1)

if __name__ == '__main__':
  main()
