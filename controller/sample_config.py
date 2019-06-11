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

HTTPS_PORT=8000

GARDEN_PORT=2938
INDOOR_PORT=2939