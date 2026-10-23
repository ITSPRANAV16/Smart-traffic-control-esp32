"""
Smart City Traffic SCADA - Local Development Server
Team Alpha - B.Tech ADEC Lab Mini-Project
Serves the web dashboard and provides live mock ESP32 telemetry.
"""

import os
import sys
import json
import time
import socket
import webbrowser
from http.server import SimpleHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

PORT = int(os.environ.get('PORT', 5000))
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WEB_DIR = os.path.join(BASE_DIR, 'web')

state = {
    "mode": "auto",
    "activeRoad": 0,
    "phase": 0,
    "remainingSeconds": 10,
    "greenTime": 10,
    "yellowTime": 2,
    "clearanceTime": 1,
    "uptime_start": time.time(),
    "last_tick": time.time()
}

class TrafficDevHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def log_message(self, format, *args):
        # Clean logging
        pass

    def do_cors(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')

    def do_OPTIONS(self):
        self.send_response(200)
        self.do_cors()
        self.end_headers()

    def update_state(self):
        now = time.time()
        elapsed = int(now - state["last_tick"])
        if elapsed >= 1:
            state["last_tick"] = now
            if state["mode"] == "auto":
                state["remainingSeconds"] -= elapsed
                if state["remainingSeconds"] <= 0:
                    if state["phase"] == 0:  # GREEN -> YELLOW
                        state["phase"] = 1
                        state["remainingSeconds"] = state["yellowTime"]
                    elif state["phase"] == 1:  # YELLOW -> CLEARANCE
                        state["phase"] = 2
                        state["remainingSeconds"] = state["clearanceTime"]
                    else:  # CLEARANCE -> NEXT ROAD GREEN
                        state["phase"] = 0
                        state["activeRoad"] = (state["activeRoad"] + 1) % 4
                        state["remainingSeconds"] = state["greenTime"]

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/status':
            self.update_state()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()

            uptime = int(time.time() - state["uptime_start"])
            phase_names = ["GREEN", "YELLOW", "CLEARANCE"]
            roads = []
            for r in range(4):
                if state["mode"] in ["emergency", "all_red"]:
                    roads.append({"road": r + 1, "r": 1, "y": 0, "g": 0, "state": "RED"})
                elif r == state["activeRoad"]:
                    p = state["phase"]
                    roads.append({
                        "road": r + 1,
                        "r": 1 if p == 2 else 0,
                        "y": 1 if p == 1 else 0,
                        "g": 1 if p == 0 else 0,
                        "state": phase_names[p]
                    })
                else:
                    roads.append({"road": r + 1, "r": 1, "y": 0, "g": 0, "state": "RED"})

            rem = max(0, state["remainingSeconds"])
            resp = {
                "status": "online",
                "mode": state["mode"],
                "activeRoad": state["activeRoad"],
                "active_road": state["activeRoad"] + 1,
                "phase": phase_names[state["phase"]],
                "remaining_ms": rem * 1000,
                "remainingSeconds": rem,
                "total_ms": (state["greenTime"] if state["phase"] == 0 else state["yellowTime"]) * 1000,
                "uptime_sec": uptime,
                "free_heap": 218940 - (uptime % 100),
                "wifi_clients": 1,
                "ip": "127.0.0.1",
                "roads": roads,
                "settings": {
                    "greenTime": state["greenTime"],
                    "yellowTime": state["yellowTime"],
                    "clearanceTime": state["clearanceTime"]
                }
            }
            self.wfile.write(json.dumps(resp).encode('utf-8'))
            return

        super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        content_len = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_len).decode('utf-8') if content_len > 0 else ""
        data = {}
        try:
            if body:
                data = json.loads(body)
        except Exception:
            pass

        if parsed.path in ['/api/manual', '/api/road']:
            road = data.get('road', 1)
            try:
                road_idx = int(road) - 1
            except:
                road_idx = 0
            state["mode"] = "manual"
            state["activeRoad"] = max(0, min(3, road_idx))
            state["phase"] = 0
            state["remainingSeconds"] = 999
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "queued", "target_road": road}).encode('utf-8'))
            return

        elif parsed.path in ['/api/emergency', '/api/mode']:
            mode_val = data.get('mode', 'all_red')
            state["mode"] = "all_red" if mode_val in ["all_red", "emergency"] else mode_val
            if state["mode"] == "auto":
                state["phase"] = 0
                state["remainingSeconds"] = state["greenTime"]
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok", "mode": state["mode"]}).encode('utf-8'))
            return

        elif parsed.path == '/api/resume':
            state["mode"] = "auto"
            state["phase"] = 0
            state["remainingSeconds"] = state["greenTime"]
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "auto_resumed"}).encode('utf-8'))
            return

        elif parsed.path == '/api/settings':
            for k in ['green', 'greenTime']:
                if k in data:
                    state["greenTime"] = int(data[k])
            for k in ['yellow', 'yellowTime']:
                if k in data:
                    state["yellowTime"] = int(data[k])
            for k in ['clearance', 'clearanceTime']:
                if k in data:
                    state["clearanceTime"] = int(data[k])
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "updated"}).encode('utf-8'))
            return

        self.send_response(404)
        self.end_headers()

def run():
    server_address = ('0.0.0.0', PORT)
    httpd = HTTPServer(server_address, TrafficDevHandler)
    url = f"http://localhost:{PORT}"
    print(f"============================================================")
    print(f" SMART CITY TRAFFIC SCADA - LOCAL DEV SERVER")
    print(f" DESIGNED & ENGINEERED BY TEAM ALPHA")
    print(f" Local URL: {url}")
    print(f" Serving Directory: {WEB_DIR}")
    print(f" Telemetry Mock: /api/status active")
    print(f"============================================================")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        httpd.server_close()

if __name__ == '__main__':
    run()
