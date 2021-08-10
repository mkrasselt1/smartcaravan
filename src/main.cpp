/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// Import required libraries
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include <FS.h>
#include <Wire.h>

// Set LED GPIO
#define ledPin 2
#define sensorPin A0
#define VED_RX D8
#define VED_TX D7
// Replace with your network credentials
const char* ssid = "smartcaravan";
const char* password = "smartcaravan";

//String value;
int timeout = 12;
int intValue;
bool victronRead = false;
SimpleTimer readWaterLevelTimer(1000);
SimpleTimer victronReadTimer(5000);
SoftwareSerial Vser(VED_RX, VED_TX);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

struct sensorData{
  int waterlevel = 0;
  char model[32] = "";
  char error[60] = "";
  char tracker[8] = "";
  char charge[30] = "";
  char serialnumber[12] = "";
  float vbat = 0;
  float ibat = 0;
  
  float pcon = 0;
  float econ = 0;

  float ppv = 0;
  float vpv = 0;
  float temperature = 0;
  float soc = 0;
  int day = 0;
  float time_reminaing = 0;

  float prod_today = 0;
  float prod_yesterday = 0;
  float prod_total = 0;
  float max_power_today = 0;
  float max_power_yesterday = 0;
  float total_charged = 0;
  float total_discharged = 0;
  float firmware_version = 0;
};

sensorData sensors;

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
  }
}

int getWaterLevel() {
  return map(1023-analogRead(sensorPin), 2, 435, 0, 100);
}

void sendCaravanData(){
  StaticJsonDocument<450> caravanData;
  String output = "";
  caravanData["water"] = sensors.waterlevel;
  caravanData["model"] = sensors.model;
  caravanData["error"] = sensors.error;
  caravanData["tracker"] = sensors.tracker;
  caravanData["charge"] = sensors.charge;
  caravanData["serial"] = sensors.serialnumber;
  caravanData["vbat"] = sensors.vbat;
  caravanData["ibat"] = sensors.ibat;
  caravanData["pcon"] = sensors.pcon;
  caravanData["econ"] = sensors.econ;

  caravanData["ppv"] = sensors.ppv;
  caravanData["vpv"] = sensors.vpv;
  caravanData["temp"] = sensors.temperature;
  caravanData["soc"] = sensors.soc;
  caravanData["day"] = sensors.day;
  caravanData["time_rem"] = sensors.time_reminaing;

  caravanData["prod_tod"] = sensors.prod_today;
  caravanData["prod_yest"] = sensors.prod_yesterday;
  caravanData["prod_total"] = sensors.prod_total;
  caravanData["max_pow_tod"] = sensors.max_power_today;
  caravanData["max_pow_yest"] = sensors.max_power_yesterday;
  caravanData["tot_charged"] = sensors.total_charged;
  caravanData["tot_discharged"] = sensors.total_discharged;
  caravanData["firmware"] = sensors.firmware_version;
  serializeJson(caravanData, output);
  ws.textAll(output);
}

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        sendCaravanData();
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}
 
void setup(){
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, false);
  // Serial port for debugging purposes
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  pinMode(ledPin, OUTPUT);

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  /*
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    timeout--;
    if(!timeout){
      ESP.restart();
    }
    Serial.print(".");
  }
  digitalWrite(ledPin, true);
  Serial.println("");
  Serial.println(WiFi.localIP());
  */
  if(WiFi.softAP(ssid, password)){
    Serial.println("Ready");
  } else {
    Serial.println("Failed!");
  }
  
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);
  
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  
  // Start server
  server.begin();
  Vser.begin(19200);
  Vser.enableRx(false);
}
 
