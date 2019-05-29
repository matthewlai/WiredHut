#!/usr/bin/env python3
""" Entry point for WiredHut controller.

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

import calendar
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer
import time
import threading
import socket
import socketserver
import sys


class Handler(BaseHTTPRequestHandler):
  def do_GET(self):
    if self.path != '/':
      self.send_error(404, "Object not found")
      return
    self.send_response(200)
    self.send_header('Content-type', 'text/html; charset=utf-8')
    self.end_headers()

    i = 0
    while True:
      self.wfile.write("{} ".format(i).encode('utf-8'))
      time.sleep(1)
      i = i + 1


def utc_to_local(utc_time):
  return utc_time.replace(tzinfo=timezone.utc).astimezone(tz=None)


class HTTPThread(threading.Thread):
  def __init__(self, addr, sock, i):
    threading.Thread.__init__(self)
    self.addr = addr
    self.sock = sock
    self.i = i
    self.daemon = True
    self.start()

  def run(self):
    httpd = HTTPServer(self.addr, Handler, False)

    httpd.socket = self.sock
    httpd.server_bind = self.server_close = lambda self: None
    httpd.serve_forever()


def main():
  addr = ('', 8000)
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  sock.bind(addr)
  sock.listen(16)

  [HTTPThread(addr, sock, i) for i in range(10)]
  time.sleep(10000)


if __name__ == '__main__':
  main()
