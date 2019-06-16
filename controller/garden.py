#!/usr/bin/env python3
""" Garden controller.

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
from io import StringIO
import time

import config
from dynamic_var import DynamicVar, AggregationFunction
from remote_handler import RemoteHandler

# Parameter list (sent periodically from MCU):
# SOL_V (solar voltage, millivolts)
# SOL_I (solar current, milliamps)
# SOL_MODE (solar mode, OFF/FAULT/BULK/ABSORPTION/FLOAT)
# MPPT_MODE (mppt algorithm mode, OFF/CVCI/MPPT)
# SOL_ERR (solar error code)
# BATT_V (battery voltage, millivolts)
# BATT_I (battery current, milliamps, discharge is negative)
# LOAD_I (total load current, milliamps)
# PUMP_ON (whether pump is on, 1/0)
# PUMP_I (pump current, milliamps)
# WATER_LEVEL (water level, mm)
# SOIL_MOISTURE (soil moisture, %)

# Commands:
### Watering
### Watering is normally controlled by the MCU firmware, but we include some
### overrides here. They both have timeouts so if we lose communication we won't
### flood the garden or starve the plants. Time is ignored for auto mode (but
### still must be sent for easier parsing on the firmware side).
# SET_MODE <AUTO/ON/OFF> <seconds>

### These functions set auto-watering algorithm parameters.
# SET_WATER_TIME <seconds>
# SET_TIME_BETWEEN_WATERING <seconds>
# SET_MIN_WATER_LEVEL <mm>
# SET_FORCE_STATE <state> <seconds> (state = "0, 1")

WATER_LEVEL_L_PER_MM=0.55

class GardenController():
  def __init__(self):
    self.logger = logging.getLogger('garden')
    self.logger.setLevel(logging.DEBUG)
    self.logger.info("Garden controller started")
    self.remote_handler = RemoteHandler(
      config.GARDEN_PORT,
      lambda addr: self.handle_remote_connected(addr),
      lambda addr, line: self.handle_remote_receive(addr, line),
      lambda addr: self.handle_remote_disconnected(addr))
    self.sol_v = DynamicVar(
        "Solar Voltage", "garden_solar_voltage", format_str='{0:.3f}V')
    self.sol_i = DynamicVar(
        "Solar Current", "garden_solar_current", format_str='{0:.3f}A')
    self.sol_p = DynamicVar(
        "Solar Power", "garden_solar_power", format_str='{0:.1f}W')
    self.sol_mode = DynamicVar("Solar Mode", "garden_solar_mode", enum_values = ['OFF', 'FAULT', 'BULK', 'ABSORPTION', 'FLOAT'], dtype=str)
    self.mppt_mode = DynamicVar("Solar MPPT Mode", "garden_mppt_mode", enum_values = ['OFF', 'CVCI', 'MPPT'], dtype=str)
    self.sol_err = DynamicVar("Solar Error", "garden_solar_error", dtype=int, aggregation_function=AggregationFunction.MAJORITY)
    self.batt_v = DynamicVar("Battery Voltage", "garden_battery_voltage", format_str='{0:.3f}V')
    self.batt_i = DynamicVar("Battery Current", "garden_battery_current", format_str='{0:.3f}A')
    self.soc = DynamicVar("Battery State of Charge", "garden_soc", format_str='{0:.1f}mAh')
    self.load_i = DynamicVar("Load Current", "garden_load_current", format_str='{0:.3f}A')
    self.pump_on = DynamicVar("Pump On", "garden_pump_on", dtype=int)
    self.pump_i = DynamicVar("Pump Current", "garden_pump_current", format_str='{0:.3f}A')
    self.water_level = DynamicVar("Water Level", "garden_water_level", format_str='{0:.1f}L')
    self.soil_moisture = DynamicVar("Soil Moisture", "garden_soil_moisture", format_str='{0:.1f}%')
    self.water_time = DynamicVar("Watering Time", "garden_water_time", format_str='{}s', dtype=int)
    self.time_between_watering = DynamicVar("Time Between Watering", "garden_time_between_watering", format_str='{}s', dtype=int)
    self.force_state = DynamicVar("State Forced", "garden_force_state", dtype=int)
    self.uptime = DynamicVar("MCU Uptime", "garden_mcu_uptime", format_str='{}s', dtype=int)

    self.vars = [self.sol_v, self.sol_i, self.sol_p, self.sol_mode,
                 self.mppt_mode, self.sol_err, self.batt_v, self.batt_i,
                 self.load_i, self.pump_on, self.pump_i, self.water_level,
                 self.soil_moisture, self.uptime, self.soc, self.water_time,
                 self.time_between_watering, self.force_state]

  def handle_remote_connected(self, addr):
    self.logger.info("Accepted connection from {}:{}".format(
                     addr[0], addr[1]))

  def handle_remote_receive(self, addr, line):
    self.logger.debug("Received from remote controller: {}".format(line))
    parts = line.split(' ')
    if len(parts) != 2:
      self.logger.error("Invalid measurement from remote MCU: {}".format(line))
      return
    value_type = parts[0]
    value = parts[1]
    if value_type == 'SOL_V':
      self.sol_v.update(float(value) / 1000)
      if self.sol_i.has_value():
        self.sol_p.update(float(value) / 1000 * self.sol_i.get_value())
    elif value_type == 'SOL_I':
      self.sol_i.update(float(value) / 1000)
      if self.sol_v.has_value():
        self.sol_p.update(float(value) / 1000 * self.sol_v.get_value())
    elif value_type == 'SOL_MODE':
      self.sol_mode.update(value)
    elif value_type == 'MPPT_MODE':
      self.mppt_mode.update(value)
    elif value_type == 'SOL_ERR':
      self.sol_err.update(value)
    elif value_type == 'BATT_V':
      self.batt_v.update(float(value) / 1000)
    elif value_type == 'BATT_I':
      self.batt_i.update(float(value) / 1000)
    elif value_type == 'LOAD_I':
      self.load_i.update(float(value) / 1000)
    elif value_type == 'PUMP_ON':
      self.pump_on.update(value)
    elif value_type == 'PUMP_I':
      self.pump_i.update(float(value) / 1000)
    elif value_type == 'WATER_LEVEL':
      self.water_level.update(float(value) * WATER_LEVEL_L_PER_MM)
    elif value_type == 'SOIL_MOISTURE':
      self.soil_moisture.update(int(value))
    elif value_type == 'UPTIME':
      self.uptime.update(int(value))
    elif value_type == 'FORCE_STATE':
      self.force_state.update(int(value))
    elif value_type == 'WATER_TIME':
      self.water_time.update(int(value))
    elif value_type == 'TIME_BETWEEN_WATERING':
      self.time_between_watering.update(int(value))
    elif value_type == 'SOC':
      self.soc.update(float(value) / 1000)
    else:
      self.logger.error("Received garden update with unknown field: {}".format(value_type))

  def handle_remote_disconnected(self, addr):
    self.logger.info("Remote controller disconnected")

  def handle_http_get(self, path_elements, query_vars, authenticated):
    raise NameError()

  def handle_http_post(self, path_elements, data, authenticated):
    # All POST require authentication
    redirect_response = '''
    <html>
    <head>
    <meta http-equiv="refresh" content="0; URL='/'" />
    </head>
    <body>
    </body>
    </html>
    '''
    if not authenticated:
      raise PermissionError()
    if len(path_elements) == 1 and path_elements[0] == 'send_remote':
      if 'command' not in data:
        raise ValueError('command field missing')
      else:
        command = data['command'][0]
        self.remote_handler.send_line_all(command)
      return redirect_response, None
    else:
      raise NameError()

  def main_section_name(self):
    return 'Garden'

  def main_section_content(self):
    ret = ''
    for var in self.vars:
      ret += var.display_html() + '\n'
    ret += '''<div class="row"><form action="/garden/send_remote" method="post">
    Command: <input type="text" name="command">
    <input type="submit" value="Submit">
    </form></div>'''
    return ret

  def append_updates(self, updates):
    for var in self.vars:
      try:
        var.append_update(updates)
      except:
        print(var.get_display_name())

  def append_all_variables(self, variables):
    for var in self.vars:
      variables.append(var)