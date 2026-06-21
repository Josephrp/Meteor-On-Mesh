#!/usr/bin/env python3
"""CLI: decode raw payload hex for a preset (uses narrowband_dispatch)."""

import argparse
import json
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "lora-wideband-decoder", "src")
sys.path.insert(0, os.path.normpath(ROOT))

from narrowband_dispatch import decode_air_frame  # noqa: E402
from lora.presets import find_preset  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="US915-meshtastic")
    ap.add_argument("--hex", required=True)
    ap.add_argument("--slot", type=int, default=0)
    args = ap.parse_args()
    p = find_preset(args.preset)
    freqs = p.get("slot_freqs_mhz", [915.0]) if p else [915.0]
    freq = freqs[min(args.slot, len(freqs) - 1)]
    raw = bytes.fromhex(args.hex.replace(" ", ""))
    out = decode_air_frame(
        raw,
        freq_mhz=freq,
        sf=int(p["sf"]) if p else 11,
        bw_hz=int(p["bw_hz"]) if p else 250000,
        region=p.get("region", "US915") if p else "US915",
        profile=p.get("profile", "custom") if p else "custom",
        preset_id=p.get("id", "") if p else "",
        slot=args.slot,
    )
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
