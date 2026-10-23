/*
  PROJECT: Smart Traffic Light App Controller
  BOARD: ESP32 DevKit
  FEATURES:
    - ESP32 creates its own Wi-Fi network (no internet required)
    - Mobile web dashboard at http://192.168.4.1
    - Automatic, Manual, and All-Red modes
    - Four road controls and live countdown/status
    - Safe Green -> Yellow -> All-Red -> next Green sequence

  IMPORTANT:
    - This is a low-voltage educational model, NOT a real road traffic controller.
    - Use suitable current-limiting resistors/driver circuits for LEDs.
    - GPIO pins are based on the user's existing wiring.
*/

#include <WiFi.h>
#include <WebServer.h>

// ---------------- GPIO PIN CONFIGURATION ----------------
// Road 1 = North, Road 2 = East, Road 3 = South, Road 4 = West
// These are the existing pins supplied for this project.
const int redPins[4]    = {18, 22, 26, 4};
const int yellowPins[4] = {19, 23, 27, 5};
const int greenPins[4]  = {21, 2, 32, 25};

// ---------------- WI-FI ACCESS POINT ----------------
const char* AP_SSID = "TrafficLight-ESP32";
const char* AP_PASSWORD = "Traffic123";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ---------------- TRAFFIC CONTROLLER STATE ----------------
enum OperatingMode {
  MODE_AUTO,
  MODE_MANUAL,
  MODE_ALL_RED
};

enum SignalPhase {
  PHASE_STARTUP_ALL_RED,
  PHASE_GREEN,
  PHASE_YELLOW,
  PHASE_ALL_RED_CLEARANCE
};

OperatingMode currentMode = MODE_AUTO;
SignalPhase currentPhase = PHASE_STARTUP_ALL_RED;

int activeRoadIndex = 0;       // 0..3 means Road 1..4
int pendingRoadIndex = -1;     // -1 means no pending road

unsigned long greenDurationMs = 10000;
unsigned long yellowDurationMs = 2000;
unsigned long clearanceDurationMs = 1000;
unsigned long phaseStartTimeMs = 0;
unsigned long currentPhaseDurationMs = 1000;

int redState[4]    = {1, 1, 1, 1};
int yellowState[4] = {0, 0, 0, 0};
int greenState[4]  = {0, 0, 0, 0};

int sequenceOrder[4] = {0, 1, 2, 3};
int sequenceStepIndex = 0;

