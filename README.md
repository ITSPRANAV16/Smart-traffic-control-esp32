# 🚦 Smart Traffic Control System — Advanced ESP32 SCADA Command Cockpit

**Designed & Engineered by Team Alpha**  
**B.Tech SEM 03 • ADEC Lab Mini-Project**  
Complete offline, enterprise-grade 4-way traffic SCADA control system with 60 FPS vector intersection, live dynamic vehicle simulation, sub-second continuous countdown radar gauge, manual priority override, and hardware safety interlocks.

---

## 📌 Hardware Pinout Configuration (Active-HIGH Logic)

The project uses an **ESP32 DevKit** board controlling **12 LEDs** (4 Red, 4 Yellow, 4 Green) with Active-HIGH logic (`digitalWrite(pin, HIGH)` = ON).

| Road # | Direction | 🔴 Red LED Pin | 🟡 Yellow LED Pin | 🟢 Green LED Pin | Logic Type |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Road 1** | North | **GPIO 18** | **GPIO 19** | **GPIO 21** | Active-HIGH |
| **Road 2** | East | **GPIO 22** | **GPIO 23** | **GPIO 2** | Active-HIGH |
| **Road 3** | South | **GPIO 26** | **GPIO 27** | **GPIO 32** | Active-HIGH |
| **Road 4** | West | **GPIO 4** | **GPIO 5** | **GPIO 25** | Active-HIGH |

> **Note on GND:** Connect the cathode (short leg) of each LED through a **220Ω / 330Ω resistor** to the ESP32 **GND** pin.

---

## 🚀 Key System Features

1. **100% Offline Embedded Web Server:**
   - Hosted directly in ESP32 Flash Memory (`PROGMEM`).
   - Requires **zero internet connection**, no router, and no external CDNs/fonts.
2. **Interactive 4-Way SVG Intersection:**
   - Top-down vector visualization with authentic radial LED shaders, asphalt textures, lane dividers, and pedestrian zebra crossings.
   - 360° Board Alignment selector (0°, 90°, 180°, 270°) to match your physical circuit layout.
3. **True Live Countdown Timer:**
   - Real-time countdown on the dashboard reflecting exact remaining seconds from the ESP32 hardware state machine.
4. **Dual Operating Modes:**
   - **Automatic Mode:** Automatically cycles Road 1 → 2 → 3 → 4 using configurable durations (Default: Green 10s, Yellow 2s, Clearance 1s).
   - **Manual Mode:** Admin selects any road (Road 1–4). The controller safely clears the active road (Yellow → All-Red Clearance) and turns the requested road Green, holding it until another road is chosen!
   - **Emergency All-Red Hold:** Halts all 4 approaches immediately for pedestrian crossings or emergency vehicles.
5. **Hardware Safety Interlock:**
   - Embedded check guarantees that **no two conflicting green/yellow signals can ever be active simultaneously**. If any logic anomaly is detected, the controller fails safe to All-Red.
6. **Mobile PWA Capability:**
   - Tap *"Add to Home Screen"* in Android Chrome to launch the dashboard as a native full-screen app with custom icon and no URL bar.

---

## 🛠️ Arduino IDE Upload Instructions

