function update_field_by_id(id, value) {
  document.getElementById(id).innerHTML = value
}

function apply_updates_loop() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      eval(this.responseText);

      // We set timeout here because there is no point queuing up more requests
      // if the previous one hasn't come back.
      setTimeout(function(){ apply_updates_loop(); }, 1000);
    }
  };
  xhttp.open("GET", "/aggregated_updates", true);
  xhttp.send();
}

window.onload=apply_updates_loop;