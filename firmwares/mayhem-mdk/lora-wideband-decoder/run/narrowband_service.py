#!/usr/bin/env python3
"""Host sidecar: decode raw WIO payloads POSTed from ESP (optional co-processor mode)."""

from __future__ import annotations

import argparse
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT = os.path.join(os.path.dirname(__file__), "..", "src")
sys.path.insert(0, os.path.normpath(ROOT))

from narrowband_dispatch import decode_air_frame  # noqa: E402

try:
    import requests
except ImportError:
    requests = None  # type: ignore


class RawHandler(BaseHTTPRequestHandler):
    esp_packet_url = ""
    default_region = "US915"

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8", errors="replace")
        try:
            ev = json.loads(body)
        except json.JSONDecodeError:
            self.send_response(400)
            self.end_headers()
            return
        hex_payload = ev.get("payload_hex") or ev.get("payload") or ""
        raw = bytes.fromhex(hex_payload.replace(" ", ""))
        rec = decode_air_frame(
            raw,
            freq_mhz=float(ev.get("freq", ev.get("freq_mhz", 915.0))),
            sf=int(ev.get("sf", 11)),
            bw_hz=int(ev.get("bw", ev.get("bw_hz", 250000))),
            region=ev.get("region", self.default_region),
            profile=ev.get("profile", "custom"),
            slot=ev.get("slot"),
            rssi=ev.get("rssi"),
            snr=ev.get("snr"),
        )
        rec["decode_backend"] = "python"
        if requests and self.esp_packet_url:
            try:
                requests.post(self.esp_packet_url, json=rec, timeout=2)
            except Exception as e:
                print("forward error:", e, file=sys.stderr)
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(rec).encode())

    def log_message(self, fmt, *args):
        print(fmt % args, file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--esp", default="http://192.168.4.1")
    ap.add_argument("--preset", default="US915-meshtastic")
    args = ap.parse_args()
    RawHandler.esp_packet_url = args.esp.rstrip("/") + "/lwd/packet"
    print("narrowband_service on :%d -> %s" % (args.port, RawHandler.esp_packet_url))
    HTTPServer(("0.0.0.0", args.port), RawHandler).serve_forever()


if __name__ == "__main__":
    main()
