#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

// Wi-Fi Access Point Credentials
const char* ssid = "ESP8266_Servo_Control";
const char* password = "password123";

AsyncWebServer server(80);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo Pulse Values (Adjust according to your servo)
#define SERVOMIN  150
#define SERVOMAX  600

// Memory Storage Struct for EEPROM
struct AppState {
  uint8_t magic; // To check if EEPROM was formatted
  bool autoMode;
  bool servoState[16];
  int targetAngle[16];
  int servoSpeed[16];
} appState;

// Variables
int targetAngle[16];
float currentAngle[16];
int servoSpeed[16];
bool servoState[16];
int sweepDir[16]; 
unsigned long lastUpdate[16];
bool autoMode = false;

// HTML, CSS, at JS Webpage (Unchanged)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Pro Servo Controller</title>
  <style>
    :root { --bg: #f4f6f9; --card: #fff; --primary: #007bff; --success: #28a745; --danger: #dc3545; --text: #333; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; }
    h1 { text-align: center; color: #1a1a1a; margin-bottom: 20px; font-weight: 700; font-size: 2rem; }
    
    .top-panel { background: var(--card); padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); margin: 0 auto 30px auto; max-width: 1400px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 15px; }
    .btn-reset { background: var(--danger); color: white; border: none; padding: 10px 20px; border-radius: 6px; font-size: 1rem; cursor: pointer; font-weight: 600; transition: 0.3s; }
    .btn-reset:hover { background: #c82333; }
    
    .mode-toggle { display: flex; align-items: center; gap: 10px; font-weight: 600; font-size: 1.1rem; }
    
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; max-width: 1400px; margin: 0 auto; }
    .card { background: var(--card); padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); transition: transform 0.2s; position: relative; }
    .card:hover { transform: translateY(-2px); box-shadow: 0 6px 16px rgba(0,0,0,0.1); }
    .card-header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #eee; padding-bottom: 10px; margin-bottom: 15px; }
    .card-header h3 { margin: 0; font-size: 1.2rem; color: #444; }
    .control-group { margin-bottom: 15px; }
    .control-header { display: flex; justify-content: space-between; font-size: 0.9rem; font-weight: 600; margin-bottom: 8px; color: #666; }
    .val { color: var(--primary); font-weight: bold; }
    
    .auto-overlay { display: none; position: absolute; top: 60px; left: 0; width: 100%; height: calc(100% - 60px); background: rgba(255,255,255,0.8); z-index: 10; justify-content: center; align-items: center; font-weight: bold; color: var(--primary); font-size: 1.2rem; border-radius: 0 0 12px 12px; }
    
    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 18px; width: 18px; border-radius: 50%; background: var(--primary); cursor: pointer; margin-top: -6px; }
    input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 6px; cursor: pointer; background: #ddd; border-radius: 3px; }
    
    .switch { position: relative; display: inline-block; width: 50px; height: 28px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 28px; }
    .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--success); }
    input:checked + .slider:before { transform: translateX(22px); }
  </style>
