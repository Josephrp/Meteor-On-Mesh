#!/usr/bin/env python3
"""
Bridge from the vendored LoRa Wideband Decoder JSONL output
into the ESP32 Mayhem MDK "LoraDecoder" application.

The ESP32 exposes POST /lwd/packet (when the LoraDecoder EPApp is active
or always via the global feed). Packets then appear in the web UI and
are queryable by PortaPack over I2C.

Usage:
  # 1. Run the wideband decoder writing JSONL (harness or stdout with [PKT])
  python -m src.soapy_rx ... | python src/decoder.py --harness /tmp/lwd.jsonl ...

  # 2. Run this bridge
  python bridge_to_esp32pp.py --url http://<esp-ip>/lwd/packet --infile /tmp/lwd.jsonl
"""

import argparse, json, time, requests, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://192.168.4.1/lwd/packet")
    ap.add_argument("--infile", default="-")
    args = ap.parse_args()

    fh = sys.stdin if args.infile == "-" else open(args.infile, "r", encoding="utf-8")
    print("LWD -> ESP32PP bridge running to", args.url, file=sys.stderr)

    for line in fh:
        line = line.strip()
        if not line or not line.startswith("[PKT]"): 
            continue
        try:
            # The decoder emits lines like: [PKT] {json}
            js = line.split(" ", 1)[1]
            ev = json.loads(js)
            # Normalize to what the ESP expects
            payload = {
                "ts": ev.get("ts") or time.time(),
                "freq": ev.get("freq_mhz") or ev.get("freq"),
                "bw": ev.get("bw_hz") or ev.get("bw"),
                "sf": ev.get("sf"),
                "rssi": ev.get("rssi"),
                "snr": ev.get("snr"),
                "payload": ev.get("payload_hex") or ev.get("payload"),
                "type": ev.get("proto") or ev.get("type"),
            }
            r = requests.post(args.url, json=payload, timeout=3)
            if r.status_code >= 300:
                print("POST failed", r.status_code, file=sys.stderr)
        except Exception as e:
            print("bridge error:", e, file=sys.stderr)

if __name__ == "__main__":
    main()
