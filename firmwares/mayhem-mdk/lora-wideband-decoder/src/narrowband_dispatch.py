#!/usr/bin/env python3
"""
Narrowband decode API — run vendored LWD protocol dispatch on raw LoRa payloads
(from WIO SX1262 or bridge) without IQ demod.
"""

from __future__ import annotations

import os
import sys
import time
from typing import Any

_SRC = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "src"))
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from lora.lorawan_grid import on_lorawan_grid  # noqa: E402
from lora.presets import region_from_freq  # noqa: E402

_PROTO_ID = {
    "meshtastic": 1, "meshcore": 2, "lorawan": 3, "loramesher": 4,
    "lora_aprs": 5, "reticulum": 6, "disaster_radio": 7, "radiohead": 8, "ebyte_lora": 9,
}


def _normalize_record(rec: dict[str, Any] | None, raw: bytes, ctx: dict) -> dict[str, Any]:
    region = ctx.get("region") or region_from_freq(ctx.get("freq_mhz", 0))
    base = {
        "ts": int(time.time() * 1000),
        "slot": ctx.get("slot"),
        "region": region,
        "profile": ctx.get("profile", "custom"),
        "preset_id": ctx.get("preset_id", ""),
        "band": region,
        "freq": ctx.get("freq_mhz"),
        "sf": ctx.get("sf"),
        "bw": ctx.get("bw_hz"),
        "cr": ctx.get("cr", 5),
        "rssi": ctx.get("rssi"),
        "snr": ctx.get("snr"),
        "sync": ctx.get("sync_word"),
        "proto": "unknown",
        "confidence": "candidate",
        "decrypted": False,
        "payload_hex": raw.hex(),
        "info": "",
        "key_label": "",
        "decode_backend": "python",
        "extras": {},
    }
    if not rec:
        return base
    proto = rec.get("proto", "unknown")
    base["proto"] = proto
    base["confidence"] = rec.get("confidence", "candidate")
    base["decrypted"] = bool(rec.get("decrypted"))
    if rec.get("from"):
        base["info"] = str(rec.get("from", ""))
    if rec.get("text"):
        base["info"] = str(rec["text"])[:120]
    if rec.get("port_name"):
        base["info"] = str(rec["port_name"])
    if rec.get("mc_type"):
        base["info"] = "MC:%s" % rec["mc_type"]
    if proto == "lorawan":
        base["info"] = str(rec.get("mtype") or rec.get("MsgType") or "LoRaWAN")
    base["extras"] = {k: v for k, v in rec.items() if k not in base}
    return base


def _try_parsers(payload: list, ctx: dict) -> dict[str, Any] | None:
    import decoder as dec  # noqa: WPS433

    rf = ctx.get("rf") or {"freq_mhz": ctx.get("freq_mhz"), "sf": ctx.get("sf"), "bw": ctx.get("bw_hz")}
    region = ctx.get("region") or "US915"

    # Meshtastic: 16-byte header + decrypt pipeline
    if len(payload) >= 16:
        try:
            dec.parse_meshtastic_packet(payload, _rf=rf)
            sender = dec.node_id_str(payload[4:8]) if hasattr(dec, "node_id_str") else ""
            return {"proto": "meshtastic", "confidence": "verified", "decrypted": True, "from": sender}
        except Exception:
            pass

    parsers = [
        ("lora_aprs", dec.parse_lora_aprs_packet),
        ("lorawan", dec.parse_lorawan_packet),
        ("meshcore", dec.parse_meshcore_packet),
        ("loramesher", dec.parse_loramesher_packet),
        ("reticulum", dec.parse_reticulum_packet),
        ("disaster_radio", dec.parse_disaster_radio_packet),
        ("ebyte_lora", dec.parse_lora_p2p_packet),
        ("radiohead", dec.parse_radiohead_packet),
    ]
    best = None
    tier = {"verified": 3, "confirmed": 2, "candidate": 1}
    for name, fn in parsers:
        try:
            rec = fn(payload, rf=rf) if name != "meshcore" else fn(payload)
            if rec:
                rec["proto"] = rec.get("proto", name)
                conf = rec.get("confidence", "candidate")
                if name == "lorawan":
                    grid = on_lorawan_grid(ctx.get("freq_mhz"), ctx.get("sf"), ctx.get("bw_hz"), region)
                    rec["on_grid"] = grid
                    if grid:
                        conf = "confirmed"
                if not best or tier.get(conf, 0) > tier.get(best.get("confidence", ""), 0):
                    rec["confidence"] = conf
                    best = rec
        except Exception:
            continue
    return best


def decode_air_frame(
    payload: bytes | list[int],
    *,
    freq_mhz: float,
    sf: int,
    bw_hz: int,
    cr: int = 5,
    region: str = "US915",
    profile: str = "custom",
    preset_id: str = "",
    sync_word: int | None = None,
    rssi: float | None = None,
    snr: float | None = None,
    slot: int | None = None,
) -> dict[str, Any]:
    if isinstance(payload, list):
        payload = bytes(payload)
    ctx = {
        "freq_mhz": freq_mhz,
        "sf": sf,
        "bw_hz": bw_hz,
        "cr": cr,
        "region": region,
        "profile": profile,
        "preset_id": preset_id,
        "sync_word": sync_word,
        "rssi": rssi,
        "snr": snr,
        "slot": slot,
        "rf": {"freq_mhz": freq_mhz, "sf": sf, "bw": bw_hz},
    }
    rec = _try_parsers(list(payload), ctx)
    return _normalize_record(rec, payload, ctx)


if __name__ == "__main__":
    import argparse
    import json

    ap = argparse.ArgumentParser(description="Decode raw LoRa payload hex")
    ap.add_argument("--hex", required=True, help="payload hex")
    ap.add_argument("--freq", type=float, default=906.875)
    ap.add_argument("--sf", type=int, default=11)
    ap.add_argument("--bw", type=int, default=250000)
    ap.add_argument("--region", default="US915")
    args = ap.parse_args()
    raw = bytes.fromhex(args.hex.replace(" ", ""))
    out = decode_air_frame(raw, freq_mhz=args.freq, sf=args.sf, bw_hz=args.bw, region=args.region)
    print(json.dumps(out, indent=2))