// ---------------- EMBEDDED WEB PAGE ----------------
// Keeping the page inside this .ino removes the missing web_assets.h error.
const char INDEX_HTML[] PROGMEM = R"TRAFFICPAGE(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Traffic Control</title>
<style>
:root{color-scheme:dark;--bg:#0b1020;--panel:#141c30;--line:#293650;--text:#edf2ff;--muted:#9eacc7;--gold:#f4c45e}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(135deg,#0b1020,#111a2e);font-family:Arial,Helvetica,sans-serif;color:var(--text)}
main{max-width:1000px;margin:auto;padding:22px}header{padding:10px 0 18px}h1{margin:0 0 8px;font-size:clamp(25px,5vw,38px)}p{color:var(--muted);line-height:1.5}
.badge{display:inline-block;border:1px solid var(--line);border-radius:20px;padding:7px 12px;color:var(--gold);font-size:13px}
.panel{background:rgba(20,28,48,.95);border:1px solid var(--line);border-radius:16px;padding:18px;margin:14px 0}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}.btn{border:1px solid var(--line);background:#202c45;color:var(--text);border-radius:10px;padding:12px 16px;font-weight:700;cursor:pointer}
.btn:active{transform:scale(.98)}.btn.active{background:var(--gold);color:#172033;border-color:var(--gold)}.btn.danger{background:#51232b;border-color:#8d3541}.btn:disabled{opacity:.5}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px}
.road{background:#10182a;border:1px solid var(--line);border-radius:14px;padding:16px}
.road h3{margin:0 0 12px}.lights{display:flex;gap:8px;margin-bottom:14px}.lamp{width:27px;height:27px;border-radius:50%;background:#30394b;border:2px solid #46516a}
.lamp.on.red{background:#ff4d5b;box-shadow:0 0 14px #ff4d5b}.lamp.on.yellow{background:#ffd45a;box-shadow:0 0 14px #ffd45a}.lamp.on.green{background:#3de18b;box-shadow:0 0 14px #3de18b}
.status{font-size:14px;color:var(--muted);line-height:1.8}.status strong{color:var(--text)}.count{font-size:32px;font-weight:800;color:var(--gold)}
label{display:block;color:var(--muted);font-size:13px;margin-bottom:7px}input{width:100%;background:#0b1020;color:var(--text);border:1px solid var(--line);border-radius:8px;padding:10px}
.field{width:120px}small{color:var(--muted)}#message{min-height:20px;color:var(--gold);margin-top:10px}
</style>
</head>
<body>
<main>
<header>
 <span class="badge">ESP32 LOCAL WI-FI • OFFLINE DASHBOARD</span>
 <h1>Smart Traffic Control</h1>
 <p>Control the four-way traffic-light model from your phone. Connect to the ESP32 Wi-Fi network, then open <b>http://192.168.4.1</b>.</p>
</header>
<section class="panel">
 <div class="row" style="justify-content:space-between">
  <div><div class="status">SYSTEM STATUS</div><h2 id="online" style="margin:5px 0">Connecting…</h2></div>
  <div><div class="status">ACTIVE PHASE COUNTDOWN</div><div class="count"><span id="countdown">--</span><small> sec</small></div></div>
 </div>
 <div class="status">Mode: <strong id="modeText">--</strong> &nbsp; | &nbsp; Phase: <strong id="phaseText">--</strong> &nbsp; | &nbsp; Active road: <strong id="activeRoad">--</strong></div>
 <div class="status">Connected devices: <strong id="clients">0</strong> &nbsp; | &nbsp; ESP32 IP: <strong id="ip">192.168.4.1</strong></div>
</section>
<section class="panel">
 <h2>Operating mode</h2>
 <div class="row">
  <button class="btn" onclick="setMode('auto')">Automatic</button>
  <button class="btn" onclick="setMode('manual')">Manual</button>
  <button class="btn danger" onclick="setMode('all_red')">Emergency: All Red</button>
 </div>
 <p>Automatic cycles Road 1 → Road 2 → Road 3 → Road 4. Manual mode keeps the selected road green until another road is selected. All-Red holds every road red.</p>
</section>
<section class="panel">
 <h2>Road controls and live lights</h2>
 <div class="grid" id="roads"></div>
</section>
<section class="panel">
 <h2>Timing settings</h2>
 <div class="row">
  <div class="field"><label for="green">Green (5–60 sec)</label><input id="green" type="number" min="5" max="60" value="10"></div>
  <div class="field"><label for="yellow">Yellow (1–10 sec)</label><input id="yellow" type="number" min="1" max="10" value="2"></div>
  <div class="field"><label for="clearance">All-red (1–5 sec)</label><input id="clearance" type="number" min="1" max="5" value="1"></div>
  <button class="btn active" onclick="saveSettings()">Save timings</button>
 </div>
 <div id="message"></div>
</section>
<footer><small>Educational demonstration only. Do not connect this model to real traffic infrastructure.</small></footer>
</main>
<script>
const roadNames=['Road 1 — North','Road 2 — East','Road 3 — South','Road 4 — West'];
const roadsEl=document.getElementById('roads');
roadsEl.innerHTML=roadNames.map((name,i)=>`<article class="road">
 <h3>${name}</h3><div class="lights">
 <span id="r${i}" class="lamp red"></span><span id="y${i}" class="lamp yellow"></span><span id="g${i}" class="lamp green"></span></div>
 <div class="status">Red: <b id="rs${i}">--</b> &nbsp; Yellow: <b id="ys${i}">--</b> &nbsp; Green: <b id="gs${i}">--</b></div>
 <div style="margin-top:13px"><button class="btn" onclick="selectRoad(${i+1})">Make this road green</button></div>
 </article>`).join('');
async function request(path,body){
 const options=body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)};
 const response=await fetch(path,options); const data=await response.json();
 if(!response.ok)throw new Error(data.error||'Request failed'); return data;
}
function msg(t){document.getElementById('message').textContent=t;setTimeout(()=>{document.getElementById('message').textContent=''},3500)}
async function setMode(mode){try{await request('/api/mode',{mode});msg('Mode changed to '+mode.replace('_',' '));await refresh()}catch(e){msg(e.message)}}
async function selectRoad(road){try{const d=await request('/api/road',{road});msg(d.status==='already_active'?'That road is already active.':'Road '+road+' queued safely.');await refresh()}catch(e){msg(e.message)}}
async function saveSettings(){
 const green=Number(document.getElementById('green').value),yellow=Number(document.getElementById('yellow').value),clearance=Number(document.getElementById('clearance').value);
 if(!Number.isInteger(green)||green<5||green>60||!Number.isInteger(yellow)||yellow<1||yellow>10||!Number.isInteger(clearance)||clearance<1||clearance>5){msg('Enter valid timing values in the shown ranges.');return}
 try{await request('/api/settings',{green,yellow,clearance});msg('Timing settings saved.');await refresh()}catch(e){msg(e.message)}
}
async function refresh(){
 try{
  const d=await request('/api/status');
  document.getElementById('online').textContent='Online';
  document.getElementById('modeText').textContent=d.mode.replace('_',' ').toUpperCase();
  document.getElementById('phaseText').textContent=d.phase.replaceAll('_',' ');
  document.getElementById('activeRoad').textContent='Road '+d.active_road;
  document.getElementById('countdown').textContent=d.mode==='all_red'?'—':d.remainingSeconds;
  document.getElementById('clients').textContent=d.wifi_clients;
  document.getElementById('ip').textContent=d.ip;
  d.roads.forEach((r,i)=>{
   [['r',r.r],['y',r.y],['g',r.g]].forEach(([c,on])=>document.getElementById(c+i).classList.toggle('on',!!on));
   document.getElementById('rs'+i).textContent=r.r?'ON':'OFF';
   document.getElementById('ys'+i).textContent=r.y?'ON':'OFF';
   document.getElementById('gs'+i).textContent=r.g?'ON':'OFF';
  });
 }catch(e){document.getElementById('online').textContent='Not connected — join TrafficLight-ESP32 Wi-Fi';}
}
refresh();setInterval(refresh,500);
</script>
</body>
</html>
)TRAFFICPAGE";

// ---------------- COMMON HELPERS ----------------
void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

void forceAllRed() {
  for (int i = 0; i < 4; i++) {
    redState[i] = 1;
    yellowState[i] = 0;
    greenState[i] = 0;
    digitalWrite(redPins[i], HIGH);
    digitalWrite(yellowPins[i], LOW);
    digitalWrite(greenPins[i], LOW);
  }
}

void applyHardwareOutputs() {
  int totalGreens = 0;
  int totalYellows = 0;

  for (int i = 0; i < 4; i++) {
    if (greenState[i]) totalGreens++;
    if (yellowState[i]) totalYellows++;
  }

  // Fail safe if conflicting green/yellow outputs are requested.
  if (totalGreens > 1 || totalYellows > 1 ||
      (totalGreens > 0 && totalYellows > 0)) {
    Serial.println("[SAFETY] Conflicting outputs. Forcing all red.");
    forceAllRed();
    return;
  }

  for (int i = 0; i < 4; i++) {
    digitalWrite(redPins[i], redState[i] ? HIGH : LOW);
    digitalWrite(yellowPins[i], yellowState[i] ? HIGH : LOW);
    digitalWrite(greenPins[i], greenState[i] ? HIGH : LOW);
  }
}

void setPhase(SignalPhase newPhase, unsigned long durationMs) {
  currentPhase = newPhase;
  phaseStartTimeMs = millis();
  currentPhaseDurationMs = durationMs;

  for (int i = 0; i < 4; i++) {
    redState[i] = 1;
    yellowState[i] = 0;
    greenState[i] = 0;
  }

  if (newPhase == PHASE_GREEN) {
    redState[activeRoadIndex] = 0;
    greenState[activeRoadIndex] = 1;
  } else if (newPhase == PHASE_YELLOW) {
    redState[activeRoadIndex] = 0;
    yellowState[activeRoadIndex] = 1;
  }

  applyHardwareOutputs();
}

void updateTrafficStateMachine() {
  if (currentMode == MODE_ALL_RED) {
    forceAllRed();
    return;
  }

  unsigned long elapsed = millis() - phaseStartTimeMs;

  switch (currentPhase) {
    case PHASE_STARTUP_ALL_RED:
      if (elapsed >= currentPhaseDurationMs) {
        activeRoadIndex = sequenceOrder[0];
        sequenceStepIndex = 0;
        setPhase(PHASE_GREEN, greenDurationMs);
      }
      break;

    case PHASE_GREEN:
      if (currentMode == MODE_AUTO) {
        if (elapsed >= currentPhaseDurationMs) {
          setPhase(PHASE_YELLOW, yellowDurationMs);
        }
      } else if (currentMode == MODE_MANUAL) {
        if (pendingRoadIndex >= 0 && pendingRoadIndex != activeRoadIndex) {
          setPhase(PHASE_YELLOW, yellowDurationMs);
        }
      }
      break;

    case PHASE_YELLOW:
      if (elapsed >= currentPhaseDurationMs) {
        setPhase(PHASE_ALL_RED_CLEARANCE, clearanceDurationMs);
      }
      break;

    case PHASE_ALL_RED_CLEARANCE:
      if (elapsed >= currentPhaseDurationMs) {
        if (pendingRoadIndex >= 0) {
          activeRoadIndex = pendingRoadIndex;
          pendingRoadIndex = -1;
          currentMode = MODE_MANUAL;
        } else if (currentMode == MODE_AUTO) {
          sequenceStepIndex = (sequenceStepIndex + 1) % 4;
          activeRoadIndex = sequenceOrder[sequenceStepIndex];
        }
        setPhase(PHASE_GREEN, greenDurationMs);
      }
      break;
  }
}

// ---------------- WEB ROUTES ----------------
void handleRoot() {
  sendCorsHeaders();
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleGetStatus() {
  sendCorsHeaders();

  unsigned long elapsed = millis() - phaseStartTimeMs;
  unsigned long remaining = 0;

  if (currentMode == MODE_ALL_RED) {
    remaining = 0;
  } else if (currentMode == MODE_MANUAL &&
             currentPhase == PHASE_GREEN && pendingRoadIndex < 0) {
    remaining = 0; // Manual green is held until another road is selected.
  } else {
    remaining = (elapsed < currentPhaseDurationMs)
                  ? currentPhaseDurationMs - elapsed : 0;
  }

  const char* modeStr = currentMode == MODE_AUTO ? "auto" :
                        currentMode == MODE_MANUAL ? "manual" : "all_red";
  const char* phaseStr = currentPhase == PHASE_GREEN ? "GREEN" :
                         currentPhase == PHASE_YELLOW ? "YELLOW" :
                         currentPhase == PHASE_ALL_RED_CLEARANCE ? "ALL_RED_CLEARANCE" :
                         "STARTUP_ALL_RED";

  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"wifi_clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"mode\":\"" + String(modeStr) + "\",";
  json += "\"active_road\":" + String(activeRoadIndex + 1) + ",";
  json += "\"phase\":\"" + String(phaseStr) + "\",";
  json += "\"remaining_ms\":" + String(remaining) + ",";
  json += "\"remainingSeconds\":" + String((remaining + 999) / 1000) + ",";
  json += "\"total_ms\":" + String(currentPhaseDurationMs) + ",";
  json += "\"pending_road\":" + String(pendingRoadIndex >= 0 ? pendingRoadIndex + 1 : 0) + ",";
  json += "\"settings\":{\"greenTime\":" + String(greenDurationMs / 1000) + ",";
  json += "\"yellowTime\":" + String(yellowDurationMs / 1000) + ",";
  json += "\"clearanceTime\":" + String(clearanceDurationMs / 1000) + "},";
  json += "\"roads\":[";
  for (int i = 0; i < 4; i++) {
    json += "{\"road\":" + String(i + 1) + ",";
    json += "\"r\":" + String(redState[i]) + ",";
    json += "\"y\":" + String(yellowState[i]) + ",";
    json += "\"g\":" + String(greenState[i]) + "}";
    if (i < 3) json += ",";
  }
  json += "]}";

  server.send(200, "application/json", json);
}

void handlePostMode() {
  sendCorsHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
    return;
  }

  String body = server.arg("plain");

  if (body.indexOf("\"mode\":\"auto\"") >= 0) {
    currentMode = MODE_AUTO;
    pendingRoadIndex = -1;
    if (currentPhase == PHASE_GREEN) {
      phaseStartTimeMs = millis();
      currentPhaseDurationMs = greenDurationMs;
    } else if (currentPhase == PHASE_STARTUP_ALL_RED) {
      // Startup will continue normally.
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"auto\"}");
  } else if (body.indexOf("\"mode\":\"manual\"") >= 0) {
    currentMode = MODE_MANUAL;
    pendingRoadIndex = -1;
    if (currentPhase == PHASE_ALL_RED_CLEARANCE) {
      // Current clearance finishes before green resumes.
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"manual\"}");
  } else if (body.indexOf("\"mode\":\"all_red\"") >= 0) {
    currentMode = MODE_ALL_RED;
    pendingRoadIndex = -1;
    forceAllRed();
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"all_red\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid mode\"}");
  }
}

void handlePostRoad() {
  sendCorsHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
    return;
  }

  String body = server.arg("plain");
  int key = body.indexOf("\"road\"");
  if (key < 0) {
    server.send(400, "application/json", "{\"error\":\"Road must be 1 to 4\"}");
    return;
  }

  int colon = body.indexOf(':', key);
  if (colon < 0) {
    server.send(400, "application/json", "{\"error\":\"Invalid road value\"}");
    return;
  }

  int targetRoad = body.substring(colon + 1).toInt();
  if (targetRoad < 1 || targetRoad > 4) {
    server.send(400, "application/json", "{\"error\":\"Road must be 1 to 4\"}");
    return;
  }

  int targetIndex = targetRoad - 1;
  currentMode = MODE_MANUAL;

  if (targetIndex == activeRoadIndex && currentPhase == PHASE_GREEN) {
    pendingRoadIndex = -1;
    server.send(200, "application/json",
                "{\"status\":\"already_active\",\"road\":" + String(targetRoad) + "}");
    return;
  }

  pendingRoadIndex = targetIndex;

  if (currentMode != MODE_ALL_RED && currentPhase == PHASE_GREEN) {
    setPhase(PHASE_YELLOW, yellowDurationMs);
  } else if (currentPhase == PHASE_STARTUP_ALL_RED) {
    // Requested road will be selected after startup all-red completes.
  }

  server.send(200, "application/json",
              "{\"status\":\"queued\",\"target_road\":" + String(targetRoad) + "}");
}


void handlePostSettings() {
  sendCorsHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
                "{\"error\":\"Missing JSON body\"}");
    return;
  }

  String body = server.arg("plain");
  int idx;
  int colon;
  int value;

  // Green duration: 5 to 60 seconds
  idx = body.indexOf("\"green\"");
  if (idx >= 0) {
    colon = body.indexOf(':', idx);

    if (colon >= 0) {
      value = body.substring(colon + 1).toInt();

      if (value >= 5 && value <= 60) {
        greenDurationMs = (unsigned long)value * 1000UL;
      }
    }
  }

  // Yellow duration: 1 to 10 seconds
  idx = body.indexOf("\"yellow\"");
  if (idx >= 0) {
    colon = body.indexOf(':', idx);

    if (colon >= 0) {
      value = body.substring(colon + 1).toInt();

      if (value >= 1 && value <= 10) {
        yellowDurationMs = (unsigned long)value * 1000UL;
      }
    }
  }

  // All-red clearance duration: 1 to 5 seconds
  idx = body.indexOf("\"clearance\"");
  if (idx >= 0) {
    colon = body.indexOf(':', idx);

    if (colon >= 0) {
      value = body.substring(colon + 1).toInt();

      if (value >= 1 && value <= 5) {
        clearanceDurationMs = (unsigned long)value * 1000UL;
      }
    }
  }

  server.send(200, "application/json",
              "{\"status\":\"updated\"}");
}

// ---------------- ARDUINO SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Configure all signal LED pins.
  for (int i = 0; i < 4; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(yellowPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);
  }

  // Start in a safe all-red state.
  forceAllRed();
  currentPhase = PHASE_STARTUP_ALL_RED;
  phaseStartTimeMs = millis();
  currentPhaseDurationMs = 1000;

  // Create the ESP32's own Wi-Fi network.
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println();
    Serial.println("Smart Traffic Light App Controller");
    Serial.print("Wi-Fi name: ");
    Serial.println(AP_SSID);
    Serial.print("Wi-Fi password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("Open dashboard: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Could not start the ESP32 Wi-Fi access point.");
  }

  // Register dashboard and API routes.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/mode", HTTP_POST, handlePostMode);
  server.on("/api/road", HTTP_POST, handlePostRoad);
  server.on("/api/settings", HTTP_POST, handlePostSettings);

  server.onNotFound([]() {
    sendCorsHeaders();
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("Web server started.");
}

// ---------------- ARDUINO MAIN LOOP ----------------
void loop() {
  server.handleClient();
  updateTrafficStateMachine();
  delay(2);
}
