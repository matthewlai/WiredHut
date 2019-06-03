function update_field_by_id(id, value) {
  document.getElementById(id).innerHTML = value
}

function apply_updates_loop() {
  setInterval(function() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        eval(this.responseText);
      }
    };
    xhttp.open("GET", "/aggregated_updates", true);
    xhttp.send();
  }, 1000);
}

window.onload=apply_updates_loop;