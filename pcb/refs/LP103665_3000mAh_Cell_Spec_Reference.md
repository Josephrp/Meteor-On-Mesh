# LP103665 / 103665 — 3.7 V 3000 mAh LiPo cell family reference

This is a **cell-format reference** for the pouch size commonly sold as Heltec / Meshtastic 3000 mAh packs (including AliExpress `1005008721775874`). It is **not** a certificate for any specific AliExpress shipment.

## Cell code meaning

| Digits | Meaning | Typical value |
| --- | --- | --- |
| `10` | Thickness (mm) | 10.0 mm |
| `36` | Width (mm) | 36.0 mm |
| `65` | Length (mm) | 65.0 mm |

Model names: **LP103665**, **PL103665**, **103665**, **FT103665P**.

## Electrical (industry typical for 3000 mAh 1S pouch + PCM)

| Parameter | Typical value | Notes |
| --- | --- | --- |
| Chemistry | 1S Li-ion / LiPo | Single-cell pouch |
| Nominal voltage | **3.7 V** | |
| Nominal capacity | **3000 mAh** | Verify printed label on received pack |
| Energy | **~11.1 Wh** | 3.7 V × 3 Ah |
| Charge voltage (max) | **4.20 V** | CC/CV; PCM may cut earlier |
| Discharge cut-off | **2.75–3.0 V** | PCM-dependent |
| Standard charge rate | **0.2C–0.5C** | 600–1500 mA for 3000 mAh |
| Max continuous charge | **1C** | 3000 mA — do not exceed without label proof |
| Standard discharge | **0.2C** | 600 mA |
| Max continuous discharge | **1C** | 3000 mA typical PCM limit |
| Peak / pulse discharge | **2C** | Some packs; verify label |
| Cycle life | **300–500** cycles to ≥80% | At 25 °C, 0.5C charge/discharge |
| Impedance | **≤50–180 mΩ** | Pack-level, vendor-dependent |

## Mechanical (bare cell vs pack)

| Item | Dimension | Notes |
| --- | --- | --- |
| Cell (T × W × L) | **10 × 36 × 65 mm** | Nominal bare pouch |
| Pack with PCM | **10 × 36 × 67 mm** | PCM adds ~1–2 mm to length |
| Weight | **~45–55 g** | With PCM and leads |
| Lead length | **50–150 mm** | Vendor-dependent |
| PCM | **Required** | OVP, UVP, over-current; some add NTC |

## Connector (often customized)

| Type | Pitch | Common on |
| --- | --- | --- |
| **JST PH** 2-pin | **2.0 mm** | Adafruit, SparkFun, many Meshtastic 3000 mAh packs, Meshtonic `J12` |
| **JST SH / “Micro JST 1.25”** 2-pin | **1.25 mm** | Heltec V3/V4 on-board battery socket, LilyGo T-Deck |

**Polarity is not standardized across vendors.** Always verify with a multimeter before first plug-in.

## Temperature and storage

| Mode | Range (typical) |
| --- | --- |
| Charge | **0 °C to +45 °C** |
| Discharge | **−20 °C to +60 °C** |
| Storage (short) | **−10 °C to +45 °C**, 45–75% RH |
| Long storage | Charge to **40–50%** SOC; cool dry place |

## Safety (pouch cell)

- No user-accessible safety vent (unlike some cylindrical cells). Failure mode is **swelling**, heat, or venting at seam — retire swollen packs.
- Do not puncture, crush, short, or charge unattended on flammable surfaces.
- PCM is mandatory for this project; bare unprotected cells are not acceptable on `J12`.

## Sources (cell family, not AliExpress listing)

| Source | URL |
| --- | --- |
| DNK Power — LP103665 3000 mAh product page | [https://www.dnkpower.com/products/103665-3-7v-3000mah-lithium-polymer-battery/](https://www.dnkpower.com/products/103665-3-7v-3000mah-lithium-polymer-battery/) |
| FelloTech — FT103665P 3000 mAh | [https://www.fellotech.com/3-7v-3000mah-103665-lipo-battery_p402.html](https://www.fellotech.com/3-7v-3000mah-103665-lipo-battery_p402.html) |
| SUJOR — 103665 3000 mAh | [https://www.sujorbattery.com/high-energy-density-battery/103665-3000mah-3-7v-lithium-polymer-battery-for-safety-helmet.html](https://www.sujorbattery.com/high-energy-density-battery/103665-3000mah-3-7v-lithium-polymer-battery-for-safety-helmet.html) |
| Akyga LP103665 distributor listing (AKY0944) | [https://elektronik.ropla.eu/en/magazyn/magazyn/?ic=AKY0944](https://elektronik.ropla.eu/en/magazyn/magazyn/?ic=AKY0944) |
| JST PH 2.0 mm connector guide | [https://keszoox.com/blogs/news/jst-ph-connector-complete-guide](https://keszoox.com/blogs/news/jst-ph-connector-complete-guide) |
