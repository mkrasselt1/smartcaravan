var websocket;

function initWebsocket() {
  console.info("Init Websocket");
  websocket = new WebSocket('ws://'+window.location.host+'/ws');
  websocket.onopen = onOpen;
  websocket.onerror = onError;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onClose(){
  console.info("End Websocket");
  setTimeout(initWebsocket, 2000);
}

function onError(event) {
  console.error(event);
  setTimeout(initWebsocket, 2000);
};

function onOpen(event) {
  console.log("Connection Established")
};

function onMessage (event) {
  console.log("Rec:", event.data)
  parsed = JSON.parse(event.data)
  $('#all').html(event.data);
  
  $('#model').html(parsed["model"]);
  $('#serialnumber').html(parsed["serial"]);
  $('#firmware').html(parsed["firmware"]);
  
  $('#charge').html(parsed["charge"]);
  $('#error').html(parsed["error"]);
  
  $('#water_level').html(Math.round(parsed["water"]/100*55));
  $('#water_level_gauge').val(parsed["water"]);

  $('#battery_level').html(parsed["vbat"]);
  $('#battery_level_gauge').val(parsed["vbat"]);
};

$(document).ready(() => {
  initWebsocket();
  console.log("ready!");
  $("#debug").click(()=>{
    $("#all").toggle();
  });
});