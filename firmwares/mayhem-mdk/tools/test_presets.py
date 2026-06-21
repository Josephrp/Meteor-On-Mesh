#!/usr/bin/env python3
"""Validate presets.toml, code generation, and region helpers."""

from __future__ import annotations

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LWD_SRC = os.path.join(ROOT, "lora-wideband-decoder", "src")
sys.path.insert(0, LWD_SRC)

from lora.presets import load_presets, find_preset, region_from_freq, apply_preset_channels  # noqa: E402


def test_presets_load():
    presets = load_presets()
    assert len(presets) >= 20, "expected 20+ presets"
    assert find_preset("US915-meshtastic") is not None
    assert find_preset("EU868-meshtastic") is not None
    chans = apply_preset_channels("US915-meshtastic")
    assert len(chans) == 4
    assert abs(chans[0]["freq_mhz"] - 906.875) < 0.01


def test_region_from_freq():
    assert region_from_freq(906.875) == "US915"
    assert region_from_freq(869.525) == "EU868"
    assert region_from_freq(433.875) == "EU433"


def test_gen_lora_bands():
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gen_lora_bands.py")],
                       capture_output=True, text=True, cwd=ROOT)
    assert r.returncode == 0, r.stderr
    h = os.path.join(ROOT, "Source", "main", "lora_bands.cpp")
    assert os.path.isfile(h)
    with open(h, encoding="utf-8") as f:
        body = f.read()
    assert "US915-meshtastic" in body


def main():
    test_presets_load()
    test_region_from_freq()
    test_gen_lora_bands()
    print("test_presets: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
