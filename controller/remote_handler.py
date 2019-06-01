#!/usr/bin/env python3
"""Class for handling remote clients

Class for running a service and communicating with a remote client using
line-based communication

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

import select
import time
import threading

import logging
from io import StringIO
import socket

class RemoteHandler(threading.Thread):
  def __init__(self, port, connected_handler, recv_handler, close_handler):
    threading.Thread.__init__(self)
    self.daemon = True
    self.addr_port = ('', port)
    self.connected_handler = connected_handler
    self.recv_handler = recv_handler
    self.close_handler = close_handler
    self.to_send = []
    self.to_send_lock = threading.Lock()
    self.read_buffer = StringIO()
    self.start()

  def run(self):
    self.server_connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.server_connection.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.server_connection.bind(self.addr_port)
    self.server_connection.listen(10)

    while True:
      connection, remote_addr = self.server_connection.accept()
      if self.connected_handler:
        self.connected_handler(remote_addr)
      self.handle_client(connection, remote_addr)

  # Handle a MCU connection
  def handle_client(self, connection, remote_addr):
    connection.settimeout(0.05)
    while True:
      try:
        recv_data = None
        recv_data = connection.recv(4096)
        if not recv_data:
          if self.close_handler:
            self.close_handler()
            self.to_send = []
          break
      except socket.timeout:
        pass
      except:
        if self.close_handler:
          self.close_handler()
          self.to_send = []
        break
      
      if recv_data:
        for c in recv_data.decode('utf-8'):
          if c == '\n':
            self.recv_handler(self.read_buffer.getvalue())
            self.read_buffer = StringIO()
          else:
            self.read_buffer.write(c)

      if self.to_send:
        with self.to_send_lock:
          for line in self.to_send:
            connection.sendall(line + '\n')
          self.to_send = []

  def send_line(line):
    with self.to_send_lock:
      to_send.append(line)