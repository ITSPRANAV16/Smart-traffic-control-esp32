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

PORT = 5000
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
                if state["mode"] == "emergency":
                    st = "RED"
                elif r == state["activeRoad"]:
                    st = phase_names[state["phase"]]
                else:
                    st = "RED"
                roads.append({"road": r + 1, "state": st})

            resp = {
                "status": "success",
                "mode": state["mode"],
                "activeRoad": state["activeRoad"],
                "phase": state["phase"],
                "phaseName": phase_names[state["phase"]],
                "remainingSeconds": max(0, state["remainingSeconds"]),
                "uptime_sec": uptime,
                "free_heap": 218940 - (uptime % 100),
                "wifi_clients": 1,
                "ip": "127.0.0.1",
                "roads": roads
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

        if parsed.path == '/api/manual':
            road = data.get('road', 0)
            state["mode"] = "manual"
            state["activeRoad"] = int(road)
            state["phase"] = 0
            state["remainingSeconds"] = 999
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "success", "mode": "manual", "road": road}).encode('utf-8'))
            return

        elif parsed.path == '/api/emergency':
            state["mode"] = "emergency"
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "emergency_activated"}).encode('utf-8'))
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
            if 'greenTime' in data:
                state["greenTime"] = int(data['greenTime'])
            if 'yellowTime' in data:
                state["yellowTime"] = int(data['yellowTime'])
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.do_cors()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "settings_updated"}).encode('utf-8'))
            return

        self.send_response(404)
        self.end_headers()

def run():
    server_address = ('127.0.0.1', PORT)
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
