#!/usr/bin/env python3
"""Bridge from vendored LWD [PKT] JSONL to ESP32 with full LoraDecodedRecord schema."""

import argparse
import json
import time
import requests
import sys

PROTO_MAP = {
    "meshtastic": 1, "meshcore": 2, "lorawan": 3, "loramesher": 4,
    "lora_aprs": 5, "reticulum": 6, "disaster_radio": 7, "radiohead": 8, "ebyte_lora": 9,
}


def normalize(ev: dict) -> dict:
    proto_name = ev.get("proto") or ev.get("type") or "unknown"
    proto_id = PROTO_MAP.get(proto_name, 0) if isinstance(proto_name, str) else int(proto_name or 0)
    return {
        "ts": ev.get("ts") or int(time.time() * 1000),
        "freq": ev.get("freq_mhz") or ev.get("freq"),
        "bw": ev.get("bw_hz") or ev.get("bw"),
        "sf": ev.get("sf"),
        "rssi": ev.get("rssi"),
        "snr": ev.get("snr"),
        "slot": ev.get("slot"),
        "region": ev.get("region") or ev.get("band"),
        "profile": ev.get("profile"),
        "preset_id": ev.get("preset_id"),
        "band": ev.get("band") or ev.get("region"),
        "proto": proto_id,
        "confidence": ev.get("confidence", "candidate"),
        "decrypted": ev.get("decrypted", False),
        "payload_hex": ev.get("payload_hex") or ev.get("payload"),
        "info": ev.get("info") or ev.get("text") or str(proto_name),
        "key_label": ev.get("key_label", ""),
        "decode_backend": ev.get("decode_backend", "host_wideband"),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://192.168.4.1/lwd/packet")
    ap.add_argument("--infile", default="-")
    args = ap.parse_args()

    fh = sys.stdin if args.infile == "-" else open(args.infile, "r", encoding="utf-8")
    print("LWD -> ESP32PP bridge v2 ->", args.url, file=sys.stderr)

    for line in fh:
        line = line.strip()
        if not line:
            continue
        try:
            if line.startswith("[PKT]"):
                js = line.split(" ", 1)[1]
                ev = json.loads(js)
            else:
                ev = json.loads(line)
            payload = normalize(ev)
            r = requests.post(args.url, json=payload, timeout=3)
            if r.status_code >= 300:
                print("POST failed", r.status_code, file=sys.stderr)
        except Exception as e:
            print("bridge error:", e, file=sys.stderr)


if __name__ == "__main__":
    main()
