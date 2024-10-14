#!/usr/bin/env python3
"""
Copyright (C) 2023 Matthew Lai <m@matthewlai.ca>

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

from dataclasses import dataclass
from datetime import datetime, time, timedelta
import json
import os
import pprint
from typing import Callable, Optional

import paho.mqtt.client as mqtt

"""
Env variables required:
MQTT_HOST
MQTT_USER
MQTT_PASS
"""

DIM_MULTIPLIER = 0.3
TRANSITION_TIME = 1.0
BRIGHTNESS_CHANGE_PER_STEP = 20
BRIGHTNESS_CHANGE_SLOW = 30
BRIGHTNESS_CHANGE_FAST = 40

def KelvinToMired(kelvin):
  return int(1000000.0 / kelvin)

# Given a schedule consisting of list of tuples representing periods and their
# associated values (start_time, value_for_period), return the current value
# based on dt.
def LookupFromSchedule(schedule, dt):
  v = schedule[0][1]
  for part in schedule:
    if dt.time() <= part[0]:
      break
    v = part[1]
  return v

# Returns whether time x is within the interval (start, end). Note that start
# may the later than end, if the interval wraps around.
def TimeIsBetween(x, interval):
  start, end = interval
  if start < end:
    return x >= start and x <= end
  else:
    return x >= start or x <= end

def DefaultColourTempSchedule(dt):
  SCHEDULE = [
    (time(), 2200),
    (time(hour=7), 4000),
    (time(hour=18, minute=30), 2700),
    (time(hour=23, minute=30), 2200)
  ]
  return LookupFromSchedule(schedule=SCHEDULE, dt=dt)

def DefaultBrightnessSchedule(dt):
  SCHEDULE = [
    (time(), 80),
    (time(hour=7), 254),
    (time(hour=18, minute=30), 210),
    (time(hour=22, minute=0), 170),
    (time(hour=23, minute=30), 120)
  ]
  return LookupFromSchedule(schedule=SCHEDULE, dt=dt)

# This is used by hallway and bathroom lights that need to be very dim at night.
def NightLightBrightnessSchedule(dt):
  SCHEDULE = [
    (time(), 80),
    (time(hour=2, minute=30), 25),
    (time(hour=7), 254),
    (time(hour=18, minute=30), 170),
    (time(hour=23, minute=30), 80)
  ]
  return LookupFromSchedule(schedule=SCHEDULE, dt=dt)

@dataclass
class ZoneConfig:
    prefix: str
    light_group: str = "Lights"
    num_motion_sensors: int = 1
    num_dimmer_switches: int = 0  # These are older 4 button dimmer switches.
    num_tap_dial_switches: int = 0
    light_timeout: timedelta = timedelta(seconds=600)
    dim_period_after_timeout: timedelta = timedelta(seconds=30)

    # Normally, if it has been this long since the last manual override action,
    # switch back to auto mode.
    manual_override_timeout: timedelta = timedelta(hours=4)

    # But not if we are in this time period (don't switch back to auto mode
    # when people may be sleeping).
    manual_override_timeout_exclusion_period: tuple[time, time] = (
      time(hour=22, minute=0), time(hour=12, minute=0))

    # Lux limits for turning on and off, with enough hysteresis so our lights
    # don't oscillate.
    # lights_off_lux_threshold: int = 400
    lights_off_lux_threshold: int = 100000
    lights_on_lux_threshold: int = 200

    # Function returns brightness in [0-254], without dimming
    brightness_fn: Callable[[datetime], int] = DefaultBrightnessSchedule

    # Function returns colour temp in [2200-6500]
    colour_temp_fn: Callable[[datetime], int] = DefaultColourTempSchedule

class LightControl:
  def __init__(self, group_name):
    # Brightness = [0-255], we automatically handle 0 as off.
    self._brightness = 0

    # Colour temperature = [2200-6500]
    self._ct = 0

    self._group_name = group_name

  def set_brightness(self, client, brightness = None, colour_temp = None):
    if brightness is None:
      brightness = self._brightness
    if colour_temp is None:
      colour_temp = self._ct
    if self._brightness != brightness or self._ct != colour_temp:
      self._brightness = brightness
      self._ct = colour_temp
      state_on = brightness > 0
      payload_dict = {
          "state": "ON" if state_on else "OFF",
          "brightness": self._brightness,
          "color_temp": KelvinToMired(self._ct),
          "transition": TRANSITION_TIME
      }
      self.send_set_message(client=client, payload=payload_dict)

  def change_brightness(self, client, delta):
    new_brightness = self._brightness + delta
    if new_brightness > 254:
      new_brightness = 254
    elif new_brightness < 0:
      new_brightness = 0
    self.set_brightness(client=client, brightness=new_brightness)

  def send_set_message(self, client, payload):
    topic = f"zigbee2mqtt/{self._group_name}/set"
    client.publish(topic=topic, payload=json.dumps(payload), qos=1)


class Zone:
  def __init__(self, config):
    self._config = config
    self._last_occupancy_time = datetime.now()
    self._off_timeout = config.light_timeout + config.dim_period_after_timeout
    self._light_control = LightControl(
      group_name=f"{config.prefix}/{config.light_group}")

    self._last_manual_override_time = datetime.now()
    self._in_manual_override = False

    # Whether it's bright enough that we don't need lights on.
    self._bright_enough = False

  def config(self):
    return self._config

  def notify_motion_detected(self, client):
    time_now = datetime.now()
    self._last_occupancy_time = time_now
    self.update(client=client, time_now = time_now)

  def notify_manual_override(
      self, client, maybe_brightness = None, maybe_ct = None):
    time_now = datetime.now()
    self._last_manual_override_time = time_now
    self._in_manual_override = True
    self._light_control.set_brightness(client=client,
        brightness=maybe_brightness, colour_temp=maybe_ct)

  def notify_manual_override_change_brightness(self, client, delta):
    time_now = datetime.now()
    self._last_manual_override_time = time_now
    self._in_manual_override = True
    self._light_control.change_brightness(client=client, delta=delta)

  def notify_manual_override_cancelled(self, client):
    self._in_manual_override = False
    self.update(client, datetime.now())

  def update_illuminance(self, illuminance):
    if self._bright_enough:
      if illuminance <= self._config.lights_on_lux_threshold:
        self._bright_enough = False
    else:
      if illuminance >= self._config.lights_off_lux_threshold:
        self._bright_enough = True

  def update(self, client, time_now):
    if self._in_manual_override:
      # See if we should get out of manual override.
      time_since_override = time_now - self._last_manual_override_time
      if time_since_override > self._config.manual_override_timeout:
        exclusion_period = self._config.manual_override_timeout_exclusion_period
        if not TimeIsBetween(time_now.time(), exclusion_period):
          self._in_manual_override = False

    # If we are still in manual override, don't do anything.
    if self._in_manual_override:
      return
    time_since_motion = time_now - self._last_occupancy_time
    brightness = self._config.brightness_fn(time_now)
    ct = self._config.colour_temp_fn(time_now)
    if time_since_motion <= self._config.light_timeout:
      self._light_control.set_brightness(
          client=client, brightness=brightness, colour_temp=ct)
    elif (time_since_motion > self._config.light_timeout and
        time_since_motion <= self._off_timeout):
      self._light_control.set_brightness(
          client=client,
          brightness=int(brightness * DIM_MULTIPLIER),
          colour_temp=ct)
    else:
      self._light_control.set_brightness(client=client, brightness=0,
          colour_temp=ct)


ZONES = {
  'Hallway': Zone(config=ZoneConfig(
    prefix="Hallway",
    light_timeout=timedelta(seconds=60)
  )),
  'UpstairsLanding': Zone(config=ZoneConfig(
    prefix="UpstairsLanding",
    brightness_fn=NightLightBrightnessSchedule,
    light_timeout=timedelta(seconds=60)
  )),
  'Kitchen': Zone(ZoneConfig(
    prefix="Kitchen",
    num_tap_dial_switches=1,
  )),
  'DiningRoom': Zone(ZoneConfig(
    prefix="DiningRoom",
    num_tap_dial_switches=1,
  )),
  'LivingRoom': Zone(ZoneConfig(
    prefix="LivingRoom",
    num_tap_dial_switches=1,
  )),
  'FrontBedroom': Zone(ZoneConfig(
    prefix="FrontBedroom",
    num_dimmer_switches=1,
  )),
  'RearBedroom': Zone(ZoneConfig(
    prefix="RearBedroom",
    num_tap_dial_switches=1,
  )),
  'Study': Zone(ZoneConfig(
    prefix="Study",
    num_tap_dial_switches=1,
  )),
  'UtilityRoom': Zone(ZoneConfig(
    prefix="UtilityRoom",
  )),
  'DownstairsToilet': Zone(ZoneConfig(
    prefix="DownstairsToilet",
    brightness_fn=lambda _ : 255, # Downstairs toilet doesn't have much light.
  )),
  'UpstairsBathroom': Zone(ZoneConfig(
    prefix="UpstairsBathroom",
    brightness_fn=NightLightBrightnessSchedule,
    num_tap_dial_switches=1,
  )),
}

def on_connect(client, userdata, flags, rc):
  print(f"Connected with result code {str(rc)}")
  # Subscribe to all the motion sensors.
  for zone_name, zone in ZONES.items():
    config = zone.config()
    for i in range(config.num_motion_sensors):
      topic = f"zigbee2mqtt/{config.prefix}/Motion{i}"
      client.subscribe(topic)
      # TODO: Set occupancy timeout to be proportional to light_timeout to save
      # power.
      timeout_payload = {
        'occupancy_timeout': int(10)
      }
      client.publish(topic=f"{topic}/set",
                     payload=json.dumps(timeout_payload))
    for i in range(config.num_dimmer_switches):
      topic = f"zigbee2mqtt/{config.prefix}/DimmerSwitch{i}"
      client.subscribe(topic)
    for i in range(config.num_tap_dial_switches):
      topic = f"zigbee2mqtt/{config.prefix}/TapDialSwitch{i}"
      client.subscribe(topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
  payload = json.loads(msg.payload)
  for zone_name, zone in ZONES.items():
    config = zone.config()
    for i in range(config.num_motion_sensors):
      topic = f"zigbee2mqtt/{config.prefix}/Motion{i}"
      if msg.topic == topic:
        # We only care about positive occupancy since we do our own timeout.
        if "illuminance_lux" in payload:
          zone.update_illuminance(int(payload["illuminance_lux"]))
        if "occupancy" in payload and payload["occupancy"]:
          zone.notify_motion_detected(client)
          break
    for i in range(config.num_dimmer_switches):
      topic = (
          f"zigbee2mqtt/{config.prefix}/DimmerSwitch{i}")
      if msg.topic == topic:
        if "action" in payload:
          if payload["action"] == "on_press_release":
            zone.notify_manual_override_cancelled(client)
          elif payload["action"] == "off_press_release":
            zone.notify_manual_override(client, maybe_brightness=0)
        break
    for i in range(config.num_tap_dial_switches):
      topic = (
          f"zigbee2mqtt/{config.prefix}/TapDialSwitch{i}")
      if msg.topic == topic:
        if "action" in payload:
          # Button 1 switches to auto, 2 to 2700, 3 to 4000, 4 to off.
          if payload["action"] == "button_1_press_release":
            zone.notify_manual_override_cancelled(client)
          elif payload["action"] == "button_2_press_release":
            zone.notify_manual_override(client, maybe_brightness=128,
              maybe_ct=2700)
          elif payload["action"] == "button_3_press_release":
            zone.notify_manual_override(client, maybe_brightness=200,
              maybe_ct=4000)
          elif payload["action"] == "button_4_press_release":
            zone.notify_manual_override(client, maybe_brightness=0)
          elif payload["action"] == "dial_rotate_right_step":
            zone.notify_manual_override_change_brightness(
              client, delta=BRIGHTNESS_CHANGE_PER_STEP)
          elif payload["action"] == "dial_rotate_right_slow":
            zone.notify_manual_override_change_brightness(
              client, delta=BRIGHTNESS_CHANGE_SLOW)
          elif payload["action"] == "dial_rotate_right_fast":
            zone.notify_manual_override_change_brightness(
              client, delta=BRIGHTNESS_CHANGE_FAST)
          elif payload["action"] == "dial_rotate_left_step":
            zone.notify_manual_override_change_brightness(
              client, delta=-BRIGHTNESS_CHANGE_PER_STEP)
          elif payload["action"] == "dial_rotate_left_slow":
            zone.notify_manual_override_change_brightness(
              client, delta=-BRIGHTNESS_CHANGE_SLOW)
          elif payload["action"] == "dial_rotate_left_fast":
            zone.notify_manual_override_change_brightness(
              client, delta=-BRIGHTNESS_CHANGE_FAST)
        break

def main():
  client = mqtt.Client(client_id="light_controller", clean_session=True)
  client.on_connect = on_connect
  client.on_message = on_message
  client.username_pw_set(os.environ["MQTT_USER"], os.environ["MQTT_PASS"])
  client.connect(os.environ["MQTT_HOST"], 1883, 60)

  while True:
    # We run this main loop at 1 Hz to save power. We only need sub-second
    # response time to turning on lights triggered by motion sensors, which
    # is handled through callback.
    client.loop(timeout=1.0)

    now = datetime.now()

    # Everything else in this loop must be fast, since motion events are not
    # processed in this time.
    for zone_name, zone in ZONES.items():
      zone.update(client=client, time_now=now)

if __name__ == "__main__":
    main()