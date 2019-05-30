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
from http_server import StartHTTPServers
import time


# Credientials file should contain one line per user, with username and password
# separated by ':'
CREDENTIALS_FILE='credentials.txt'


def GetHTTPContent(path_elements, authenticated):
  if len(path_elements) >= 1 and path_elements[0] == 'auth':
    # Anything under auth/ is assumed to require authentication.
    if not authenticated:
      raise PermissionError()
    path_elements = path_elements[1:]

  if len(path_elements) == 0:
    if authenticated:
      return 'Logged in', None
    else:
      return 'Not logged in (<a href="/auth">login</a>)', None
  else:
    raise NameError()


def utc_to_local(utc_time):
  return utc_time.replace(tzinfo=timezone.utc).astimezone(tz=None)


def main():
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
    print("{} credentials read".format(len(credentials)))
          
  StartHTTPServers(port=8000, credentials=credentials, num_threads=16, content_fn=GetHTTPContent)
  time.sleep(10000)


if __name__ == '__main__':
  main()
