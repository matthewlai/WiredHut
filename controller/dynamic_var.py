#!/usr/bin/env python3
""" Convenience wrapper for a dynamic variable.

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

from collections import namedtuple
from enum import Enum
import threading
import time

TimeRange = namedtuple('TimeRange', ['str', 'range_seconds', 'is_default'], defaults=[False])

COLOURS = [
  'rgb(255, 99, 132)',
  'rgb(255, 159, 64)',
  'rgb(255, 205, 86)',
  'rgb(75, 192, 192)',
  'rgb(54, 162, 235)',
  'rgb(153, 102, 255)',
  'rgb(201, 203, 207)'
]

class AggregationFunction(Enum):
  AVERAGE = 0
  MAJORITY = 1
  AUTO = 2 # Majority for strings, average for other types

class DynamicVar():
  current_colour_index = 0

  # Internal name must be unique
  def __init__(self, display_name, internal_name, enum_values = [], dtype=float,
               format_str = '{}',
               aggregation_function = AggregationFunction.AUTO):
    self.lock = threading.Lock()
    self.display_name = display_name
    self.internal_name = internal_name
    self.format_str = format_str
    if dtype is float:
      self.sql_type = 'REAL'
    elif dtype is str:
      self.sql_type = 'TEXT'
      if not enum_values:
        raise ValueError("string vars must have enum values")
      self.enum_values = enum_values
    elif dtype is int:
      self.sql_type = 'INTEGER'
    else:
      print('Unknown dtype: {}'.format(dtype))
      self.sql_type = 'TEXT'

    if aggregation_function == AggregationFunction.AUTO:
      if dtype is str:
        self.aggregation_function = AggregationFunction.MAJORITY
      else:
        self.aggregation_function = AggregationFunction.AVERAGE
    else:
      self.aggregation_function = aggregation_function
    self.colour = COLOURS[self.current_colour_index]
    DynamicVar.current_colour_index = (self.current_colour_index + 1) % len(COLOURS)

  def update(self, new_value):
    with self.lock:
      self.value = new_value
      self.last_update_time_ms = time.time() * 1000

  def get_value(self):
    with self.lock:
      if hasattr(self, 'value'):
        return self.value
      else:
        return 'No value'

  def get_sql_type(self):
    return self.sql_type

  def has_value(self):
    with self.lock:
      return hasattr(self, 'value')

  def get_value_str(self):
    with self.lock:
      if hasattr(self, 'value'):
        return self.format_str.format(self.value)
      else:
        return 'No value'

  def get_internal_name(self):
    return self.internal_name

  def get_last_update_time_ms(self):
    with self.lock:
      return self.last_update_time_ms

  def append_update_with_values_timestamps(self, updates, values, timestamps):
    value_pairs = []
    for value, timestamp in zip(values, timestamps):
      if value is not None:
        value = str(value)
        if self.sql_type == 'TEXT':
          try:
            value =  self.enum_values.index(value)
          except ValueError:
            value = -1
        value_pairs.append('{{ x: moment.unix({timestamp}).toDate(), y: {value} }}'.format(
          timestamp=timestamp, value=value))
    updates.append('add_data("{internal_name}", [{value_pairs}]);'.format(
       internal_name=self.internal_name, value_pairs=','.join(value_pairs)))

  def append_update(self, updates):
    if self.has_value():
      self.append_update_with_values_timestamps(updates, [self.get_value()],
          [int(self.get_last_update_time_ms() / 1000)])

  def display_html(self):
    ret = ''
    time_ranges = [
      TimeRange('1m', 60),
      TimeRange('1h', 60 * 60),
      TimeRange('1d', 24 * 60 * 60, True),
      TimeRange('1w', 7 * 24 * 60 * 60),
      TimeRange('1m', 30 * 24 * 60 * 60),
      TimeRange('1y', 365 * 24 * 60 * 60),
    ]
    time_controls = []
    for time_range in time_ranges:
      checked = 'checked' if time_range.is_default else ''
      time_control = '''<input type="radio" name="{internal_name}_range" 
          onclick="{internal_name}_range = {range_seconds};
          update_chart('{internal_name}');" {checked}> {label}'''.format(
          internal_name=self.internal_name,
          range_seconds=time_range.range_seconds,
          checked=checked,
          label=time_range.str)
      time_controls.append(time_control)

    y_axis_additional_settings = ''

    if hasattr(self, 'enum_values') and self.enum_values:
      cases = [ 'case {num}: return "{str}";'.format(num=i,
          str=self.enum_values[i]) for i in range(len(self.enum_values)) ]

      y_axis_additional_settings = '''
      ticks: {{
        min: 0,
        max: {max_y},
        stepSize: 1,
        callback: function(label, index, labels) {{
          switch (label) {{
            {cases}
          }}
        }}
      }}'''.format(max_y=(len(self.enum_values) - 1), cases='\n'.join(cases))

    chart_code = '''<div class="var-cell">
    {controls}
    <div>
    <canvas id="{internal_name}_chart" width="400" height="400"></canvas>
    </div>
    <script>
    var {internal_name}_range = 24 * 60 * 60;
    var ctx = document.getElementById('{internal_name}_chart').getContext('2d');
    var {internal_name}_chart = new Chart(ctx, {{
      type: 'line',
      data: {{
        datasets: [{{
          label: '{display_name}',
          backgroundColor: Chart.helpers.color('{colour}').alpha(0.3).rgbString(),
          borderColor: '{colour}',
          data: [
          //{{ x: moment.unix(0).toDate(), y: 0 }}
          ],
          showLine: true,
          datalabels: {{
            //align: 'right',
            clip: true,
            display: function(context) {{
              // Only on last value
              return context.dataIndex == (context.dataset.data.length - 1);
            }},
            formatter: function(value, context) {{
              return '' + value.y.toFixed(2);
            }}
          }}
        }}]
      }},
      options: {{
        responsive: false,
        maintainAspectRatio: false,
        animation: {{
            duration: 0
        }},
        hover: {{
            animationDuration: 0
        }},
        responsiveAnimationDuration: 0,
        elements: {{
          line: {{
            tension: 0
          }}
        }},
        scales: {{
          xAxes: [{{
            type: 'time',
            display: true,
            time: {{
              minUnit: 'second',
            }},
            ticks: {{
              major: {{
                fontStyle: 'bold',
              }}
            }}
          }}],
          yAxes: [{{
            display: true,
            scaleLabel: {{
              display: false,
              labelString: 'value'
            }},
            {y_axis_additional_settings}
          }}]
        }},
        title: {{
          display: true,
          text: '{display_name}',
          fontSize: 24,
        }},
        legend: {{
          display: false,
        }},
        tooltips: {{
            callbacks: {{
                label: function(tooltipItem, data) {{
                    var label = Math.round(tooltipItem.yLabel * 100) / 100;
                    return label;
                }}
            }}
        }}
      }}
    }});
    update_chart('{internal_name}');
    </script></div>
    '''.format(internal_name=self.internal_name, display_name=self.display_name,
        controls=' '.join(time_controls),
        y_axis_additional_settings=y_axis_additional_settings,
        colour=self.colour)
    ret += chart_code
    return ret

  def get_display_name(self):
    return self.display_name

  def get_aggregation_function(self):
    return self.aggregation_function
