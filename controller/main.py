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
import sys
import sqlite3
import time
import threading
import traceback

import config
from dynamic_var import DynamicVar
from garden import GardenController
from http_server import start_http_server
from indoor import IndoorController
from static_server import StaticServer
from util import utc_to_local


def handle_http_get(path_elements, query_vars, authenticated, delegation_map):
  if len(path_elements) == 0:
    return main_page(delegation_map), None
  elif path_elements[0] == 'aggregated_updates':
    updates = []
    for delegate in delegation_map.values():
      if hasattr(delegate, 'append_updates'):
        delegate.append_updates(updates)
    return '\n'.join(updates), None
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
  console.setLevel(logging.INFO)

  logging.basicConfig(
    format=('[%(asctime)s.%(msecs)03d][%(levelname)s] %(name)s' + 
            ' %(message)s (%(filename)s:%(lineno)d)'),
    datefmt='%m/%d/%Y %H:%M:%S')

  credentials = config.CREDENTIALS

  garden_controller = GardenController()
  indoor_controller = IndoorController()

  static_server = StaticServer()

  http_delegation_map = {
    'garden': garden_controller,
    'indoor': indoor_controller,
    'static': static_server
  }
          
  start_http_server(port=config.HTTPS_PORT, credentials=credentials, num_threads=16,
                    get_handler=lambda path_elements, query_vars,
                    authenticated: handle_http_get(path_elements, query_vars,
                                                   authenticated,
                                                   http_delegation_map),
                    post_handler=lambda path_elements, data,
                    authenticated: handle_http_post(
                        path_elements, data, authenticated,
                        http_delegation_map))

  timestamp_start = DynamicVar("Timestamp Start (ms)", "timestamp_start_ms")
  timestamp_end = DynamicVar("Timestamp End (ms)", "timestamp_end_ms")

  db = sqlite3.connect(config.SQLITE_PATH)
  db.execute("PRAGMA synchronous = OFF")

  variables = []
  for service in http_delegation_map.values():
    if hasattr(service, 'append_all_variables'):
      service.append_all_variables(variables)

  create_string = ','.join(['{} {}'.format(
      var.get_internal_name(), var.get_sql_type()) for var in variables])

  # Try to create a new table. This will fail if the table already exists, and
  # that is fine.
  create_statement = '''CREATE TABLE data1s ({})'''.format(create_string)
  try:
    db.execute(create_statement)
  except sqlite3.OperationalError:
    pass

  # Insert any new variable since last time.
  existing_columns = set(
      map(lambda x: x[0], db.execute('SELECT * from data1s').description))

  for var in variables:
    if var.get_internal_name() not in existing_columns:
      print('''ALTER TABLE data1s ADD {} {};'''.format(
          var.get_internal_name(), var.get_sql_type()))
      db.execute('''ALTER TABLE data1s ADD {} {};'''.format(
          var.get_internal_name(), var.get_sql_type()))

  variable_names = [var.get_internal_name() for var in variables]

  while True:
    timestamp_start.update(int(time.time() * 1000))
    try:
      time.sleep(1)
    except KeyboardInterrupt:
      for th in threading.enumerate():
        print(th)
        traceback.print_stack(sys._current_frames()[th.ident])
      break
    timestamp_end.update(int(time.time() * 1000))
    values = []

    for var in variables:
      if (var.has_value()):
        values.append(var.get_value())
      else:
        values.append(None)
    
    statement = '''INSERT INTO data1s ({}) VALUES ({});'''.format(
        ','.join(variable_names), ','.join('?' for _ in values))
    db.execute(statement, values)
    db.commit()

if __name__ == '__main__':
  main()
