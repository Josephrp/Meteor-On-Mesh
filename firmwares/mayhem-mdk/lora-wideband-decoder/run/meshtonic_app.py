#!/usr/bin/env python3
"""OPTIONAL dev tool — full LWD web UI on a PC with HackRF.

Meshtonic H4M does NOT require this. The board runs the LoRa Decoder EPApp
on-device (ESP OLED + PortaPack LCD + 4x WIO). Use this only for lab/debug.

Usage:
    python run/meshtonic_app.py
"""
from __future__ import annotations

import json
import os
import sys
import threading
import time
import webbrowser

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_HERE)
_SRC = os.path.join(_ROOT, "src")
for _p in (_SRC, os.path.join(_SRC, "web")):
    if _p not in sys.path:
        sys.path.insert(0, _p)

_DEFAULT_CONFIG = os.path.join(_ROOT, "lora-meshtonic.toml")
_DATA_DIR = os.path.join(_ROOT, "lora_web")
_SETTINGS_PATH = os.path.join(_DATA_DIR, "web_settings.json")


def _ensure_hackrf_defaults() -> None:
    """First-run: prefer SoapySDR + HackRF at 20 Msps for Meshtonic workflows."""
    os.makedirs(_DATA_DIR, exist_ok=True)
    try:
        import meshtonic_pp as mpp
    except ImportError:
        return
    cur: dict = {}
    try:
        with open(_SETTINGS_PATH, encoding="utf-8") as f:
            cur = json.load(f)
    except Exception:
        pass
    if cur.get("meshtonic_hackrf_init"):
        return
    patched = mpp.apply_hackrf_defaults(cur)
    patched["meshtonic_hackrf_init"] = True
    with open(_SETTINGS_PATH, "w", encoding="utf-8") as f:
        json.dump(patched, f, indent=2)
    mpp.save_settings(_DATA_DIR, {
        "esp_url": "http://192.168.4.1",
        "preset_id": "US915-meshtastic",
        "bridge_enabled": True,
        "apply_preset_on_start": True,
    })


def main() -> int:
    if not os.environ.get("LORA_CONFIG") and os.path.isfile(_DEFAULT_CONFIG):
        os.environ["LORA_CONFIG"] = _DEFAULT_CONFIG
    os.environ.setdefault("MESHTONIC_STANDALONE", "1")

    _ensure_hackrf_defaults()

    import argparse
    import runpy

    ap = argparse.ArgumentParser(description="Meshtonic MDK standalone (LWD + HackRF + ESP32PP)")
    ap.add_argument("--config", default=None, help="lora.toml path (default: lora-meshtonic.toml)")
    ap.add_argument("--host", default=None)
    ap.add_argument("--port", type=int, default=None)
    ap.add_argument("--no-browser", action="store_true")
    ap.add_argument("--debug", action="store_true")
    args, _rest = ap.parse_known_args()

    if args.config:
        os.environ["LORA_CONFIG"] = args.config

    host = args.host or "127.0.0.1"
    port = args.port or 5000

    if not args.no_browser:
        url = "http://%s:%d/#meshtonic" % (host if host != "0.0.0.0" else "127.0.0.1", port)

        def _open():
            time.sleep(1.8)
            try:
                webbrowser.open(url)
            except Exception:
                print("Open in browser:", url)

        threading.Thread(target=_open, daemon=True).start()

    argv = ["meshtonic_app"]
    if args.config:
        argv += ["--config", args.config]
    if args.host:
        argv += ["--host", args.host]
    if args.port:
        argv += ["--port", str(args.port)]
    if args.debug:
        argv += ["--debug"]
    sys.argv = argv

    runpy.run_path(os.path.join(_SRC, "web", "app.py"), run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