</head>
<body>
  <h1>Servo Command Center</h1>
  
  <div class="top-panel">
    <button class="btn-reset" onclick="resetAll()">⚠️ Reset All Data</button>
    <div class="mode-toggle">
      <span>Manual</span>
      <label class="switch">
        <input type="checkbox" id="masterMode" onchange="toggleAutoMode()">
        <span class="slider"></span>
      </label>
      <span style="color: var(--primary);">Automatic (Min to Max)</span>
    </div>
  </div>

  <div class="grid" id="servo-container"></div>

  <script>
    const container = document.getElementById('servo-container');
    let html = '';
    
    for(let i=0; i<16; i++) {
      html += `
        <div class="card">
          <div class="card-header">
            <h3>Channel ${i}</h3>
            <label class="switch">
              <input type="checkbox" id="sw_${i}" onchange="sendData(${i})">
              <span class="slider"></span>
            </label>
          </div>
          <div class="auto-overlay" id="overlay_${i}">Automatic Sweep Active</div>
          <div class="control-group">
            <div class="control-header"><span>Target Angle</span><span class="val" id="ang_val_${i}">90&deg;</span></div>
            <input type="range" id="ang_${i}" min="0" max="180" value="90" oninput="document.getElementById('ang_val_${i}').innerHTML=this.value+'&deg;'" onchange="sendData(${i})">
          </div>
          <div class="control-group">
            <div class="control-header"><span>Movement Speed</span><span class="val" id="spd_val_${i}">Fast (10)</span></div>
            <input type="range" id="spd_${i}" min="1" max="10" value="10" oninput="updateSpeedLabel(${i}, this.value)" onchange="sendData(${i})">
          </div>
        </div>
      `;
    }
    container.innerHTML = html;

    window.onload = function() {
      fetch('/get').then(r => r.json()).then(data => {
        document.getElementById('masterMode').checked = (data.auto == 1);
        handleModeUI(data.auto == 1);
        
        for(let i=0; i<16; i++) {
          document.getElementById('sw_'+i).checked = (data.servos[i].st == 1);
          document.getElementById('ang_'+i).value = data.servos[i].a;
          document.getElementById('ang_val_'+i).innerHTML = data.servos[i].a + '&deg;';
          document.getElementById('spd_'+i).value = data.servos[i].sp;
          updateSpeedLabel(i, data.servos[i].sp);
        }
      });
    }

    function updateSpeedLabel(id, val) {
      let label = val == 10 ? 'Instant (10)' : val == 1 ? 'Slowest (1)' : val;
      document.getElementById('spd_val_'+id).innerHTML = label;
    }

    function sendData(i) {
      let state = document.getElementById('sw_'+i).checked ? 1 : 0;
      let angle = document.getElementById('ang_'+i).value;
      let speed = document.getElementById('spd_'+i).value;
      fetch(`/set?s=${i}&on=${state}&a=${angle}&sp=${speed}`);
    }

    function toggleAutoMode() {
      let isAuto = document.getElementById('masterMode').checked;
      handleModeUI(isAuto);
      fetch(`/mode?val=${isAuto ? 1 : 0}`);
    }

    function handleModeUI(isAuto) {
      for(let i=0; i<16; i++) {
        document.getElementById('overlay_'+i).style.display = isAuto ? 'flex' : 'none';
      }
    }

    function resetAll() {
      if(confirm("Burahin lahat ng nakasave at ibalik sa simula?")) {
        fetch('/reset').then(() => {
          setTimeout(() => location.reload(), 500); 
        });
      }
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Setup I2C for ESP8266 (SDA = D2/GPIO4, SCL = D1/GPIO5)
  Wire.begin(4, 5); 
  
  // Setup PCA9685
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(60);

  // Initialize EEPROM (Reserve 256 bytes for our struct)
  EEPROM.begin(256);
  EEPROM.get(0, appState);

  // Check if EEPROM has been initialized previously
  if(appState.magic != 0xAA) {
    appState.magic = 0xAA;
    appState.autoMode = false;
    for(int i=0; i<16; i++) {
      appState.servoState[i] = false;
      appState.targetAngle[i] = 90;
      appState.servoSpeed[i] = 10;
    }
    EEPROM.put(0, appState);
    EEPROM.commit();
  }

  // Load Saved States 
  autoMode = appState.autoMode;
  for(int i=0; i<16; i++) {
    servoState[i] = appState.servoState[i];
    targetAngle[i] = appState.targetAngle[i];
    servoSpeed[i] = appState.servoSpeed[i];
    
    currentAngle[i] = targetAngle[i];
    sweepDir[i] = 1; 
    lastUpdate[i] = millis();

    // Set initial physical positions
    if(servoState[i]) {
      int pulse = map(currentAngle[i], 0, 180, SERVOMIN, SERVOMAX);
      pwm.setPWM(i, 0, pulse);
    } else {
      pwm.setPWM(i, 0, 4096); 
    }
  }

  // Setup Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);

  // mDNS Setup
  if (MDNS.begin("servo")) {
    Serial.println("MDNS responder started. Access via http://servo.local");
  }

  // --- Async Web Server Routes ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"auto\":" + String(autoMode ? 1 : 0) + ",\"servos\":[";
    for(int i=0; i<16; i++) {
      json += "{\"st\":" + String(servoState[i] ? 1 : 0) + ",";
      json += "\"a\":" + String(targetAngle[i]) + ",";
      json += "\"sp\":" + String(servoSpeed[i]) + "}";
      if(i < 15) json += ",";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("s") && request->hasParam("on") && request->hasParam("a") && request->hasParam("sp")) {
      int s = request->getParam("s")->value().toInt();
      
      // Update running arrays
      servoState[s] = (request->getParam("on")->value().toInt() == 1);
      targetAngle[s] = request->getParam("a")->value().toInt();
      servoSpeed[s] = request->getParam("sp")->value().toInt();
      
      // Update struct and save to EEPROM
      appState.servoState[s] = servoState[s];
      appState.targetAngle[s] = targetAngle[s];
      appState.servoSpeed[s] = servoSpeed[s];
      EEPROM.put(0, appState);
      EEPROM.commit();
      
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("val")) {
      autoMode = (request->getParam("val")->value().toInt() == 1);
      appState.autoMode = autoMode;
      EEPROM.put(0, appState);
      EEPROM.commit();
      
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    appState.magic = 0xAA;
    appState.autoMode = false;
    autoMode = false;
    
    for(int i=0; i<16; i++) {
      appState.servoState[i] = false;
      appState.targetAngle[i] = 90;
      appState.servoSpeed[i] = 10;
      
      servoState[i] = false;
      targetAngle[i] = 90;
      servoSpeed[i] = 10;
      currentAngle[i] = 90;
      pwm.setPWM(i, 0, 4096); 
    }
    
    EEPROM.put(0, appState);
    EEPROM.commit();
    request->send(200, "text/plain", "RESET_OK");
  });

  server.begin();
  Serial.println("Async HTTP server started");
}

