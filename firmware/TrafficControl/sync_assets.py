import os

src = r"c:\Users\admin\OneDrive\Desktop\BTECH\SEM_03\07_ADEC_LAB\MINI_PROJECT\traffic-control\web\standalone_dashboard.html"
dst = r"c:\Users\admin\OneDrive\Desktop\BTECH\SEM_03\07_ADEC_LAB\MINI_PROJECT\traffic-control\firmware\TrafficControl\web_assets.h"

with open(src, "r", encoding="utf-8") as f:
    html = f.read()

header = f"""#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

// Ultra-Advanced Smart City Traffic SCADA Cockpit (White & Navy Blue Unified UI)
const char INDEX_HTML[] PROGMEM = R"rawliteral({html})rawliteral";

#endif // WEB_ASSETS_H
"""

with open(dst, "w", encoding="utf-8") as f:
    f.write(header)

print("Successfully written web_assets.h with size:", len(header))
