function update_chart(name) {
  var chart = window[name + "_chart"];
  var range_seconds = window[name + "_range"];
  var data = chart.data.datasets[0].data;
  var last_time_point;
  if (data.length == 0) {
    return;
  } else {
    last_time_point = moment(data[data.length - 1].x);
  }
  var new_chart_begin = moment(last_time_point);
  new_chart_begin = new_chart_begin.subtract(range_seconds, 's');
  chart.options.scales.xAxes[0].time.min = new_chart_begin.toDate();
  chart.options.scales.xAxes[0].time.max = last_time_point.toDate();
  chart.update();
}

function add_data(name, value_pairs) {
  var chart = window[name + "_chart"];
  var data = chart.data.datasets[0].data;
  var last_time_point;
  if (data.length != 0 && value_pairs.length == 1) {
    last_time_point = data[data.length - 1].x;
    if (last_time_point == value_pairs[0].x) {
      return;
    }
  }

  for (var i = 0; i < value_pairs.length; ++i) {
    data.push(value_pairs[i]);
  }

  data.sort(function(a, b){return a.x - b.x});
  update_chart(name);
}
