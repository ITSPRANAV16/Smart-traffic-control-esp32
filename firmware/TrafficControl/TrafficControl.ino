/* ==========================================================================
   PROJECT: SMART TRAFFIC CONTROL SYSTEM - ADVANCED EMBEDDED SCADA
   DESIGN BY: TEAM ALPHA
   HARDWARE: ESP32 DevKit • 4-Way Traffic Intersection • Direct Local Wi-Fi
   AUTHOR: Team Alpha • B.Tech ADEC Lab Mini-Project
   ========================================================================== */

#include <WiFi.h>
#include <WebServer.h>
#include "web_assets.h"

// =========================================================================
// 1. HARDWARE GPIO PIN CONFIGURATION (ACTIVE-HIGH LOGIC)
// =========================================================================
// Road 1: North | Road 2: East | Road 3: South | Road 4: West
const int redPins[4]    = {18, 22, 26, 4};
const int yellowPins[4] = {19, 23, 27, 5};
const int greenPins[4]  = {21, 2,  32, 25};

// Wi-Fi SoftAP Configuration
const char* AP_SSID     = "TrafficLight-ESP32";
const char* AP_PASSWORD = "Traffic123";
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Web Server on Port 80
WebServer server(80);

// =========================================================================
// 2. STATE MACHINE DEFINITIONS
// =========================================================================
enum OperatingMode {
  MODE_AUTO,       // Automatic sequence cycle using configured timings
  MODE_MANUAL,     // Admin selects road, stays green until another road is chosen
  MODE_ALL_RED     // Emergency all-red hold for pedestrian or emergency vehicles
};

enum SignalPhase {
  PHASE_STARTUP_ALL_RED,    // Initial boot state (1000ms)
  PHASE_GREEN,              // Active road flows green, others red
  PHASE_YELLOW,             // Active road clears yellow, others red
  PHASE_ALL_RED_CLEARANCE   // Inter-road clearance, all 4 roads red
};

// =========================================================================
// 3. CONTROLLER TIMING & RUNTIME STATE
// =========================================================================
OperatingMode currentMode = MODE_AUTO;
SignalPhase currentPhase  = PHASE_STARTUP_ALL_RED;

int activeRoadIndex  = 0;  // 0 = Road 1, 1 = Road 2, 2 = Road 3, 3 = Road 4
int pendingRoadIndex = -1; // -1 = None, 0..3 = Target road waiting for clearance

// Timing settings in milliseconds (Admin configurable via Web)
unsigned long greenDurationMs     = 10000; // 10 seconds default
unsigned long yellowDurationMs    = 2000;  // 2 seconds default
unsigned long clearanceDurationMs = 1000;  // 1 second default

// Non-blocking timer tracking variables
unsigned long phaseStartTimeMs    = 0;
unsigned long currentPhaseDurationMs = 1000;

// Hardware output memory (active HIGH: 1 = ON, 0 = OFF)
int redState[4]    = {1, 1, 1, 1};
int yellowState[4] = {0, 0, 0, 0};
int greenState[4]  = {0, 0, 0, 0};

// Sequence order in Auto mode (0 = Road 1, 1 = Road 2, 2 = Road 3, 3 = Road 4)
int sequenceOrder[4] = {0, 1, 2, 3};
int sequenceStepIndex = 0;

// =========================================================================
// 4. HARDWARE FAIL-SAFE & OUTPUT DRIVER
// =========================================================================
// Emergency fail-safe: All roads RED
void forceAllRed() {
  for (int i = 0; i < 4; i++) {
    redState[i]    = 1;
    yellowState[i] = 0;
    greenState[i]  = 0;
    digitalWrite(redPins[i], HIGH);
    digitalWrite(yellowPins[i], LOW);
    digitalWrite(greenPins[i], LOW);
  }
}

// Hardware interlocking safety verification
// Guarantees physically that NO TWO ROADS can ever show Green or Yellow simultaneously
void applyHardwareOutputs() {
  int totalGreens = 0;
  int totalYellows = 0;

  for (int i = 0; i < 4; i++) {
    if (greenState[i] == 1) totalGreens++;
    if (yellowState[i] == 1) totalYellows++;
  }

  // HARDWARE SAFETY INTERLOCK CHECK:
  // If logic anomaly attempts to light more than 1 Green/Yellow, immediately abort to ALL RED!
  if (totalGreens > 1 || totalYellows > 1 || (totalGreens >= 1 && totalYellows >= 1)) {
    Serial.println("[CRITICAL SAFETY INTERLOCK] Conflicting signals detected! Tripping to ALL-RED.");
    forceAllRed();
    return;
  }

  // Apply verified signals to physical GPIO pins
  for (int i = 0; i < 4; i++) {
    digitalWrite(redPins[i],    redState[i] ? HIGH : LOW);
    digitalWrite(yellowPins[i], yellowState[i] ? HIGH : LOW);
    digitalWrite(greenPins[i],  greenState[i] ? HIGH : LOW);
  }
}

