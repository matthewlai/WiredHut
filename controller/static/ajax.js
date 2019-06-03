function load_val_impl(path, id) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById(id).innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", path, true);
  xhttp.send();
}

function load_val(path, id, auto_refresh) {
  load_val_impl(path, id);
  if (auto_refresh) {
    setInterval(function() {load_val_impl(path, id); }, 1000);
  }
}