#!/usr/bin/env python3
"""
Example bridge: consume JSONL packet events from the vendored LoRa Wideband Decoder
and forward normalized "heard packet" events to a Meshtonic ESP32 companion
(over HTTP POST to its web UI, or serial).

Run the decoder with harness output, then run this bridge pointing at the ESP IP.

Decoder example:
  python -m src.soapy_rx --profile hackrf ... | python src/decoder.py --harness /tmp/mt_packets.jsonl ...

Then:
  python meshtonic_bridge_example.py --url http://192.168.4.1/meshtonic/packet --infile /tmp/mt_packets.jsonl
"""

import argparse
import json
import time
import requests
import sys

def forward_to_esp(url: str, event: dict):
    try:
        # Normalize minimal event for the companion
        payload = {
            "ts": event.get("ts") or time.time(),
            "freq": event.get("freq"),
            "bw": event.get("bw"),
            "sf": event.get("sf"),
            "cr": event.get("cr"),
            "rssi": event.get("rssi"),
            "snr": event.get("snr"),
            "payload": event.get("payload_hex") or event.get("payload"),
            "type": event.get("type") or "lora",
            "src": "hackrf-wideband"
        }
        r = requests.post(url, json=payload, timeout=2)
        if r.status_code >= 300:
            print("bridge: POST failed", r.status_code, file=sys.stderr)
    except Exception as e:
        print("bridge error:", e, file=sys.stderr)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://192.168.4.1/meshtonic/packet", help="ESP endpoint")
    ap.add_argument("--infile", default="-", help="JSONL file or '-' for stdin")
    args = ap.parse_args()

    fh = sys.stdin if args.infile == "-" else open(args.infile, "r", encoding="utf-8")
    print("bridge listening...", file=sys.stderr)
    for line in fh:
        line = line.strip()
        if not line: continue
        try:
            ev = json.loads(line)
            forward_to_esp(args.url, ev)
        except Exception:
            pass

if __name__ == "__main__":
    main()