// Transition state machine to a new phase
void setPhase(SignalPhase newPhase, unsigned long durationMs) {
  currentPhase = newPhase;
  phaseStartTimeMs = millis();
  currentPhaseDurationMs = durationMs;

  switch (currentPhase) {
    case PHASE_STARTUP_ALL_RED:
    case PHASE_ALL_RED_CLEARANCE:
      for (int i = 0; i < 4; i++) {
        redState[i]    = 1;
        yellowState[i] = 0;
        greenState[i]  = 0;
      }
      break;

    case PHASE_GREEN:
      for (int i = 0; i < 4; i++) {
        if (i == activeRoadIndex) {
          redState[i]    = 0;
          yellowState[i] = 0;
          greenState[i]  = 1;
        } else {
          redState[i]    = 1;
          yellowState[i] = 0;
          greenState[i]  = 0;
        }
      }
      break;

    case PHASE_YELLOW:
      for (int i = 0; i < 4; i++) {
        if (i == activeRoadIndex) {
          redState[i]    = 0;
          yellowState[i] = 1;
          greenState[i]  = 0;
        } else {
          redState[i]    = 1;
          yellowState[i] = 0;
          greenState[i]  = 0;
        }
      }
      break;
  }

  applyHardwareOutputs();

  Serial.printf("[TRAFFIC SM] Active Road: %d | Phase: %s | Duration: %lu ms\n",
                activeRoadIndex + 1,
                (currentPhase == PHASE_GREEN) ? "GREEN" :
                (currentPhase == PHASE_YELLOW) ? "YELLOW" :
                (currentPhase == PHASE_ALL_RED_CLEARANCE) ? "ALL_RED_CLEARANCE" : "STARTUP",
                durationMs);
}

