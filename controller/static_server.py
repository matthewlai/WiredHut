#!/usr/bin/env python3
""" Static file server.

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
import os
import time

# Supported extensions and MIME types.
MIME_TABLE = {
  'css': 'text/css',
  'csv': 'text/csv',
  'htm': 'text/html',
  'html': 'text/html',
  'ico': 'image/vnd.microsoft.icon',
  'jpg': 'image/jpeg',
  'jpeg': 'image/jpeg',
  'js': 'text/javascript',
  'json': 'text/json',
  'png': 'image/png',
  'pdf': 'application/pdf',
  'txt': 'text/plain'
}


def mime_type(name):
  name_split = name.split('.')
  if len(name_split) < 2:
    return 'text/plain'
  else:
    ext  = name_split[-1]
    if ext in MIME_TABLE:
      return MIME_TABLE[ext]
    else:
      return 'text/plain'


class StaticServer():
  def __init__(self):
    self.logger = logging.getLogger('static')
    self.logger.setLevel(logging.DEBUG)

    # Recursively build a list of files. This becomes our whitelist to make
    # path sanitization easier.
    self.file_list = []
    for root, dirs, files in os.walk('static'):
      if not files:
        continue
      path_prefix = root.split('/')[1:]
      for file in files:
        full_path = path_prefix.copy()
        full_path.append(file)
        self.file_list.append('/'.join(full_path))

  def handle_http_get(self, path_elements, query_vars, authenticated):
    joined_path = '/'.join(path_elements)
    if not joined_path in self.file_list:
      raise NameError()

    with open('static/' + joined_path, 'rb') as f:
      return f.read(), mime_type(path_elements[-1])

  def handle_http_post(self, path_elements, data, authenticated):
    # No POST requests supported.
    raise NameError()