void loop(){
  ws.cleanupClients();
  if (readWaterLevelTimer.isReady()) {            // Check is ready a first timer
    readWaterLevelTimer.reset();
    sensors.waterlevel = getWaterLevel();
    sendCaravanData();
  }
  if(victronReadTimer.isReady()){
    victronReadTimer.reset();
    if(victronRead){
      //Start Reading Victron
      Vser.enableRx(true);
      Serial.println("serial recieve enable");
    }else{
      Vser.enableRx(false);
      Serial.println("serial recieve disable");
    }
    victronRead=!victronRead;
  }

  //Victron Direkt
  if (Vser.available()){
    String label, val;
    char buf[45];
    label = Vser.readStringUntil('\t');                // this is the actual line that reads the label from the MPPT controller
    val = Vser.readStringUntil('\r\r\n');              // this is the line that reads the value of the label
    char charBufL[label.length() + 1];
    label.toCharArray(charBufL, label.length() + 1);
    if (label == "PID") {
      //Model
      long A = strtol(val.c_str(), NULL, 0);
      String model;
      switch (A) {
        case 0x203  : model = "BMV-700";  break;
        case 0x204  : model = "BMV-702";  break;
        case 0x205  : model = "BMV-700H";  break;
        case 0xA389 : model = "SmartShunt";  break;
        case 0xA381 : model = "BMV-712 Smart";  break;
        case 0xA04C : model = "BlueSolar MPPT 75/10";  break;
        case 0x300  : model = "BlueSolar MPPT 70/15";  break;
        case 0xA042 : model = "BlueSolar MPPT 75/15";  break;
        case 0xA043 : model = "BlueSolar MPPT 100/15";  break;
        case 0xA044 : model = "BlueSolar MPPT 100/30 rev1";  break;
        case 0xA04A : model = "BlueSolar MPPT 100/30 rev2";  break;
        case 0xA041 : model = "BlueSolar MPPT 150/35 rev1";  break;
        case 0xA04B : model = "BlueSolar MPPT 150/35 rev2";  break;
        case 0xA04D : model = "BlueSolar MPPT 150/45";  break;
        case 0xA040 : model = "BlueSolar MPPT 75/50";  break;
        case 0xA045 : model = "BlueSolar MPPT 100/50 rev1";  break;
        case 0xA049 : model = "BlueSolar MPPT 100/50 rev2";  break;
        case 0xA04E : model = "BlueSolar MPPT 150/60";  break;
        case 0xA046 : model = "BlueSolar MPPT 150/70";  break;
        case 0xA04F : model = "BlueSolar MPPT 150/85";  break;
        case 0xA047 : model = "BlueSolar MPPT 150/100";  break;
        case 0xA050 : model = "SmartSolar MPPT 250/100";  break;
        case 0xA051 : model = "SmartSolar MPPT 150/100";  break;
        case 0xA052 : model = "SmartSolar MPPT 150/85";  break;
        case 0xA053 : model = "SmartSolar MPPT 75/15";  break;
        case 0xA054 : model = "SmartSolar MPPT 75/10";  break;
        case 0xA055 : model = "SmartSolar MPPT 100/15";  break;
        case 0xA056 : model = "SmartSolar MPPT 100/30";  break;
        case 0xA057 : model = "SmartSolar MPPT 100/50";  break;
        case 0xA058 : model = "SmartSolar MPPT 150/35";  break;
        case 0xA059 : model = "SmartSolar MPPT 150/100 rev2";  break;
        case 0xA05A : model = "SmartSolar MPPT 150/85 rev2";  break;
        case 0xA05B : model = "SmartSolar MPPT 250/70";  break;
        case 0xA05C : model = "SmartSolar MPPT 250/85";  break;
        case 0xA05D : model = "SmartSolar MPPT 250/60";  break;
        case 0xA05E : model = "SmartSolar MPPT 250/45";  break;
        case 0xA05F : model = "SmartSolar MPPT 100/20";  break;
        case 0xA060 : model = "SmartSolar MPPT 100/20 48V";  break;
        case 0xA061 : model = "SmartSolar MPPT 150/45";  break;
        case 0xA062 : model = "SmartSolar MPPT 150/60";  break;
        case 0xA063 : model = "SmartSolar MPPT 150/70";  break;
        case 0xA064 : model = "SmartSolar MPPT 250/85 rev2";  break;
        case 0xA065 : model = "SmartSolar MPPT 250/100 rev2";  break;
        case 0xA201 : model = "Phoenix Inverter 12V 250VA 230V";  break;
        case 0xA202 : model = "Phoenix Inverter 24V 250VA 230V";  break;
        case 0xA204 : model = "Phoenix Inverter 48V 250VA 230V";  break;
        case 0xA211 : model = "Phoenix Inverter 12V 375VA 230V";  break;
        case 0xA212 : model = "Phoenix Inverter 24V 375VA 230V";  break;
        case 0xA214 : model = "Phoenix Inverter 48V 375VA 230V";  break;
        case 0xA221 : model = "Phoenix Inverter 12V 500VA 230V";  break;
        case 0xA222 : model = "Phoenix Inverter 24V 500VA 230V";  break;
        case 0xA224 : model = "Phoenix Inverter 48V 500VA 230V";  break;
        case 0xA231 : model = "Phoenix Inverter 12V 250VA 230V";  break;
        case 0xA232 : model = "Phoenix Inverter 24V 250VA 230V";  break;
        case 0xA234 : model = "Phoenix Inverter 48V 250VA 230V";  break;
        case 0xA239 : model = "Phoenix Inverter 12V 250VA 120V";  break;
        case 0xA23A : model = "Phoenix Inverter 24V 250VA 120V";  break;
        case 0xA23C : model = "Phoenix Inverter 48V 250VA 120V";  break;
        case 0xA241 : model = "Phoenix Inverter 12V 375VA 230V";  break;
        case 0xA242 : model = "Phoenix Inverter 24V 375VA 230V";  break;
        case 0xA244 : model = "Phoenix Inverter 48V 375VA 230V";  break;
        case 0xA249 : model = "Phoenix Inverter 12V 375VA 120V";  break;
        case 0xA24A : model = "Phoenix Inverter 24V 375VA 120V";  break;
        case 0xA24C : model = "Phoenix Inverter 48V 375VA 120V";  break;
        case 0xA251 : model = "Phoenix Inverter 12V 500VA 230V";  break;
        case 0xA252 : model = "Phoenix Inverter 24V 500VA 230V";  break;
        case 0xA254 : model = "Phoenix Inverter 48V 500VA 230V";  break;
        case 0xA259 : model = "Phoenix Inverter 12V 500VA 120V";  break;
        case 0xA25A : model = "Phoenix Inverter 24V 500VA 120V";  break;
        case 0xA25C : model = "Phoenix Inverter 48V 500VA 120V";  break;
        case 0xA261 : model = "Phoenix Inverter 12V 800VA 230V";  break;
        case 0xA262 : model = "Phoenix Inverter 24V 800VA 230V";  break;
        case 0xA264 : model = "Phoenix Inverter 48V 800VA 230V";  break;
        case 0xA269 : model = "Phoenix Inverter 12V 800VA 120V";  break;
        case 0xA26A : model = "Phoenix Inverter 24V 800VA 120V";  break;
        case 0xA26C : model = "Phoenix Inverter 48V 800VA 120V";  break;
        case 0xA271 : model = "Phoenix Inverter 12V 1200VA 230V";  break;
        case 0xA272 : model = "Phoenix Inverter 24V 1200VA 230V";  break;
        case 0xA274 : model = "Phoenix Inverter 48V 1200VA 230V";  break;
        case 0xA279 : model = "Phoenix Inverter 12V 1200VA 120V";  break;
        case 0xA27A : model = "Phoenix Inverter 24V 1200VA 120V";  break;
        case 0xA27C : model = "Phoenix Inverter 48V 1200VA 120V";  break;
        default:
          model = "model not detected";
      }
      model.toCharArray(sensors.model, model.length() + 1);
    } else if (label == "I"){
      // In this case I chose to read charging current
      val.toCharArray(buf, sizeof(buf));
      sensors.ibat = atof(buf) / 1000;
    } else if (label == "T") {
      val.toCharArray(buf, sizeof(buf));
      sensors.temperature = atof(buf) / 1000;
    } else if (label == "V") {
      val.toCharArray(buf, sizeof(buf));
      sensors.vbat = atof(buf) / 1000;
    } else if (label == "PPV") {
      val.toCharArray(buf, sizeof(buf));
      sensors.ppv = atof(buf);
    } else if (label == "VPV") {
      //Voltage PV
      val.toCharArray(buf, sizeof(buf));
      sensors.vpv = atof(buf) / 1000;
    } else if (label == "H20") {
      val.toCharArray(buf, sizeof(buf));
      sensors.prod_today = atof(buf) / 100;
    } else if (label == "H22") {
      //Yield yesterday, kWh
      val.toCharArray(buf, sizeof(buf));
      sensors.prod_yesterday = atof(buf) / 100;
    } else if (label == "H19") {
      //-- Yield total, kWh
      val.toCharArray(buf, sizeof(buf));
      sensors.prod_total = atof(buf) / 100;
    } else if (label == "H21") {
      //Maximum power today, W
      val.toCharArray(buf, sizeof(buf));
      sensors.max_power_today = atof(buf);
    } else if (label == "H23") {
      //Maximum power yesterday, W
      val.toCharArray(buf, sizeof(buf));
      sensors.max_power_yesterday = atof(buf);
    } else if (label == "FW") {
      //FW 119     -- Firmware version of controller, v1.19
      val.toCharArray(buf, sizeof(buf));
      sensors.firmware_version = atof(buf) / 100;
    } else if (label == "HSDS") {
      //Day sequence number (0..364)
      val.toCharArray(buf, sizeof(buf));
      sensors.day = atoi(buf);
    } else if (label == "MPPT") {
      val.toCharArray(buf, sizeof(buf));
      intValue = atof(buf);
      String tracker;
      switch (intValue) {
        case 0 : tracker = "Off"; break;
        case 1 : tracker = "Limited"; break;
        case 2 : tracker = "Active"; break;
        default:
          tracker = "unknown";
        tracker.toCharArray(sensors.tracker, tracker.length() + 1);
      }
    } else if (label == "SOC") { 
      // State Of Charge of the battery
      val.toCharArray(buf, sizeof(buf));
      sensors.soc = atof(buf) / 10 ;
    } else if (label == "TTG") {
      // Time To Go in decimal hours
      val.toCharArray(buf, sizeof(buf));
      sensors.time_reminaing = atof(buf) / 60;
    } else if (label == "P") {
      // Instant power
      val.toCharArray(buf, sizeof(buf));
      sensors.pcon = atof(buf);
    } else if (label == "CE") { 
      //Consumption of engery in A/h
      val.toCharArray(buf, sizeof(buf));
      sensors.econ = atof(buf) / 1000;
    } else if (label == "H17") {
      // Total discharged enery from the first power on in kWh
      val.toCharArray(buf, sizeof(buf));
      sensors.total_discharged = atof(buf) / 100;
    } else if (label == "H18") {
      // Total charged enery from the first power on in kWh
      val.toCharArray(buf, sizeof(buf));
      sensors.total_charged = atof(buf) / 100;
    } else if (label == "ERR") {
      // This routine reads the error code.
      val.toCharArray(buf, sizeof(buf));
      intValue = atoi(buf);
      String error;
      switch (intValue) {
        case   0: error = "No error"; break;
        case   2: error = "Battery voltage too high"; break;
        case  17: error = "Charger temperature too high"; break;
        case  18: error = "Charger over current"; break;
        case  19: error = "Charger current reversed"; break;
        case  20: error = "Bulk time limit exceeded"; break;
        case  21: error = "Current sensor issue"; break;
        case  26: error = "Terminals overheated"; break;
        case  28: error = "Converter issue"; break;
        case  33: error = "Input voltage too high (solar panel)"; break;
        case  34: error = "Input current too high (solar panel)"; break;
        case  38: error = "Input shutdown (excessive battery voltage)"; break;
        case  39: error = "Input shutdown (due to current flow during off mode)"; break;
        case  65: error = "Lost communication with one of devices"; break;
        case  66: error = "Synchronised charging device configuration issue"; break;
        case  67: error = "BMS connection lost"; break;
        case  68: error = "Network misconfigured"; break;
        case 116: error = "Factory calibration data lost"; break;
        case 117: error = "Invalid/incompatible firmware"; break;
        case 119: error = "User settings invalid"; break;
        default:
          error = "unknown";
      }
      error.toCharArray(sensors.error, error.length() + 1);
    } else if (label == "SER#") {
      // This routine reads the Serial Number.
      val.toCharArray(sensors.serialnumber, val.length() + 1);
    } else if (label == "CS") {
      //  Charge Status
      val.toCharArray(buf, sizeof(buf));
      intValue = atoi(buf);
      String state;
      switch (intValue) {
        case   0 : state = "Off"; break;
        case   2 : state = "Fault"; break;
        case   3 : state = "Bulk"; break;
        case   4 : state = "Absorption"; break;
        case   5 : state = "Float"; break;
        case   7 : state = "Equalize (manual)"; break;
        case 245 : state = "Starting-up"; break;
        case 247 : state = "Auto equalize / Recondition"; break;
        case 252 : state = "External control"; break;
        default:
          state = "unknown";
      }
      state.toCharArray(sensors.charge, state.length() + 1);
    }/*else{
      Serial.print("Unknown Label:");
      Serial.print(label);
      Serial.print(":");
      Serial.println(val);
    }*/
  }
  /*if (WiFi.status() != WL_CONNECTED) {
    ESP.restart();
  }*/
}
