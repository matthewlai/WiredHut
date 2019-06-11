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

import base64
from datetime import datetime
import gzip
from http.server import BaseHTTPRequestHandler, HTTPServer
import socket
import socketserver
import ssl
import time
import threading
from urllib import parse

import config

# The user is banned if they attempted FAILED_ATTEMPTS_COUNTS failed
# authorizations in the past FAILED_ATTEMPTS_WINDOW seconds.
FAILED_ATTEMPTS_COUNT = 3
FAILED_ATTEMPTS_WINDOW = 300

# Files with these extensions are assumed to be cacheable
CACHEABLE_EXTS = ['jpg', 'png']


def encode_credential(credential):
  return base64.b64encode((credential[0] + ':' +
      credential[1]).encode('utf-8')).decode('utf-8')


class HTTPHandler(BaseHTTPRequestHandler):
  auth_dict_lock = threading.Lock()
  failed_auth_times = {}
  credentials = []

  # We only have one *_handler, but we need to store it in a container to
  # avoid binding.
  get_handler = []
  post_handler = []

  def handle_request(self):
    client_ip = self.client_address[0]
    if self.is_banned(client_ip):
      self.send_error(403, 'Forbidden',
                      'Too many failed authorization attempts')
      return

    try:
      path_elements = []
      path_without_query = self.path
      if path_without_query.find('?') >= 0:
        path_without_query = path_without_query.split('?')[0]
      for component in path_without_query.split('/'):
        if len(component) > 0:
          path_elements.append(component)

      authenticated = False
      if 'Authorization' in self.headers:
        for credential in self.credentials:
          if self.headers['Authorization'] == ("Basic " +
              encode_credential(credential)):
            authenticated = True
            break
        if authenticated:
          self.handle_successful_auth(client_ip)
        else:
          self.handle_failed_auth(client_ip)

      if self.command == 'GET' or self.command == 'HEAD':
        query_vars = {}
        if self.path.find('?') >= 0:
          query_vars = parse.parse_qs(self.path.split('?')[1])
        content, content_type = self.get_handler[0](path_elements, query_vars,
                                                    authenticated)
      elif self.command == 'POST':
        if ('Content-Type' not in self.headers or
            self.headers['Content-Type'] !=
                'application/x-www-form-urlencoded'):
          self.send_error(415, 'Unsupported Media Type',
                          'Unsupported Content-Type')
          return
        read_len = int(self.headers['Content-Length'])
        data = parse.parse_qs(self.rfile.read(read_len).decode('utf-8'))
        content, content_type = self.post_handler[0](path_elements, data,
                                                     authenticated)
      if not content_type:
        content_type = 'text/html; charset=utf-8'
      try:
        content = content.encode('utf-8')
      except AttributeError:
        # We probably already have a byte array.
        pass
    except NameError as e:
      self.send_error(404, 'Object not found', str(e))
      return
    except ValueError as e:
      self.send_error(400, 'Bad request', str(e))
      return
    except PermissionError:
      self.send_response(401)
      self.send_header('WWW-Authenticate',
                       'Basic realm="Controller", charset="UTF-8"')
      self.end_headers()
      self.wfile.write(
          'Authorization required for function requested'.encode('utf-8'))
      return

    protocol_version = 'HTTP/1.1'

    self.send_response(200)

    if (len(content) > 100 and 'Accept-Encoding' in self.headers and
        'gzip' in self.headers['Accept-Encoding']):
      plain_size = len(content)
      content = gzip.compress(content)
      self.log_message('Transferring %d bytes (%d bytes uncompressed)',
                       len(content), plain_size)
      self.send_header('Content-Encoding', 'gzip')
    else:
      self.log_message('Transferring %d bytes', len(content))

    self.send_header('Content-Type', content_type)
    self.send_header('Content-Length', str(len(content)))

    if len(path_elements) > 0 and path_elements[-1].find('.') >= 0:
      if path_elements[-1].split('.')[-1] in CACHEABLE_EXTS:
        self.send_header('Cache-Control', 'public,max-age=86400')

    self.end_headers()

    if self.command != 'HEAD':
      try:
        self.wfile.write(content)
      except BrokenPipeError:
        self.log_message('Connection closed')

  def do_GET(self):
    self.handle_request()

  def do_HEAD(self):
    self.handle_request()

  def do_POST(self):
    self.handle_request()

  def handle_successful_auth(self, ip):
    with self.auth_dict_lock:
      if ip in self.failed_auth_times:
        self.failed_auth_times[ip] = []

  def handle_failed_auth(self, ip):
    with self.auth_dict_lock:
      if not ip in self.failed_auth_times:
        self.failed_auth_times[ip] = []
      self.failed_auth_times[ip].append(datetime.now().timestamp())
      self.failed_auth_times[ip] = \
          self.failed_auth_times[ip][-FAILED_ATTEMPTS_COUNT:]

  def is_banned(self, ip):
    with self.auth_dict_lock:
      if (not ip in self.failed_auth_times or
          len(self.failed_auth_times[ip]) < FAILED_ATTEMPTS_COUNT):
        return False

      cutoff_time = datetime.now().timestamp() - FAILED_ATTEMPTS_WINDOW
      # If any of the failed attempts were FAILED_ATTEMPTS_WINDOW before now,
      # they are forgiven.
      for failed_timestamp in self.failed_auth_times[ip]:
        if failed_timestamp < cutoff_time:
          return False

      return True


class HTTPThread(threading.Thread):
  def __init__(self, addr, sock):
    threading.Thread.__init__(self)
    self.addr = addr
    self.sock = sock
    self.daemon = True
    self.start()

  def run(self):
    httpd = HTTPServer(self.addr, HTTPHandler, False)
    httpd.socket = self.sock
    httpd.server_bind = self.server_close = lambda self: None
    httpd.serve_forever()


def start_http_server(port, credentials, num_threads, get_handler,
                      post_handler):
  addr = ('', port)
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  sock.bind(addr)
  sock.listen(num_threads)

  tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
  tls_context.load_cert_chain(config.TLS_CERTCHAIN_PATH,
                              config.TLS_PRIVATE_KEY_PATH)

  sock = tls_context.wrap_socket(sock, server_side=True)

  HTTPHandler.credentials = credentials
  HTTPHandler.get_handler.append(get_handler)
  HTTPHandler.post_handler.append(post_handler)
  http_threads = [HTTPThread(addr, sock) for _ in range(num_threads)]
  

