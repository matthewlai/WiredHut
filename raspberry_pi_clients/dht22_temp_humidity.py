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

import Adafruit_DHT

ADDR_PORT=('wiredhut_hub', 2939)


def SendVal(connection, var_name, val, location):
  to_send = '{} {} {}\n'.format(var_name, val, location).encode('utf-8')
  connection.sendall(to_send)


def main():
  sensor = Adafruit_DHT.DHT22
  pin = 4

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
        humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
        SendVal(connection, 'TEMP', int(temperature * 100), 'indoor')
        SendVal(connection, 'HUMIDITY', int(humidity * 10), 'indoor')
        t += 1
        time.sleep(2)
    except ConnectionRefusedError:
      pass
    time.sleep(10)

if __name__ == '__main__':
  main()
