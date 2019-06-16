function update_field_by_id(id, value) {
  document.getElementById(id).innerHTML = value
}

function apply_updates_loop(first_time) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      eval(this.responseText);

      // We set timeout here because there is no point queuing up more requests
      // if the previous one hasn't come back.
      setTimeout(function(){ apply_updates_loop(false); }, 1000);
    }
  };
  var path = "/aggregated_updates";
  if (first_time) {
    path = "/historical_values";
  }
  xhttp.open("GET", path, true);
  xhttp.send();
}

window.onload=function () { apply_updates_loop(true); };