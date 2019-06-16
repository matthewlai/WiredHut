#!/usr/bin/env python3
""" Config file.

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

CREDENTIALS = [
  ('user', 'pass'),
  ('user2', 'pass2')
]

TLS_CERTCHAIN_PATH='/path/to/fullchain.pem'
TLS_PRIVATE_KEY_PATH='/path/to/privkey.pem'

SQLITE_PATH='record.db'

SQLITE_TABLE_CONFIGS = [
  # (table name, update period in seconds, keep period in seconds)
  ('data1s', 1, 30 * 60), # Every second for 30 minutes
  ('data10s', 10, 24 * 60 * 60), # Every 10 seconds for 1 day
  ('data1m', 60, 10 * 24 * 60 * 60), # Every minute for 10 days
  ('data5m', 10 * 60, None), # Every 5 minutes forever
  ('data1h', 60 * 60, None), # Every hour forever
  ('data4h', 4 * 60 * 60, None), # Every 4 hours forever
]

# How many latest datapoints to use from each table in the charts
DATAPOINTS_PER_TABLE=200

HTTPS_PORT=8000
HTTP_THREADS=4

GARDEN_PORT=2938
INDOOR_PORT=2939