void loop() {
  MDNS.update(); // Required for ESP8266 mDNS to work properly in the loop
  
  unsigned long currentMillis = millis();
  
  // Hardware Servo Movement Logic
  for(int i=0; i<16; i++) {
    if(!servoState[i]) {
      pwm.setPWM(i, 0, 4096); 
      continue;
    }

    int delayInterval = map(servoSpeed[i], 1, 10, 30, 0); 
    
    // ======== AUTOMATIC MODE ========
    if(autoMode) {
      if(delayInterval == 0) delayInterval = 5; 
      
      if(currentMillis - lastUpdate[i] >= delayInterval) {
        lastUpdate[i] = currentMillis;
        
        currentAngle[i] += sweepDir[i];
        
        if(currentAngle[i] >= 180) {
          currentAngle[i] = 180;
          sweepDir[i] = -1; 
        } else if(currentAngle[i] <= 0) {
          currentAngle[i] = 0;
          sweepDir[i] = 1;  
        }
        
        int pulse = map(currentAngle[i], 0, 180, SERVOMIN, SERVOMAX);
        pwm.setPWM(i, 0, pulse);
      }
    } 
    // ======== MANUAL MODE ========
    else {
      if(delayInterval == 0) {
        currentAngle[i] = targetAngle[i];
        int pulse = map(currentAngle[i], 0, 180, SERVOMIN, SERVOMAX);
        pwm.setPWM(i, 0, pulse);
      } else {
        if(currentAngle[i] != targetAngle[i]) {
          if(currentMillis - lastUpdate[i] >= delayInterval) {
            lastUpdate[i] = currentMillis;
            
            if(currentAngle[i] < targetAngle[i]) currentAngle[i]++;
            else if(currentAngle[i] > targetAngle[i]) currentAngle[i]--;
            
            int pulse = map(currentAngle[i], 0, 180, SERVOMIN, SERVOMAX);
            pwm.setPWM(i, 0, pulse);
          }
        }
      }
    }
  }
}