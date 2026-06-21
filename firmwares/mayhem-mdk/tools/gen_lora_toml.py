#!/usr/bin/env python3
"""Generate lora.toml snippet for a preset (wideband SDR center)."""

from __future__ import annotations

import argparse
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "lora-wideband-decoder", "src")
sys.path.insert(0, os.path.normpath(ROOT))

from lora.presets import find_preset  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="US915-meshtastic")
    ap.add_argument("-o", "--output", help="Write to file (default stdout)")
    args = ap.parse_args()
    p = find_preset(args.preset)
    if not p:
        print("Unknown preset:", args.preset, file=sys.stderr)
        return 1
    center = p.get("sdr_center_mhz", 915.0)
    lw = p.get("lorawan_region") or p.get("region", "US915")
    out = """# Generated for preset: %s
[radio]
rate_hz = 20000000
bandwidth_hz = 20000000
center_mhz = %.1f
format = "sc16"

[preset]
id = "%s"
region = "%s"
lora_region = "%s"
""" % (p["id"], center, p["id"], p["region"], lw)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(out)
    else:
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
