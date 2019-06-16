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

import logging
import sys
import sqlite3
import time

import config
from dynamic_var import DynamicVar
from garden import GardenController
from indoor import IndoorController
from table import SQLiteTable

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

  http_delegation_map = {
    'garden': garden_controller,
    'indoor': indoor_controller
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

  variable_names = [var.get_internal_name() for var in variables]
  columns = variable_names
  statement = 'SELECT {} from {};'.format(','.join(columns), 'data1s')
  all_rows = db.execute(statement).fetchall()
  total_num_records = len(all_rows)
  print("{} records found".format(total_num_records))

  for table in tables:
    table.clear_table()

  simulated_now = time.time() - total_num_records

  num_datapoints = 0
  for row in all_rows:
    for table in tables:
      table.update(row, simulated_now, commit=False)
    simulated_now += 1
    num_datapoints += 1
    if num_datapoints % 1000 == 0:
      table.commit()
      print(num_datapoints)

if __name__ == '__main__':
  main()
