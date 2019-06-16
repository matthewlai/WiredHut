#!/usr/bin/env python3
""" SQLite table manager.

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

from dynamic_var import AggregationFunction
import sqlite3
import time
import threading

class Aggregator():
  def __init__(self, aggregation_function):
    self.aggregation_function = aggregation_function
    self.reset()

  def add_value(self, value, duration):
    if value is not None:
      self.num_samples += 1
      if self.aggregation_function == AggregationFunction.AVERAGE:
        self.weighted_value_sum += value * duration
        self.total_duration += duration
      elif self.aggregation_function == AggregationFunction.MAJORITY:
        if value not in self.total_duration_by_value:
          self.total_duration_by_value[value] = 0.0
        self.total_duration_by_value[value] += duration

  def get_aggregated_value(self, reset=False):
    if self.num_samples > 0:
      if self.aggregation_function == AggregationFunction.AVERAGE:
        return_val = self.weighted_value_sum / self.total_duration
      elif self.aggregation_function == AggregationFunction.MAJORITY:
        return_val = max(self.total_duration_by_value,
                   key=lambda key: self.total_duration_by_value[key])
    else:
      return_val = None

    if reset:
      self.reset()

    return return_val

  def reset(self):
    self.num_samples = 0
    if self.aggregation_function == AggregationFunction.AVERAGE:
      self.weighted_value_sum = 0.0
      self.total_duration = 0.0
    elif self.aggregation_function == AggregationFunction.MAJORITY:
      self.total_duration_by_value = {}
    else:
      raise ValueError("Unknown aggregation function: {}".format(
          self.aggregation_function))

class SQLiteTable():
  def __init__(self, conn, name, store_period_s, keep_period_s, variables):
    self.lock = threading.Lock()
    self.conn = conn
    self.name = name
    self.store_period_s = store_period_s
    self.keep_period_s = keep_period_s
    self.variables = variables
    self.last_store_time_s = 0
    self.last_update_time_s = 0

    columns = ['timestamp_start_ms INTEGER', 'timestamp_end_ms INTEGER']
    columns.extend(['{} {}'.format(
      var.get_internal_name(), var.get_sql_type()) for var in variables])
    column_desc = ','.join(columns)

    # Try to create a new table. This will fail if the table already exists, and
    # that is fine.
    create_statement = '''CREATE TABLE {table_name} ({column_desc})'''.format(
        table_name=self.name, column_desc=column_desc)
    try:
      self.conn.execute(create_statement)
    except sqlite3.OperationalError:
      pass

    # Insert any new variable since last time.
    existing_columns = set(
        map(lambda x: x[0], self.conn.execute('SELECT * from {}'.format(
        self.name)).description))

    for column in columns:
      column_name = column.split(' ')[0]
      if column_name not in existing_columns:
        self.conn.execute('''ALTER TABLE {} ADD {};'''.format(
            self.name, column))
    self.aggregators = [
        Aggregator(var.get_aggregation_function()) for var in variables]
    self.variable_names = [var.get_internal_name() for var in variables]

  def update(self, data, current_time = None, commit=True):
    with self.lock:
      if current_time is None:
        current_time = time.time()
      time_since_last_update = current_time - self.last_update_time_s

      for i in range(len(data)):
        if data[i] is not None:
          self.aggregators[i].add_value(data[i], time_since_last_update)

      last_update_time_s = current_time

      if current_time >= (self.last_store_time_s + self.store_period_s - 0.1):
        # Time to make a new row.
        columns = ['timestamp_start_ms', 'timestamp_end_ms']
        columns += self.variable_names
        statement = '''INSERT INTO {} ({}) VALUES ({});'''.format(self.name, 
            ','.join(columns), ','.join('?' for _ in columns))
        values = [int(self.last_store_time_s * 1000), int(current_time * 1000)]
        values += [ag.get_aggregated_value(reset=True) for ag in self.aggregators]
        self.conn.execute(statement, values)
        self.last_store_time_s = current_time

        # Remove outdated rows
        if self.keep_period_s:
          outdated_threshold_ms = int((current_time - self.keep_period_s) * 1000)
          statement = 'DELETE FROM {} WHERE timestamp_end_ms < {}'.format(
              self.name, outdated_threshold_ms)
          self.conn.execute(statement)

      if commit:
        self.conn.commit()

  def clear_table(self):
    with self.lock:
      self.conn.execute('DELETE FROM {}'.format(self.name))
      self.conn.commit()

  def commit(self):
    with self.lock:
      self.conn.commit()

  # Appends 'max_num_datapoints' latest data points from the table up to 'end_time'.
  # Returns time of the first sample.
  def append_historical_values(self, historical_values, max_num_datapoints,
                               end_time = None):
    with self.lock:
      columns = ['timestamp_start_ms', 'timestamp_end_ms']
      columns += self.variable_names
      statement = ('SELECT {} from {} WHERE timestamp_end_ms < {}'
                   ' ORDER BY timestamp_end_ms DESC;').format(
          ','.join(columns), self.name, int(end_time * 1000))

      num_datapoints = 0
      first_end_time = 0
      timestamps = []
      values_by_variable_index = [[] for _ in self.variables]
      for row in self.conn.execute(statement):
        timestamps.append(float(row[1]) / 1000)
        for column_index in range(2, len(columns)):
          variable_index = column_index - 2
          values_by_variable_index[variable_index].append(row[column_index])

        num_datapoints += 1
        if (max_num_datapoints is not None and
            num_datapoints >= max_num_datapoints):
          break

      for variable, values in zip(self.variables, values_by_variable_index):
        variable.append_update_with_values_timestamps(historical_values, values,
            timestamps)

      if len(timestamps) > 0:
        return timestamps[len(timestamps) - 1]
      else:
        return end_time
