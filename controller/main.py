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
from table import SQLiteTable
from util import utc_to_local


def handle_http_get(path_elements, query_vars, authenticated, delegation_map,
                    tables):
  if len(path_elements) == 0:
    return main_page(delegation_map), None
  elif path_elements[0] == 'aggregated_updates':
    updates = []
    for delegate in delegation_map.values():
      if hasattr(delegate, 'append_updates'):
        delegate.append_updates(updates)
    return '\n'.join(updates), None
  elif path_elements[0] == 'historical_values':
    historical_values = []
    end_time = 4102444800 # 2100
    for table in tables:
      end_time = table.append_historical_values(historical_values,
          config.DATAPOINTS_PER_TABLE, end_time)
    return '\n'.join(historical_values), None
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
    <!-- Force a refresh every hour because we only do value summarising
         on the server side, and our charts will grow too big -->
    <meta http-equiv="refresh" content="3600">
    <link rel="stylesheet" href="/static/style.css">
    <script src="/static/moment.min.js"></script>
    <script src="/static/ajax.js"></script>
    <script src="/static/util.js"></script>
    <script src="/static/Chart.bundle.min.js"></script>

    <!-- datalabels plugin destroys my DOM and breaks everything for some
         reason, so we have to settle with no datalabels for now.
    -->
    <!--<script src="/static/chartjs-plugin-datalabels"</script>-->
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

  variables = []
  for service in http_delegation_map.values():
    if hasattr(service, 'append_all_variables'):
      service.append_all_variables(variables)

  db = sqlite3.connect(config.SQLITE_PATH, check_same_thread = False)
  db.execute("PRAGMA synchronous = OFF")

  tables = []
  for table_config in config.SQLITE_TABLE_CONFIGS:
    name, store_period, keep_period = table_config
    tables.append(SQLiteTable(db, name, store_period, keep_period, variables))
          
  start_http_server(port=config.HTTPS_PORT, credentials=credentials,
                    num_threads=config.HTTP_THREADS,
                    get_handler=lambda path_elements, query_vars,
                    authenticated: handle_http_get(path_elements, query_vars,
                                                   authenticated,
                                                   http_delegation_map,
                                                   tables),
                    post_handler=lambda path_elements, data,
                    authenticated: handle_http_post(
                        path_elements, data, authenticated,
                        http_delegation_map))

  while True:
    try:
      time.sleep(1)
    except KeyboardInterrupt:
      for th in threading.enumerate():
        print(th)
        traceback.print_stack(sys._current_frames()[th.ident])
      break

    # Update tables
    values = []
    for var in variables:
      if (var.has_value()):
        values.append(var.get_value())
      else:
        values.append(None)

    for table in tables:
      table.update(values)

if __name__ == '__main__':
  main()