### 1. Prerequisites
- [Arduino IDE](https://www.arduino.cc/en/software) (Version 2.x recommended).
- **ESP32 Board Package:**
  - In Arduino IDE: `File` → `Preferences` → `Additional boards manager URLs`:
    ```
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ```
  - Open `Tools` → `Board` → `Boards Manager`, search for **esp32** (by Espressif) and click **Install**.

### 2. Flashing the Firmware
1. Connect your ESP32 to your computer using a micro-USB / Type-C cable.
2. Open `traffic-control/firmware/TrafficControl/TrafficControl.ino`.
3. Select your board and port:
   - `Tools` → `Board` → `ESP32 Arduino` → **ESP32 Dev Module**.
   - `Tools` → `Port` → Select your ESP32 COM port (e.g., COM3, COM5).
4. Click **Upload** (Arrow button) in Arduino IDE.
5. Wait until upload completes: `Leaving... Hard resetting via RTS pin...`

---

## 📱 Mobile / Laptop Wi-Fi Connection Guide

1. On your Android phone, iPhone, or laptop, open Wi-Fi settings.
2. Connect to the ESP32 network:
   - **SSID:** `TrafficLight-ESP32`
   - **Password:** `Traffic123`
3. *Note for Android:* If Android shows *"This network has no internet access. Stay connected?"*, tap **YES / Keep Connected**.
4. Open **Google Chrome** (or any browser) and navigate to:
   ```
   http://192.168.4.1
   ```
5. The full smart traffic control dashboard will open immediately!
6. **Turn into an App:** In Chrome, tap the 3-dots menu (⋮) → tap **"Add to Home Screen"** or **"Install app"**. A "TrafficControl" icon will be added to your mobile home screen!

---

## 🔌 REST API Documentation

The ESP32 firmware exposes clean JSON endpoints for integration with web dashboards, mobile apps, or Python test scripts:

### 1. `GET /api/status`
Returns live state of all 12 pins, current phase, remaining time, and diagnostics.
**Sample Response:**
```json
{
  "mode": "auto",
  "active_road": 1,
  "phase": "GREEN",
  "remaining_ms": 7800,
  "total_ms": 10000,
  "pending_road": 0,
  "uptime_sec": 128,
  "free_heap": 218450,
  "settings": {
    "greenTime": 10,
    "yellowTime": 2,
    "clearanceTime": 1
  },
  "roads": [
    {"road": 1, "r": 0, "y": 0, "g": 1},
    {"road": 2, "r": 1, "y": 0, "g": 0},
    {"road": 3, "r": 1, "y": 0, "g": 0},
    {"road": 4, "r": 1, "y": 0, "g": 0}
  ]
}
```

### 2. `POST /api/mode`
Switch between Automatic, Manual, and Emergency All-Red.
```json
{"mode": "manual"}   // Options: "auto" | "manual" | "all_red"
```

### 3. `POST /api/road`
Manually request green signal for a specific road (Road 1–4).
```json
{"road": 3}
```

### 4. `POST /api/settings`
Update timing settings dynamically without rebooting the controller.
```json
{
  "green": 15,      // 5 to 60 seconds
  "yellow": 3,      // 1 to 10 seconds
  "clearance": 2    // 1 to 5 seconds
}
```

---

## 🎯 Viva & Examination Q&A Guide

### Q1: Why did you replace `delay()` with `millis()`?
**Answer:**
In standard Arduino code, `delay()` completely halts the CPU. During a 10-second green light delay, the ESP32 cannot process Wi-Fi packets, cannot respond to HTTP requests, and cannot accept manual button clicks. By using non-blocking `millis()`, the state machine checks elapsed time every loop iteration while simultaneously handling web client requests and safety interlocking with zero lag.

### Q2: What is Hardware Interlocking in traffic systems?
**Answer:**
Hardware interlocking ensures that catastrophic states—such as two perpendicular roads having a green light simultaneously—are physically impossible. In our firmware, before writing to GPIOs, `applyHardwareOutputs()` audits all 12 channels. If more than one Green or Yellow is active, it trips immediately to a fail-safe All-Red state.

### Q3: How does the system work without internet access?
**Answer:**
The ESP32 runs in **SoftAP (Software Access Point) mode**, creating its own private Wi-Fi network (`TrafficLight-ESP32`). The HTML, CSS, JavaScript, and SVG assets are compiled directly into the ESP32 flash memory (`PROGMEM`). The browser fetches everything directly from `192.168.4.1`, making the system 100% self-reliant.

### Q4: Explain the safe transition sequence when switching roads manually.
**Answer:**
When an admin selects Road 3 while Road 1 is Green:
1. Road 1 switches from **Green → Yellow** (2 seconds caution period).
2. All roads switch to **All-Red Clearance** (1 second safety buffer to clear the junction box).
3. Road 3 turns **Green** and remains Green until the admin chooses another action.
This prevents vehicle collisions at the junction.

---

## 📁 Project Directory Structure

```
traffic-control/
├── firmware/
│   └── TrafficControl/
│       ├── TrafficControl.ino    # Complete non-blocking ESP32 Arduino firmware
│       └── web_assets.h          # Embedded PROGMEM web dashboard for 1-click upload
├── web/
│   ├── index.html                # Standalone HTML5 dashboard
│   ├── styles.css                # Premium glassmorphic responsive stylesheet
│   ├── app.js                    # Core client controller & SVG animation engine
│   └── manifest.json             # PWA metadata for Android home-screen installation
└── README.md                     # Complete project documentation & viva guide
```
