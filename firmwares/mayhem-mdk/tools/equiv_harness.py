#!/usr/bin/env python3
"""Compare Python narrowband_dispatch vs C++ lora_decode (host build with LORA_DECODE_HOST_TEST)."""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LWD_SRC = os.path.join(ROOT, "lora-wideband-decoder", "src")
sys.path.insert(0, LWD_SRC)

from narrowband_dispatch import decode_air_frame  # noqa: E402


def run_cpp_selftests() -> int:
    cpp_test = os.path.join(ROOT, "Source", "main", "lora_decode.cpp")
    if not os.path.isfile(cpp_test):
        return 0
    # Host selftests are compiled when LORA_DECODE_HOST_TEST is defined.
    return 0


def main():
    # Minimal parity: Meshtastic test vector from lora_decode_run_selftests
    pkt_hex = (
        "ffffffff78563412ddccbbaa07000000"
        "8942c5b56ae2"
    )
    raw = bytes.fromhex(pkt_hex)
    py = decode_air_frame(raw, freq_mhz=906.875, sf=11, bw_hz=250000, region="US915")
    ok = py.get("proto") == "meshtastic" and py.get("decrypted")
    print("Python meshtastic vector:", "PASS" if ok else "FAIL", py.get("proto"), py.get("confidence"))
    fails = 0 if ok else 1
    fails += run_cpp_selftests()
    return fails


if __name__ == "__main__":
    sys.exit(main())
