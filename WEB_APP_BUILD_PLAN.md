# 🚦 Smart Traffic Control System — Full Advanced Web App Build Plan

**Project:** 4-Way Traffic Signal Control Web Application with ESP32 Hardware Integration  
**Course:** B.Tech ADEC Lab Mini-Project (Semester 03)  
**Hardware Platform:** ESP32 DevKit (12 LEDs, Active-HIGH Logic)  
**Connectivity:** Direct Wi-Fi SoftAP (`192.168.4.1`) • 100% Offline • Zero CDN / Cloud  

---

## 📌 Executive Summary

This document specifies the complete engineering build plan for the **Smart Traffic Control Web Application**. The application operates both as a **standalone browser simulator** (for offline demonstration and UI testing) and as a **live hardware control dashboard** hosted directly on the ESP32 microcontroller.

---

## 📐 System Architecture Overview

```
+-----------------------------------------------------------------------------------------+
|                  CLIENT DEVICE (Android Phone / Laptop / Tablet)                        |
|                  Browser: Google Chrome / Edge / Safari                                 |
|                  Network: Direct Wi-Fi "TrafficLight-ESP32"                             |
+-----------------------------------------------------------------------------------------+
                                             │
                                             ▼ HTTP GET / POST (JSON API)
+-----------------------------------------------------------------------------------------+
|                                    WEB APPLICATION                                      |
|                                                                                         |
|  [ MODULE 1: HEADER & TELEMETRY ]                                                       |
|  • Connection Status (ONLINE 192.168.4.1 / OFFLINE) with animated pulse dot            |
|  • Operating Mode Badge (AUTO / MANUAL / ALL-RED)                                       |
|  • Controller Uptime Clock (HH:MM:SS)                                                   |
|                                                                                         |
|  [ MODULE 2: COMMAND RIBBON & COUNTDOWN ENGINE ]                                        |
|  • Mode Toggles: [⚡ Auto Mode]  [🎮 Manual Override]  [🛑 All-Red Hold]                |
|  • Active Phase Pill: "ROAD 1 • GREEN" (Dynamic Glowing Status Dot)                     |
|  • Big Bold Digital Countdown: Real remaining seconds (10s, 09s... 00s)                 |
|                                                                                         |
|  [ MODULE 3: INTERACTIVE 4-WAY INTERSECTION (SVG) ]                                     |
|  • Top-Down Vector Crossing: Asphalt texture, yellow double lines, zebra crossings      |
|  • 4 Traffic Signal Posts: North, East, South, West                                     |
|  • Realistic Radial Glow Shaders for Red, Yellow, Green LED lenses                      |
|  • 360° Orientation Alignment Selector (0°, 90°, 180°, 270°)                            |
|                                                                                         |
|  [ MODULE 4: ROAD CONTROL CHANNELS (ADMIN OVERRIDE) ]                                   |
|  • 4 Physical Road Cards: Road 1 (North), Road 2 (East), Road 3 (South), Road 4 (West)  |
|  • Pinout Tags: R:18, Y:19, G:21 | R:22, Y:23, G:2 | R:26, Y:27, G:32 | R:4, Y:5, G:25  |
|  • Mini 3-Lamp Real-time Signal Box                                                     |
|  • "Select Road" Action Button with "Pending Clearance" highlight                       |
|                                                                                         |
|  [ MODULE 5: SIGNAL TIMING CONFIGURATION MODAL (⚙️) ]                                   |
|  • Green Duration (5s to 60s, Default: 10s)                                             |
|  • Yellow Caution Duration (1s to 10s, Default: 2s)                                     |
|  • All-Red Clearance Interval (1s to 5s, Default: 1s)                                   |
|  • Reset to Defaults & Instant Apply Buttons                                            |
|                                                                                         |
|  [ MODULE 6: LIVE AUDIT LOG & DIAGNOSTICS ]                                             |
|  • Timestamped Event Stream: Log state transitions, admin commands, safety locks        |
|  • Hardware Metrics: ESP32 Free Heap, Wi-Fi Clients, API Latency                        |
+-----------------------------------------------------------------------------------------+
                                             ▲
                                             │ REST JSON API (Poll every 700ms)
                                             ▼
+-----------------------------------------------------------------------------------------+
|                                ESP32 HARDWARE CONTROLLER                                |
|  • SoftAP: SSID: TrafficLight-ESP32 | Pass: Traffic123 | IP: 192.168.4.1                |
|  • WebServer on Port 80 (Serves PROGMEM Web Assets & JSON Endpoints)                    |
|  • Non-Blocking millis() State Machine: STARTUP, GREEN, YELLOW, CLEARANCE               |
|  • Physical Hardware Interlock: Prevents multiple greens across all 4 roads             |
+-----------------------------------------------------------------------------------------+
                                             │
                                             ▼ Active-HIGH Digital Outputs
+-----------------------------------------------------------------------------------------+
|  Road 1 (North): R: GPIO 18  |  Y: GPIO 19  |  G: GPIO 21                               |
|  Road 2 (East):  R: GPIO 22  |  Y: GPIO 23  |  G: GPIO 2                                |
|  Road 3 (South): R: GPIO 26  |  Y: GPIO 27  |  G: GPIO 32                               |
|  Road 4 (West):  R: GPIO 4   |  Y: GPIO 5   |  G: GPIO 25                               |
+-----------------------------------------------------------------------------------------+
```

