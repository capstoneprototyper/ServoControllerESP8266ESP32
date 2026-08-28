#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Preferences.h>
#include <ESPmDNS.h>

// Wi-Fi Access Point Credentials
const char* ssid = "ESP32_Servo_Control";
const char* password = "password123";

AsyncWebServer server(80);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
Preferences preferences;

// Servo Pulse Values (Adjust according to your servo)
#define SERVOMIN  150
#define SERVOMAX  600

// Variables
int targetAngle[16];
float currentAngle[16];
int servoSpeed[16];
bool servoState[16];
int sweepDir[16]; // Ginagamit para sa Auto Mode (1 = pataas, -1 = pababa)
unsigned long lastUpdate[16];
bool autoMode = false;

// HTML, CSS, at JS Webpage
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
    
    /* Top Controls Panel */
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
    
    /* Auto Mode Overlay */
    .auto-overlay { display: none; position: absolute; top: 60px; left: 0; width: 100%; height: calc(100% - 60px); background: rgba(255,255,255,0.8); z-index: 10; justify-content: center; align-items: center; font-weight: bold; color: var(--primary); font-size: 1.2rem; border-radius: 0 0 12px 12px; }
    
    /* Range Sliders */
    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 18px; width: 18px; border-radius: 50%; background: var(--primary); cursor: pointer; margin-top: -6px; }
    input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 6px; cursor: pointer; background: #ddd; border-radius: 3px; }
    
    /* Toggle Switch */
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
    
    // Generate 16 Channel Cards UI
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

    // Load State on Page Load (WebAsync Persistence)
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
          setTimeout(() => location.reload(), 500); // Reload page after reset
        });
      }
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Setup I2C and PCA9685
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(60);

  // Initialize Preferences (Memory Storage)
  preferences.begin("servo_app", false);

  // Load Saved States o ilagay sa Default kung walang nakasave
  autoMode = preferences.getBool("autoMode", false);
  
  for(int i=0; i<16; i++) {
    String pState = "st" + String(i);
    String pAng = "ang" + String(i);
    String pSpd = "spd" + String(i);

    servoState[i] = preferences.getBool(pState.c_str(), false); // Default: OFF (false)
    targetAngle[i] = preferences.getInt(pAng.c_str(), 90);      // Default: 90 deg
    servoSpeed[i] = preferences.getInt(pSpd.c_str(), 10);       // Default: Speed 10
    
    currentAngle[i] = targetAngle[i];
    sweepDir[i] = 1; // Pataas ang direksyon initially for Auto Mode
    lastUpdate[i] = millis();

    // Set initial physical positions
    if(servoState[i]) {
      int pulse = map(currentAngle[i], 0, 180, SERVOMIN, SERVOMAX);
      pwm.setPWM(i, 0, pulse);
    } else {
      pwm.setPWM(i, 0, 4096); // I-off ang PWM kung naka-off
    }
  }

  // Setup Wi-Fi Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);

  // mDNS Setup - Pangalan ng link
  if (MDNS.begin("servo")) {
    Serial.println("MDNS responder started. Access via http://servo.local");
  }

  // --- Async Web Server Routes ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // I-send ang current saved state bilang JSON para sa Page Load (WebAsync Persistency)
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

  // Tanggapin ang changes galing sa Web UI at I-save sa Memory
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("s") && request->hasParam("on") && request->hasParam("a") && request->hasParam("sp")) {
      int s = request->getParam("s")->value().toInt();
      servoState[s] = (request->getParam("on")->value().toInt() == 1);
      targetAngle[s] = request->getParam("a")->value().toInt();
      servoSpeed[s] = request->getParam("sp")->value().toInt();
      
      // I-save permanently!
      preferences.putBool(("st"+String(s)).c_str(), servoState[s]);
      preferences.putInt(("ang"+String(s)).c_str(), targetAngle[s]);
      preferences.putInt(("spd"+String(s)).c_str(), servoSpeed[s]);
      
      request->send(200, "text/plain", "OK");
    }
  });

  // Switch para sa Manual o Auto Sweep
  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("val")) {
      autoMode = (request->getParam("val")->value().toInt() == 1);
      preferences.putBool("autoMode", autoMode);
      request->send(200, "text/plain", "OK");
    }
  });

  // Reset Button (Burahin lahat at ibalik sa Default OFF)
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    preferences.clear(); // Burahin ang memory
    autoMode = false;
    for(int i=0; i<16; i++) {
      servoState[i] = false;
      targetAngle[i] = 90;
      servoSpeed[i] = 10;
      currentAngle[i] = 90;
      pwm.setPWM(i, 0, 4096); // Turn off logic
    }
    request->send(200, "text/plain", "RESET_OK");
  });

  server.begin();
  Serial.println("Async HTTP server started");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Hardware Servo Movement Logic
  for(int i=0; i<16; i++) {
    if(!servoState[i]) {
      pwm.setPWM(i, 0, 4096); // Disable channel if OFF
      continue;
    }

    int delayInterval = map(servoSpeed[i], 1, 10, 30, 0); 
    
    // ======== AUTOMATIC MODE (Min to Max Sweeping) ========
    if(autoMode) {
      if(delayInterval == 0) delayInterval = 5; // Pinakamabilis na sweep delay
      
      if(currentMillis - lastUpdate[i] >= delayInterval) {
        lastUpdate[i] = currentMillis;
        
        currentAngle[i] += sweepDir[i];
        
        if(currentAngle[i] >= 180) {
          currentAngle[i] = 180;
          sweepDir[i] = -1; // Balik pababa
        } else if(currentAngle[i] <= 0) {
          currentAngle[i] = 0;
          sweepDir[i] = 1;  // Balik pataas
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