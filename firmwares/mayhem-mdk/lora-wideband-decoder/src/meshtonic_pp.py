"""Meshtonic MDK / ESP32PP integration for the LWD host web app.

Bridges decoded packets to the ESP HTTP API and sends LORA:* preset commands.
"""

from __future__ import annotations

import json
import os
import threading
import time
from typing import Any
from urllib.parse import urlparse

try:
    import requests
except ImportError:
    requests = None  # type: ignore

_HERE = os.path.dirname(os.path.abspath(__file__))
_LWD_ROOT = os.path.dirname(_HERE)
_PRESETS_TOML = os.path.join(_LWD_ROOT, "presets.toml")

PROTO_MAP = {
    "meshtastic": 1,
    "meshcore": 2,
    "lorawan": 3,
    "loramesher": 4,
    "lora_aprs": 5,
    "reticulum": 6,
    "disaster_radio": 7,
    "radiohead": 8,
    "ebyte_lora": 9,
}

_SETTINGS_LOCK = threading.Lock()
_BRIDGE_LOCK = threading.Lock()
_LAST_FWD: set[tuple] = set()
_MAX_FWD_CACHE = 5000

_DEFAULTS: dict[str, Any] = {
    "esp_url": "http://192.168.4.1",
    "preset_id": "US915-meshtastic",
    "bridge_enabled": True,
    "apply_preset_on_start": True,
    "open_esp_ui": True,
}


def settings_path(data_dir: str) -> str:
    return os.path.join(data_dir, "meshtonic_settings.json")


def load_settings(data_dir: str) -> dict[str, Any]:
    path = settings_path(data_dir)
    with _SETTINGS_LOCK:
        try:
            with open(path, encoding="utf-8") as f:
                return {**_DEFAULTS, **json.load(f)}
        except Exception:
            return dict(_DEFAULTS)


def save_settings(data_dir: str, patch: dict[str, Any]) -> dict[str, Any]:
    cur = load_settings(data_dir)
    cur.update({k: v for k, v in patch.items() if k in _DEFAULTS or k in cur})
    path = settings_path(data_dir)
    with _SETTINGS_LOCK:
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(cur, f, indent=2)
        except Exception:
            pass
    return cur


def list_presets() -> list[dict[str, Any]]:
    try:
        from lora.presets import load_presets

        return load_presets()
    except Exception:
        return []


def _packet_url(esp_url: str) -> str:
    return esp_url.rstrip("/") + "/lwd/packet"


def _cmd_url(esp_url: str) -> str:
    return esp_url.rstrip("/") + "/lwd/cmd"


def _presets_url(esp_url: str) -> str:
    return esp_url.rstrip("/") + "/lwd/presets"


def record_to_esp_payload(event: dict[str, Any], preset_id: str = "") -> dict[str, Any]:
    proto_name = event.get("proto", "unknown")
    if isinstance(proto_name, str):
        proto_id = PROTO_MAP.get(proto_name, 0)
    else:
        proto_id = int(proto_name or 0)

    freq = event.get("freq") or event.get("freq_mhz")
    bw = event.get("bw") or event.get("bw_hz")
    region = event.get("region") or event.get("band") or ""

    return {
        "ts": event.get("ts") or int(time.time() * 1000),
        "freq": freq,
        "bw": bw,
        "sf": event.get("sf"),
        "cr": event.get("cr"),
        "rssi": event.get("rssi"),
        "snr": event.get("snr"),
        "slot": event.get("slot"),
        "region": region,
        "profile": event.get("profile") or "",
        "preset_id": event.get("preset_id") or preset_id,
        "band": event.get("band") or region,
        "proto": proto_id,
        "confidence": event.get("confidence", "candidate"),
        "decrypted": event.get("decrypted", False),
        "payload_hex": event.get("payload_hex") or event.get("raw_hex") or event.get("payload"),
        "info": event.get("info") or event.get("text") or event.get("port_name") or "",
        "key_label": event.get("key_label", ""),
        "decode_backend": event.get("decode_backend", "host_wideband"),
        "sync": event.get("sync"),
    }