---

## 🧩 Detailed Module Specifications

### Module 1: Header, Navigation & Telemetry
- **Visual Design:** Dark Navy Glassmorphism (`#070c18`, `#0f172a`, `rgba(18, 28, 49, 0.75)`).
- **Branding:** Project title: `SMART TRAFFIC CONTROL` with modern gradient typography.
- **Connection Indicator:**
  - `ONLINE (192.168.4.1)` with pulsing green dot when connected to ESP32.
  - `OFFLINE` with pulsing red dot and alert banner when disconnected.
- **Operating Mode Badge:** Indicates `AUTO`, `MANUAL`, or `ALL-RED`.
- **System Uptime Clock:** Displays running time (`HH:MM:SS`) reported by controller.

---

### Module 2: Command Ribbon & Real Countdown Engine
- **Mode Switching Controls:**
  - `⚡ Automatic Mode`: ESP32 automatically cycles Road 1 → 2 → 3 → 4 using configured seconds.
  - `🎮 Manual Override`: Admin selects road directly; chosen road remains green indefinitely.
  - `🛑 All-Red Hold`: Emergency safety freeze where all 4 approaches show Red.
- **Dynamic Phase Pill:**
  - Glowing colored dot: Green for active flow, Yellow for caution, Red for clearance.
  - Informative text: `ROAD 1 (North) • GREEN ACTIVE`.
- **Countdown Display:**
  - Large monospace digital timer (e.g. `10 sec`, `09 sec` ... `00 sec`).
  - Truthful calculation from hardware `remaining_ms`.

---

### Module 3: Vector 4-Way Intersection (Interactive SVG)
- **Road Markings:**
  - Dark slate asphalt crossing with curb radii.
  - Double yellow central dividing lines.
  - White pedestrian zebra crossings on all 4 approaches.
  - Diagonal yellow cross-hatch caution box in center junction.
- **Signal Lights:**
  - 4 physical housings placed at corner approaches (NW, NE, SE, SW).
  - SVG filters: `<radialGradient>` and `<feGaussianBlur>` to render authentic LED lamp illumination.
- **360° Orientation Selector:**
  - Dropdown allows rotating visual layout (0°, 90°, 180°, 270°) to match physical orientation of hardware model on table.

---

### Module 4: 4-Channel Road Control Cards
Each card represents one physical road channel:
- **Road 1 (North):** Red Pin: GPIO 18 | Yellow Pin: GPIO 19 | Green Pin: GPIO 21
- **Road 2 (East):**  Red Pin: GPIO 22 | Yellow Pin: GPIO 23 | Green Pin: GPIO 2
- **Road 3 (South):** Red Pin: GPIO 26 | Yellow Pin: GPIO 27 | Green Pin: GPIO 32
- **Road 4 (West):**  Red Pin: GPIO 4  | Yellow Pin: GPIO 5  | Green Pin: GPIO 25

**Features per Card:**
- Mini 3-lens traffic signal displaying real-time LED states.
- Current status badge (`🟢 GREEN ACTIVE`, `🟡 YELLOW CAUTION`, `🔴 RED STOP`).
- Individual phase remaining time.
- **Manual Priority Button (`Select Road`):**
  - Clicking sends command to ESP32.
  - Card turns to `Pending Clearance...` state with cyan border pulse.
  - Active road clears (Yellow 2s → All-Red 1s) → Selected road turns Green!

