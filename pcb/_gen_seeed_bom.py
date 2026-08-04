"""Generate Seeed Fusion PCBA BOM for meshtonic_h4m_v2."""
from __future__ import annotations

import csv
from pathlib import Path

OUT = Path(r"c:\Users\MeMyself\meshtonic\pcb\meshtonic_h4m_v2.csv")
EXCLUDED_OUT = Path(r"c:\Users\MeMyself\meshtonic\pcb\meshtonic_h4m_v2_excluded_from_pcba.csv")

# Seeed Fusion columns only. Prefer Seeed OPL SKU when package matches.
# Designators: comma-separated, no spaces (Seeed parser preference).
ROWS: list[tuple[str, str, int, str]] = [
    # Designator, MPN or Seeed SKU, Qty, Link
    # Polar 0603 tantalum per schematic (CAP-13210 / Kyocera AVX TAC L-case)
    (
        "C1,C2,C8,C10,C12,C14",
        "TACL106M006XTA",
        6,
        "https://www.digikey.com/en/products/detail/kyocera-avx/TACL106M006XTA/3885426",
    ),
    # Murata ordering code includes packaging suffix D (φ180mm paper reel)
    (
        "C3,C4,C5",
        "GRM188R60J226MEA0D",
        3,
        "https://www.digikey.com/en/products/detail/murata-electronics/GRM188R60J226MEA0D/4280542",
    ),
    (
        "C6,C7,C9,C11,C13,C15,C18",
        "302010165",
        7,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "C16,C17",
        "302010138",
        2,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "D1",
        "XZMDK68W-2",
        1,
        "https://www.sunledusa.com/products/spec/XZMDK68W-2.pdf",
    ),
    # THT DO-41 on PCB (pitch 10.16mm) — not OPL SMD DO-214AC
    (
        "D2,D3",
        "1N5819-E3/54",
        2,
        "https://www.digikey.com/en/products/detail/vishay-general-semiconductor-diodes-division/1N5819-E3-54/2799665",
    ),
    (
        "D4,D5",
        "IN-S126ATG",
        2,
        "https://www.inolux-corp.com/datasheet/SMDLED/Mono%20Color%20Top%20View/IN-S126AT%20Series_V1.0.pdf",
    ),
    (
        "D6,D7,D8",
        "304090042",
        3,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "J1,J3,J4,J5,J6,J7",
        "320110033",
        6,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "J2",
        "320110034",
        1,
        "https://www.seeedstudio.com/opl.html",
    ),
    # Seeed OPL SKU 320110030 = MPN 1125S-SMT-4P (Grove SMD 4P 2.0mm vertical)
    (
        "J8,J_AS1,J_BMI1,J_BMM1,J_BMP1,J_SHT1",
        "1125S-SMT-4P",
        6,
        "https://statics3.seeedstudio.com/fusion/opl/datasheet/320110030.pdf",
    ),
    (
        "J9,J10,J11,JP_H4M_3V1,JP_VBAT_H4M1,J13,SC1",
        "320020016",
        7,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "J12",
        "S2B-PH-SM4-TB(LF)(SN)",
        1,
        "https://www.digikey.com/en/products/detail/jst-sales-america-inc/S2B-PH-SM4-TB-LF-SN/926633",
    ),
    # Seeed OPL SKU 320090008 = MPN TF-01 (BEST). Not XKTF-015-G.
    (
        "J14",
        "TF-01",
        1,
        "https://statics3.seeedstudio.com/fusion/opl/datasheet/320090008.pdf",
    ),
    (
        "J15",
        "68021-212HLF",
        1,
        "https://www.digikey.com/en/products/detail/amphenol-icc-fci/68021-212HLF/1535637",
    ),
    (
        "J_DISP1",
        "PRPC009SAAN-RC",
        1,
        "https://www.digikey.com/en/products/detail/sullins-connector-solutions/PRPC009SAAN-RC/2775213",
    ),
    # L1 value 1.5uH matches TPS63020; was wrongly linked to 1.0uH datasheet
    (
        "L1",
        "MBKK2012T1R5M",
        1,
        "https://www.digikey.com/en/products/detail/taiyo-yuden/MBKK2012T1R5M/5035317",
    ),
    (
        "R1,R_H4M_SCL1,R_H4M_SDA1",
        "RC0402JR-0733RL",
        3,
        "https://www.digikey.com/en/products/detail/yageo/RC0402JR-0733RL/727310",
    ),
    (
        "R2",
        "301010006",
        1,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "R3,R7",
        "301010291",
        2,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "R4",
        "RC0402JR-07560KL",
        1,
        "https://www.digikey.com/en/products/detail/yageo/RC0402JR-07560KL/5918752",
    ),
    (
        "R5",
        "RC0402JR-072KL",
        1,
        "https://www.digikey.com/en/products/detail/yageo/RC0402JR-072KL/5918740",
    ),
    (
        "R6",
        "301010089",
        1,
        "https://www.seeedstudio.com/opl.html",
    ),
    # R8 = I2C series 0R; R34 = TFT_BL always-on jumper to 3V3 (firmware BL=-1)
    (
        "R8,R34",
        "301010292",
        2,
        "https://www.seeedstudio.com/opl.html",
    ),
    # R35 joins existing CS_TFT pull-up R37 (parallel 10k)
    (
        "R9,R10,R11,R12,R13,R14,R15,R16,R17,R18,R19,R20,R21,R22,R23,R24,R25,R26,R27,R28,R29,R30,R31,R32,R33,R35,R36,R37,R38,R39,R40,R41,R45",
        "301010293",
        33,
        "https://www.seeedstudio.com/opl.html",
    ),
    (
        "R42,R43,R44",
        "301010300",
        3,
        "https://www.seeedstudio.com/opl.html",
    ),
    # WA-SMSI M3 same land (4.2mm ID / 6mm OD); 3.0mm tall (was 2.7mm 9774027360R, unavailable)
    (
        "ST1,ST2,ST3",
        "9774030360R",
        3,
        "https://www.digikey.com/en/products/detail/w%C3%BCrth-elektronik/9774030360R/4810228",
    ),
    (
        "U1",
        "TPS63020DSJR",
        1,
        "https://www.ti.com/product/TPS63020",
    ),
    (
        "U2",
        "CN3170",
        1,
        "http://www.consonance-elec.com/en/static/upload/file/20221107/1667805770192920.pdf",
    ),
    (
        "U3",
        "102010671",
        1,
        "https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32S3-Plus-p-6361.html",
    ),
    (
        "U4",
        "TCA9548APWR",
        1,
        "https://www.ti.com/product/TCA9548A",
    ),
    (
        "U5",
        "MCP23017-E/SS",
        1,
        "https://www.microchip.com/en-us/product/mcp23017",
    ),
]