def forward_packet_to_esp(esp_url: str, event: dict[str, Any], preset_id: str = "") -> bool:
    if not requests:
        return False
    payload = record_to_esp_payload(event, preset_id)
    try:
        r = requests.post(_packet_url(esp_url), json=payload, timeout=2)
        return r.status_code < 300
    except Exception:
        return False


def maybe_forward_packet(event: dict[str, Any], data_dir: str) -> None:
    cfg = load_settings(data_dir)
    if not cfg.get("bridge_enabled"):
        return
    esp = (cfg.get("esp_url") or "").strip()
    if not esp or not requests:
        return
    if event.get("pktid") is not None:
        key = ("pkt", event.get("pktid"), event.get("hops"))
    else:
        key = ("raw", event.get("proto"), event.get("raw_hex"))
    with _BRIDGE_LOCK:
        if key in _LAST_FWD:
            return
        _LAST_FWD.add(key)
        if len(_LAST_FWD) > _MAX_FWD_CACHE:
            _LAST_FWD.clear()
    forward_packet_to_esp(esp, event, cfg.get("preset_id", ""))


def esp_reachable(esp_url: str) -> bool:
    if not requests:
        return False
    try:
        r = requests.get(_presets_url(esp_url), timeout=2)
        return r.status_code < 300
    except Exception:
        return False


def esp_fetch_presets(esp_url: str) -> dict[str, Any] | None:
    if not requests:
        return None
    try:
        r = requests.get(_presets_url(esp_url), timeout=3)
        if r.status_code >= 300:
            return None
        return r.json()
    except Exception:
        return None


def esp_send_cmd(esp_url: str, cmd: str, start_app: bool = True) -> dict[str, Any]:
    if not requests:
        return {"ok": False, "error": "requests package not installed"}
    cmd = (cmd or "").strip()
    if not cmd:
        return {"ok": False, "error": "empty command"}
    body: dict[str, Any] = {"cmd": cmd}
    if start_app:
        body["start_app"] = True
    try:
        r = requests.post(_cmd_url(esp_url), json=body, timeout=4)
        if r.status_code < 300:
            return {"ok": True, "response": r.text.strip()}
        return {"ok": False, "error": "HTTP %d" % r.status_code, "body": r.text[:200]}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def apply_preset_to_esp(esp_url: str, preset_id: str) -> dict[str, Any]:
    preset_id = (preset_id or "").strip()
    if not preset_id:
        return {"ok": False, "error": "no preset_id"}
    out: dict[str, Any] = {"ok": True, "steps": []}
    for cmd in ("LORA:START", "LORA:PRESET:%s" % preset_id):
        res = esp_send_cmd(esp_url, cmd)
        out["steps"].append({"cmd": cmd, **res})
        if not res.get("ok"):
            out["ok"] = False
            out["error"] = res.get("error", "command failed")
            break
        time.sleep(0.15)
    return out


def esp_web_ui_url(esp_url: str) -> str:
    p = urlparse(esp_url)
    host = p.netloc or p.path
    scheme = p.scheme or "http"
    return "%s://%s/" % (scheme, host)


def apply_hackrf_defaults(settings: dict[str, Any]) -> dict[str, Any]:
    """Prefer HackRF via SoapySDR for the Meshtonic standalone app."""
    settings = dict(settings)
    settings["sdr"] = "soapy"
    radio = dict(settings.get("radio") or {})
    radio.setdefault("soapy_driver", "hackrf")
    radio.setdefault("rate_hz", 20_000_000)
    radio.setdefault("bandwidth_hz", 20_000_000)
    radio.setdefault("center_mhz", 915.0)
    radio.setdefault("format", "cs8")
    radio.setdefault("gain", 40)
    settings["radio"] = radio
    return settings