// =========================================================================
// 5. NON-BLOCKING TRAFFIC CONTROLLER LOOP (millis() BASED)
// =========================================================================
void updateTrafficStateMachine() {
  // If Emergency All-Red Hold mode is active, keep all roads red indefinitely
  if (currentMode == MODE_ALL_RED) {
    forceAllRed();
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - phaseStartTimeMs;

  switch (currentPhase) {
    case PHASE_STARTUP_ALL_RED:
      if (elapsed >= currentPhaseDurationMs) {
        // Startup delay complete -> Enter first green phase
        activeRoadIndex = sequenceOrder[0];
        sequenceStepIndex = 0;
        setPhase(PHASE_GREEN, greenDurationMs);
      }
      break;

    case PHASE_GREEN:
      if (currentMode == MODE_AUTO) {
        // In Auto mode, countdown expires based on greenDurationMs
        if (elapsed >= currentPhaseDurationMs) {
          // Green expired -> Transition to Yellow on current road
          setPhase(PHASE_YELLOW, yellowDurationMs);
        }
      } else if (currentMode == MODE_MANUAL) {
        // In Manual mode, stay green indefinitely until admin selects another road!
        if (pendingRoadIndex >= 0 && pendingRoadIndex != activeRoadIndex) {
          // Admin requested another road -> Transition current road to yellow
          setPhase(PHASE_YELLOW, yellowDurationMs);
        }
      }
      break;

    case PHASE_YELLOW:
      if (elapsed >= currentPhaseDurationMs) {
        // Yellow expired -> Enter safety All-Red Clearance
        setPhase(PHASE_ALL_RED_CLEARANCE, clearanceDurationMs);
      }
      break;

    case PHASE_ALL_RED_CLEARANCE:
      if (elapsed >= currentPhaseDurationMs) {
        // Clearance expired -> Select next road to receive green
        if (pendingRoadIndex >= 0) {
          activeRoadIndex = pendingRoadIndex;
          pendingRoadIndex = -1; // Request served cleanly!
          currentMode = MODE_MANUAL;
        } else if (currentMode == MODE_AUTO) {
          // Advance sequence in Auto mode (Road 1 -> 2 -> 3 -> 4 -> 1)
          sequenceStepIndex = (sequenceStepIndex + 1) % 4;
          activeRoadIndex = sequenceOrder[sequenceStepIndex];
        }
        setPhase(PHASE_GREEN, greenDurationMs);
      }
      break;
  }
}

// =========================================================================
// 6. REST API REQUEST HANDLERS & WEB SERVER
// =========================================================================

// Add standard CORS and anti-cache headers
void sendCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

// Serve Embedded Web Dashboard
void handleRoot() {
  sendCorsHeaders();
  server.send_P(200, "text/html", INDEX_HTML);
}

// Handle GET /api/status - Live hardware state
void handleGetStatus() {
  sendCorsHeaders();

  unsigned long elapsed = millis() - phaseStartTimeMs;
  unsigned long remaining = 0;
  if (currentMode == MODE_ALL_RED) {
    remaining = 0;
  } else if (currentMode == MODE_MANUAL && currentPhase == PHASE_GREEN && pendingRoadIndex < 0) {
    // In manual mode holding green, countdown indicates continuous hold
    remaining = greenDurationMs;
  } else {
    remaining = (elapsed < currentPhaseDurationMs) ? (currentPhaseDurationMs - elapsed) : 0;
  }

  const char* modeStr = (currentMode == MODE_AUTO) ? "auto" :
                        (currentMode == MODE_MANUAL) ? "manual" : "all_red";

  const char* phaseStr = (currentPhase == PHASE_GREEN) ? "GREEN" :
                         (currentPhase == PHASE_YELLOW) ? "YELLOW" :
                         (currentPhase == PHASE_ALL_RED_CLEARANCE) ? "ALL_RED_CLEARANCE" : "STARTUP";

  // Build JSON response efficiently
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"wifi_clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"mode\":\"" + String(modeStr) + "\",";
  json += "\"active_road\":" + String(activeRoadIndex + 1) + ",";
  json += "\"activeRoad\":" + String(activeRoadIndex) + ",";
  json += "\"phase\":\"" + String(phaseStr) + "\",";
  json += "\"remaining_ms\":" + String(remaining) + ",";
  json += "\"remainingSeconds\":" + String((remaining + 999) / 1000) + ",";
  json += "\"total_ms\":" + String(currentPhaseDurationMs) + ",";
  json += "\"totalSeconds\":" + String((currentPhaseDurationMs + 999) / 1000) + ",";
  json += "\"pending_road\":" + String(pendingRoadIndex >= 0 ? pendingRoadIndex + 1 : 0) + ",";
  json += "\"uptime_sec\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  
  // Settings
  json += "\"settings\":{";
  json += "\"greenTime\":" + String(greenDurationMs / 1000) + ",";
  json += "\"yellowTime\":" + String(yellowDurationMs / 1000) + ",";
  json += "\"clearanceTime\":" + String(clearanceDurationMs / 1000);
  json += "},";

  // All 12 LED output states
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

// Handle POST /api/mode - Switch between auto, manual, and all_red
void handlePostMode() {
  sendCorsHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.printf("[API] POST /api/mode: %s\n", body.c_str());

  if (body.indexOf("\"mode\":\"auto\"") >= 0 || body.indexOf("\"auto\"") >= 0) {
    currentMode = MODE_AUTO;
    pendingRoadIndex = -1;
    if (currentPhase == PHASE_GREEN) {
      // Reset timer for auto mode
      phaseStartTimeMs = millis();
      currentPhaseDurationMs = greenDurationMs;
    }
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"auto\"}");
  } else if (body.indexOf("\"mode\":\"manual\"") >= 0 || body.indexOf("\"manual\"") >= 0) {
    currentMode = MODE_MANUAL;
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"manual\"}");
  } else if (body.indexOf("\"mode\":\"all_red\"") >= 0 || body.indexOf("\"all_red\"") >= 0) {
    currentMode = MODE_ALL_RED;
    pendingRoadIndex = -1;
    forceAllRed();
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"all_red\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid mode\"}");
  }
}