EXCLUDED: list[tuple[str, str, str]] = [
    (
        "TP1,TP_H4M_3V1,TP_H4M_SCL1,TP_H4M_SDA1,TP_VBAT_H4M1",
        "Test points",
        "Seeed: do not include test points in PCBA BOM",
    ),
    (
        "J_WIO1,J_WIO2,J_WIO3,J_WIO4",
        "XIAO-ESP32-S3-DIP hybrid SMD+TH landing (15.24mm row spacing); not a 2.54mm dual-row header",
        "Hand-fit 2x 1x7 female headers 2.54mm (e.g. PPPC071LFBN-RC) per site, or SMD-solder Wio modules",
    ),
    (
        "DISP1,WIO1,WIO2,WIO3,WIO4,ST4",
        "Excluded from board in schematic",
        "Not fabricated / not assembled",
    ),
    (
        "J16,J17,J18,J19,J20,J21,J22,J23",
        "DNP / excluded from board",
        "Not for Fusion purchase",
    ),
]


def validate(rows: list[tuple[str, str, int, str]]) -> None:
    seen: set[str] = set()
    for des, mpn, qty, link in rows:
        assert mpn.strip(), f"empty MPN for {des}"
        assert qty > 0, des
        assert link.strip(), des
        refs = [r.strip() for r in des.split(",") if r.strip()]
        assert len(refs) == qty, f"{des}: designator count {len(refs)} != qty {qty}"
        for r in refs:
            assert r not in seen, f"duplicate designator {r}"
            seen.add(r)
            assert ";" not in r and "-" not in r[1:], f"bad designator format: {r}"
    print(f"OK: {len(rows)} lines, {len(seen)} designators")


def main() -> None:
    validate(ROWS)
    with OUT.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "Designator",
                "Manufacturer Part Number or Seeed SKU",
                "Qty",
                "Link",
            ]
        )
        for des, mpn, qty, link in ROWS:
            w.writerow([des, mpn, qty, link])

    with EXCLUDED_OUT.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Reason", "Action"])
        for row in EXCLUDED:
            w.writerow(row)

    print(f"Wrote {OUT}")
    print(f"Wrote {EXCLUDED_OUT}")


if __name__ == "__main__":
    main()