---

### Module 5: Timing Configuration Modal (⚙️)
Allows live adjustment of traffic intervals:
- **Green Light Duration:** 5 to 60 seconds (Default: 10s).
- **Yellow Caution Duration:** 1 to 10 seconds (Default: 2s).
- **All-Red Clearance Duration:** 1 to 5 seconds (Default: 1s).
- **Buttons:**
  - `Apply to ESP32`: Sends `POST /api/settings` and applies timings without rebooting.
  - `Reset Defaults`: Restores 10s / 2s / 1s defaults.

---

### Module 6: Live Audit Log & System Diagnostics
- **Activity Log:** Scrollable terminal logging all state changes with timestamps:
  - `[STATE] Road 1 Green phase started (10s)`
  - `[STATE] Road 1 Yellow caution started (2s)`
  - `[SAFETY] All-Red junction clearance active (1s)`
  - `[CMD] Admin requested manual green for Road 3`
  - `[SAFETY] Hardware interlock verified - 100% collision free`
- **Diagnostics Panel:**
  - Wi-Fi SoftAP SSID & Gateway (`192.168.4.1`)
  - Free Heap RAM (`~218 KB`)
  - API Roundtrip Latency (`~25 ms`)
  - Request Success Rate (`100% OK`)

---

## 🔌 REST API Specification

| Method | Endpoint | Description | Request Body | Response Body |
| :--- | :--- | :--- | :--- | :--- |
| **GET** | `/` | Serves complete embedded web app | None | HTML5 page |
| **GET** | `/api/status` | Returns live telemetry of all 12 pins & state | None | JSON with mode, active road, phase, remaining time, pin states |
| **POST** | `/api/mode` | Sets operating mode | `{"mode":"auto"}` | `{"status":"ok","mode":"auto"}` |
| **POST** | `/api/road` | Requests manual green for road | `{"road":3}` | `{"status":"queued","target_road":3}` |
| **POST** | `/api/settings` | Updates signal timings | `{"green":12,"yellow":3,"clearance":2}` | `{"status":"updated"}` |

---

## 📂 Project Files Reference

All files have been built and saved in the workspace:

1. **[`traffic-control/web/standalone_dashboard.html`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/web/standalone_dashboard.html):**  
   Single-file self-contained dashboard. Double-click to run directly in Google Chrome on your computer.
2. **[`traffic-control/web/index.html`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/web/index.html):**  
   Modular production frontend markup.
3. **[`traffic-control/web/styles.css`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/web/styles.css):**  
   Dark navy glassmorphic stylesheet with responsive mobile-first grid.
4. **[`traffic-control/web/app.js`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/web/app.js):**  
   Core client controller, non-blocking polling, SVG animator, and countdown ticker.
5. **[`traffic-control/firmware/TrafficControl/TrafficControl.ino`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/firmware/TrafficControl/TrafficControl.ino):**  
   Complete upgraded ESP32 firmware with SoftAP, non-blocking `millis()` state machine, and REST API.
6. **[`traffic-control/firmware/TrafficControl/web_assets.h`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/firmware/TrafficControl/web_assets.h):**  
   Embedded PROGMEM web server payload for 1-click Arduino IDE flashing.
7. **[`traffic-control/README.md`](file:///c:/Users/admin/OneDrive/Desktop/BTECH/SEM_03/07_ADEC_LAB/MINI_PROJECT/traffic-control/README.md):**  
   Wiring diagram, upload instructions, and Viva examination Q&A.

---

## 🚀 Execution Steps

1. **Step 1 (Preview Web App):** Open `standalone_dashboard.html` in your browser to verify the visual interface, countdown, and road selection buttons.
2. **Step 2 (Flash ESP32):** Open `TrafficControl.ino` in Arduino IDE and click **Upload**.
3. **Step 3 (Connect Phone/Laptop):** Connect to Wi-Fi `TrafficLight-ESP32` (Password: `Traffic123`).
4. **Step 4 (Launch Live App):** Open `http://192.168.4.1` in Chrome to control the physical traffic LEDs live!
