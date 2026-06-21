"""LoRaWAN regional channel grid checks (ported from decoder.py, all regions)."""

from __future__ import annotations

DEFAULT_TOL_KHZ = 60.0


def _near(freq_mhz: float, chans: list[float], tol_khz: float) -> bool:
    tol = tol_khz / 1000.0
    return any(abs(freq_mhz - c) <= tol for c in chans)


def us915_channels(bw_khz: float, sf: int) -> list[float]:
    chans: list[float] = []
    if abs(bw_khz - 125) < 30 and 7 <= sf <= 10:
        chans = [902.3 + 0.2 * n for n in range(64)]
    elif abs(bw_khz - 500) < 100:
        if sf == 8:
            chans += [903.0 + 1.6 * n for n in range(8)]
        if sf in (8, 9, 10, 11, 12):
            chans += [923.3 + 0.6 * n for n in range(8)]
    return chans


def eu868_channels() -> list[float]:
    return [868.1, 868.3, 868.5, 867.1, 867.3, 867.5, 867.7, 867.9]


def cn470_channels() -> list[float]:
    return [470.3 + 0.2 * n for n in range(96)]


def in865_channels() -> list[float]:
    return [865.0625 + 0.18 * n for n in range(4)]


def as923_channels(sub: str = "AS923") -> list[float]:
    if sub.upper() in ("AS2", "AS923-2"):
        return [923.2, 923.4, 923.6, 923.8, 924.0, 924.2, 924.4, 924.6]
    return [923.2, 923.4, 922.2, 922.4, 922.6, 922.8, 923.0, 922.0]


def kr920_channels() -> list[float]:
    return [922.1 + 0.2 * n for n in range(16)]


def on_lorawan_grid(
    freq_mhz: float | None,
    sf: int | None,
    bw_hz: int | None,
    region: str = "US915",
    tol_khz: float = DEFAULT_TOL_KHZ,
) -> bool | None:
    if freq_mhz is None or sf is None or bw_hz is None:
        return None
    bw_khz = bw_hz / 1000.0
    region = (region or "US915").upper()
    chans: list[float] = []
    if region == "US915":
        chans = us915_channels(bw_khz, sf)
    elif region in ("EU868", "EU_868"):
        chans = eu868_channels()
    elif region == "CN470":
        chans = cn470_channels()
    elif region == "IN865":
        chans = in865_channels()
    elif region.startswith("AS923"):
        chans = as923_channels(region)
    elif region == "KR920":
        chans = kr920_channels()
    elif region in ("AU915", "ANZ"):
        chans = us915_channels(bw_khz, sf)  # same grid on 915 band
    else:
        return None
    if not chans:
        return False
    return _near(freq_mhz, chans, tol_khz)
