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
import logging
import time

from garden import GardenController
from http_server import start_http_server
from indoor import IndoorController
from static_server import StaticServer
from util import utc_to_local


# Credientials file should contain one line per user, with username and password
# separated by ':'
CREDENTIALS_FILE='credentials.txt'


def handle_http_get(path_elements, query_vars, authenticated, delegation_map):
  if len(path_elements) == 0:
    return main_page(delegation_map), None
  elif path_elements[0] in delegation_map:
    return delegation_map[path_elements[0]].handle_http_get(path_elements[1:],
                                                            query_vars,
                                                            authenticated)
  else:
    raise NameError()


def handle_http_post(path_elements, data, authenticated, delegation_map):
  if len(path_elements) > 0 and path_elements[0] in delegation_map:
    return delegation_map[path_elements[0]].handle_http_post(path_elements[1:],
                                                             data,
                                                             authenticated)
  else:
    raise NameError()


def read_credentials():
  credentials = []

  try:
    with open('credentials.txt', 'r') as cred_file:
      for line in cred_file:
        if line.find(':') != -1:
          user = line.split(':')[0]
          password = line.split(':')[1]
          credentials.append((user.strip('\n'), password.strip('\n')))
  except:
    print("Failed to read " + CREDENTIALS_FILE)
    return

  if len(credentials) == 0:
    print("Failed to read any credential from " + CREDENTIALS_FILE)
    return
  else:
    print("{} credential(s) read".format(len(credentials)))

  return credentials


def main_page(delegation_map):
  template = """<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <link rel="stylesheet" href="/static/style.css">
    <script src="/static/ajax.js"></script>
    <title>WiredHut</title>
  </head>
  <body>
    {content}
  </body>
</html>
"""
  content = ''
  for delegate in delegation_map.values():
    if not hasattr(delegate, 'main_section_name'):
      continue
    content += '<div class="section-container"><h2>{name}</h2><div class="content-section">{section_content}</div></div>'.format(
        name=delegate.main_section_name(),
        section_content=delegate.main_section_content())

  return template.format(content=content)


def main():
  console = logging.StreamHandler()
  console.setLevel(logging.DEBUG)

  logging.basicConfig(
    format=('[%(asctime)s.%(msecs)03d][%(levelname)s] %(name)s' + 
            ' %(message)s (%(filename)s:%(lineno)d)'),
    datefmt='%m/%d/%Y %H:%M:%S')

  credentials = read_credentials()

  garden_controller = GardenController()
  indoor_controller = IndoorController()

  static_server = StaticServer()

  http_delegation_map = {
    'garden': garden_controller,
    'indoor': indoor_controller,
    'static': static_server
  }
          
  start_http_server(port=8000, credentials=credentials, num_threads=16,
                    get_handler=lambda path_elements, query_vars,
                    authenticated: handle_http_get(path_elements, query_vars,
                                                   authenticated,
                                                   http_delegation_map),
                    post_handler=lambda path_elements, data,
                    authenticated: handle_http_post(
                        path_elements, data, authenticated,
                        http_delegation_map))
  time.sleep(10000)


if __name__ == '__main__':
  main()
