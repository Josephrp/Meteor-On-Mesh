# AliExpress item 1005008721775874 — Heltec LiPo battery note

Source URL:

- [AliExpress item 1005008721775874](https://www.aliexpress.com/item/1005008721775874.html)

## Identification

| Field | Value |
| --- | --- |
| Listing title (cached) | **Heltec 3.7 V LiPo Battery 3000 mAh / 800 mAh PL103665** — for T114 V2, LoRa 32 V3, LoRa node dev boards |
| Likely cell format | **PL103665 / LP103665** (10 × 36 × 65 mm pouch) when 3000 mAh variant is shipped |
| Alternate SKU on same listing | **800 mAh** smaller pack — same listing ID, different capacity |
| Manufacturer datasheet | **None tied to this AliExpress ID** |
| Cell-family reference | `pcb/refs/LP103665_3000mAh_Cell_Spec_Reference.md` |

**Confidence:** **medium** for 3000 mAh PL103665 identity from listing title and community cross-use; **low** for exact connector type, polarity, PCM thresholds, and true capacity until the received pack is measured and labeled.

## Unverified until received part

- Printed mAh on pouch label (3000 vs 800 vs overstated).
- Connector: **JST PH 2.0 mm** vs **JST SH 1.25 mm** (Heltec boards often use 1.25 mm on-board).
- Wire polarity at connector (vendor-dependent).
- PCM over-current and charge-current limit on protection board.
- True thickness with PCM (67 mm length is common).

## Meshtonic design implications

- `J12` is **JST PH 2.0 mm** horizontal (`PCM_SparkFun-Connector:JST_1x02_P2.0mm_Horizontal_SMD`). If the pack ships **1.25 mm**, use an adapter cable or re-crimp — do not force the wrong pitch.
- **Verify polarity with a meter** before first connection. Meshtonic `J12` PCB pads: **pin 1 = GND**, **pin 2 = VBAT** (SparkFun footprint `-_1` / `+_2`). This matches Adafruit/SparkFun LiPo convention but **differs from `SPEC.md` §14** (which incorrectly states pin 1 = VBAT).
- Board charge current (`CN3170` `R6` = 2.0 kΩ) ≈ **594 mA** — appropriate for **3000 mAh** (~0.2C). If the **800 mAh** variant arrives, **lower `R6`** or do not charge on-board until current is reduced (~160 mA for 0.2C).
- Reserve a **padded battery pocket** (~67 × 38 × 12 mm planning envelope) and **strain relief**; keep cell away from `U1`/`U2` heat and sharp case edges.
- Pouch cells **swell** slightly; do not design a friction-fit cavity from nominal dimensions alone.