// Handle POST /api/road - Manual selection of Road 1 to 4
void handlePostRoad() {
  sendCorsHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.printf("[API] POST /api/road: %s\n", body.c_str());

  int targetRoad = 0;
  for (int r = 1; r <= 4; r++) {
    if (body.indexOf("\"road\":" + String(r)) >= 0 || body.indexOf(String(r)) >= 0) {
      targetRoad = r;
      break;
    }
  }

  if (targetRoad < 1 || targetRoad > 4) {
    server.send(400, "application/json", "{\"error\":\"Road must be 1 to 4\"}");
    return;
  }

  int targetIndex = targetRoad - 1;

  // Set to Manual mode unconditionally when manual road is selected
  currentMode = MODE_MANUAL;

  // If target road is already active and green, keep holding
  if (targetIndex == activeRoadIndex && currentPhase == PHASE_GREEN) {
    pendingRoadIndex = -1;
    server.send(200, "application/json", "{\"status\":\"already_active\",\"road\":" + String(targetRoad) + "}");
    return;
  }

  // Queue requested road safely
  pendingRoadIndex = targetIndex;

  // If currently in GREEN phase, trigger immediate transition to YELLOW -> CLEARANCE
  if (currentPhase == PHASE_GREEN) {
    setPhase(PHASE_YELLOW, yellowDurationMs);
  }

  server.send(200, "application/json", "{\"status\":\"queued\",\"target_road\":" + String(targetRoad) + "}");
}

// Handle POST /api/settings - Update Green, Yellow, Clearance timings
void handlePostSettings() {
  sendCorsHeaders();

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.printf("[API] POST /api/settings: %s\n", body.c_str());

  // Parse green
  int gIdx = body.indexOf("\"green\":");
  if (gIdx >= 0) {
    int gVal = body.substring(gIdx + 8).toInt();
    if (gVal >= 5 && gVal <= 60) greenDurationMs = (unsigned long)gVal * 1000;
  }

  // Parse yellow
  int yIdx = body.indexOf("\"yellow\":");
  if (yIdx >= 0) {
    int yVal = body.substring(yIdx + 9).toInt();
    if (yVal >= 1 && yVal <= 10) yellowDurationMs = (unsigned long)yVal * 1000;
  }

  // Parse clearance
  int cIdx = body.indexOf("\"clearance\":");
  if (cIdx >= 0) {
    int cVal = body.substring(cIdx + 12).toInt();
    if (cVal >= 1 && cVal <= 5) clearanceDurationMs = (unsigned long)cVal * 1000;
  }

  server.send(200, "application/json", "{\"status\":\"updated\"}");
}

// Handle OPTIONS preflight requests for CORS
void handleOptions() {
  sendCorsHeaders();
  server.send(204);
}

// 404 Handler
void handleNotFound() {
  sendCorsHeaders();
  server.send(404, "text/plain", "Resource not found on ESP32");
}

// =========================================================================
// 7. SETUP & INITIALIZATION
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n========================================================");
  Serial.println("   SMART TRAFFIC CONTROL SYSTEM - SCADA v3.0            ");
  Serial.println("             DESIGN BY TEAM ALPHA                       ");
  Serial.println("========================================================");

  // Initialize all 12 GPIO pins as OUTPUT
  for (int i = 0; i < 4; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(yellowPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);
  }

  // Startup safe state: All roads RED immediately
  forceAllRed();
  Serial.println("[HARDWARE] All 12 GPIO pins initialized. Set to ALL-RED.");

  // Configure Wi-Fi SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.printf("[WI-FI] SoftAP started successfully!\n");
    Serial.printf("        SSID:     %s\n", AP_SSID);
    Serial.printf("        Password: %s\n", AP_PASSWORD);
    Serial.printf("        IP:       http://%s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[WI-FI ERROR] Failed to start SoftAP!");
  }

  // Attach Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/mode", HTTP_POST, handlePostMode);
  server.on("/api/road", HTTP_POST, handlePostRoad);
  server.on("/api/settings", HTTP_POST, handlePostSettings);

  // CORS Preflight Handlers
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/mode", HTTP_OPTIONS, handleOptions);
  server.on("/api/road", HTTP_OPTIONS, handleOptions);
  server.on("/api/settings", HTTP_OPTIONS, handleOptions);

  server.onNotFound(handleNotFound);

  // Start Web Server
  server.begin();
  Serial.println("[SERVER] HTTP WebServer listening on port 80.");

  // Initialize Traffic State Machine with Startup All-Red
  setPhase(PHASE_STARTUP_ALL_RED, 1000);
  Serial.println("[SYSTEM] Ready! Connect phone to Wi-Fi and open http://192.168.4.1\n");
}

// =========================================================================
// 8. MAIN ARDUINO LOOP (100% NON-BLOCKING)
// =========================================================================
void loop() {
  // Handle incoming HTTP client requests
  server.handleClient();

  // Run traffic safety state machine
  updateTrafficStateMachine();
}
