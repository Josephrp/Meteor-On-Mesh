#!/usr/bin/env python3
"""
Unified Meshtonic listener orchestrator — preset workflows for wideband + ESP bridge.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LWD = os.path.join(ROOT, "lora-wideband-decoder")
SRC = os.path.join(LWD, "src")
sys.path.insert(0, SRC)

from lora.presets import find_preset  # noqa: E402
import meshtonic_pp as mpp  # noqa: E402


def apply_preset_to_esp(esp: str, preset_id: str) -> int:
    res = mpp.apply_preset_to_esp(esp, preset_id)
    if not res.get("ok"):
        print("ESP preset failed:", res.get("error"), file=sys.stderr)
        return 1
    print("Applied preset", preset_id, "on", esp)
    return 0


def run_wideband(preset_id: str, esp: str) -> int:
    p = find_preset(preset_id)
    if not p:
        print("Unknown preset", preset_id, file=sys.stderr)
        return 1
    data_dir = os.path.join(LWD, "lora_web")
    mpp.save_settings(data_dir, {
        "esp_url": esp,
        "preset_id": preset_id,
        "bridge_enabled": True,
        "apply_preset_on_start": True,
    })
    apply_preset_to_esp(esp, preset_id)
    app = os.path.join(LWD, "run", "meshtonic_app.py")
    print("Launching standalone web UI (HackRF + bridge)...")
    return subprocess.call([sys.executable, app])


def run_sidecar(preset_id: str, esp: str) -> int:
    svc = os.path.join(LWD, "run", "narrowband_service.py")
    subprocess.Popen([sys.executable, svc, "--esp", esp, "--preset", preset_id])
    return apply_preset_to_esp(esp, preset_id)


def main():
    ap = argparse.ArgumentParser(description="Meshtonic LWD orchestrator")
    ap.add_argument("--preset", default="US915-meshtastic")
    ap.add_argument("--mode", choices=["wideband", "narrowband-sidecar", "apply"], default="apply")
    ap.add_argument("--esp", default="http://192.168.4.1")
    args = ap.parse_args()
    if args.mode == "wideband":
        return run_wideband(args.preset, args.esp)
    if args.mode == "narrowband-sidecar":
        return run_sidecar(args.preset, args.esp)
    return apply_preset_to_esp(args.esp, args.preset)


if __name__ == "__main__":
    sys.exit(main())
