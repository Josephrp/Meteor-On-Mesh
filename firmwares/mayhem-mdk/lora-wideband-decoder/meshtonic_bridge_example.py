#!/usr/bin/env python3
"""
Bridge: consume JSONL packet events from vendored LoRa Wideband Decoder
and forward full LoraDecodedRecord fields to Meshtonic ESP32.
"""

import argparse
import json
import time
import requests
import sys

PROTO_MAP = {
    "meshtastic": 1, "meshcore": 2, "lorawan": 3, "loramesher": 4,
    "lora_aprs": 5, "reticulum": 6, "disaster_radio": 7, "radiohead": 8, "ebyte_lora": 9,
}


def forward_to_esp(url: str, event: dict):
    proto_name = event.get("proto", "unknown")
    if isinstance(proto_name, str):
        proto_id = PROTO_MAP.get(proto_name, 0)
    else:
        proto_id = int(proto_name or 0)

    payload = {
        "ts": event.get("ts") or int(time.time() * 1000),
        "freq": event.get("freq") or event.get("freq_mhz"),
        "bw": event.get("bw") or event.get("bw_hz"),
        "sf": event.get("sf"),
        "cr": event.get("cr"),
        "rssi": event.get("rssi"),
        "snr": event.get("snr"),
        "slot": event.get("slot"),
        "region": event.get("region") or event.get("band"),
        "profile": event.get("profile"),
        "preset_id": event.get("preset_id"),
        "band": event.get("band") or event.get("region"),
        "proto": proto_id,
        "confidence": event.get("confidence", "candidate"),
        "decrypted": event.get("decrypted", False),
        "payload_hex": event.get("payload_hex") or event.get("payload"),
        "info": event.get("info") or event.get("text") or event.get("port_name"),
        "key_label": event.get("key_label", ""),
        "decode_backend": event.get("decode_backend", "host_wideband"),
        "sync": event.get("sync"),
    }
    try:
        r = requests.post(url, json=payload, timeout=2)
        if r.status_code >= 300:
            print("bridge: POST failed", r.status_code, file=sys.stderr)
    except Exception as e:
        print("bridge error:", e, file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://192.168.4.1/lwd/packet", help="ESP endpoint")
    ap.add_argument("--infile", default="-", help="JSONL file or '-' for stdin")
    args = ap.parse_args()

    fh = sys.stdin if args.infile == "-" else open(args.infile, "r", encoding="utf-8")
    print("bridge listening (v2 full schema)...", file=sys.stderr)
    for line in fh:
        line = line.strip()
        if not line:
            continue
        try:
            ev = json.loads(line)
            forward_to_esp(args.url, ev)
        except Exception:
            pass


if __name__ == "__main__":
    main()
