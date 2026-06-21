"""Load band presets from presets.toml (shared with firmware lora_bands.h)."""

from __future__ import annotations

import os
from typing import Any

try:
    import tomllib
except ImportError:
    import tomli as tomllib  # type: ignore

_PRESETS_CACHE: list[dict[str, Any]] | None = None


def _presets_path() -> str:
    return os.path.join(os.path.dirname(__file__), "..", "..", "presets.toml")


def load_presets() -> list[dict[str, Any]]:
    global _PRESETS_CACHE
    if _PRESETS_CACHE is not None:
        return _PRESETS_CACHE
    path = os.path.normpath(_presets_path())
    with open(path, "rb") as f:
        data = tomllib.load(f)
    _PRESETS_CACHE = list(data.get("preset", []))
    return _PRESETS_CACHE


def find_preset(preset_id: str) -> dict[str, Any] | None:
    pid = (preset_id or "").strip()
    if not pid:
        return None
    for p in load_presets():
        if p.get("id", "").lower() == pid.lower():
            return p
    return None


def region_from_freq(freq_mhz: float) -> str:
    if 902.0 <= freq_mhz <= 928.0:
        return "US915"
    if 869.4 <= freq_mhz <= 869.65:
        return "EU868"
    if 433.0 <= freq_mhz <= 435.0:
        return "EU433"
    if 470.0 <= freq_mhz <= 510.0:
        return "CN470"
    if 920.0 <= freq_mhz <= 928.0:
        return "JP"
    if 920.0 <= freq_mhz <= 923.5:
        return "KR"
    if 865.0 <= freq_mhz <= 867.0:
        return "IN865"
    if 915.0 <= freq_mhz <= 928.0:
        return "ANZ"
    if 917.0 <= freq_mhz <= 925.0:
        return "AS923"
    if 863.0 <= freq_mhz <= 868.0:
        return "NZ865"
    return "CUSTOM"


def apply_preset_channels(preset_id: str) -> list[dict[str, Any]]:
    """Return list of 4 channel dicts {freq_mhz, sf, bw_hz, cr, region, profile, preset_id}."""
    p = find_preset(preset_id)
    if not p:
        return []
    freqs = list(p.get("slot_freqs_mhz", [915.0]))
    while len(freqs) < 4:
        freqs.append(freqs[-1] if freqs else 915.0)
    chans = []
    for i in range(4):
        chans.append({
            "slot": i,
            "freq_mhz": freqs[i],
            "sf": int(p["sf"]),
            "bw_hz": int(p["bw_hz"]),
            "cr": int(p.get("cr", 5)),
            "region": p["region"],
            "profile": p["profile"],
            "preset_id": p["id"],
        })
    return chans
