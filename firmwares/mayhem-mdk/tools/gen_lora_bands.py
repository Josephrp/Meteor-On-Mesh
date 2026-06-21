#!/usr/bin/env python3
"""Generate lora_bands.h / lora_bands.cpp from lora-wideband-decoder/presets.toml."""

from __future__ import annotations

import os
import re
import sys

try:
    import tomllib
except ImportError:
    import tomli as tomllib  # type: ignore

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS_PATH = os.path.join(ROOT, "lora-wideband-decoder", "presets.toml")
OUT_H = os.path.join(ROOT, "Source", "main", "lora_bands.h")
OUT_CPP = os.path.join(ROOT, "Source", "main", "lora_bands.cpp")


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def load_presets():
    with open(PRESETS_PATH, "rb") as f:
        data = tomllib.load(f)
    return data.get("preset", [])


def gen_header(presets) -> str:
    lines = [
        "#pragma once",
        "/* AUTO-GENERATED from lora-wideband-decoder/presets.toml — do not edit */",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "struct RadioChannel;",
        "struct LoraDecoderConfig;",
        "",
        "constexpr int LORA_MAX_PRESETS = %d;" % len(presets),
        "constexpr int LORA_MAX_SLOTS = 4;",
        "",
        "struct LoraBandPreset {",
        "    const char* id;",
        "    const char* region;",
        "    const char* profile;",
        "    uint8_t sf;",
        "    uint32_t bw_hz;",
        "    uint8_t cr;",
        "    float slot_freqs_mhz[LORA_MAX_SLOTS];",
        "    uint8_t slot_count;",
        "    float sdr_center_mhz;",
        "    const char* lorawan_region;",
        "};",
        "",
        "extern const LoraBandPreset g_lora_presets[LORA_MAX_PRESETS];",
        "",
        "const LoraBandPreset* lora_find_preset(const char* id);",
        "const char* lora_region_from_freq(float freq_mhz);",
        "bool lora_apply_preset_to_config(LoraDecoderConfig* cfg, const char* preset_id);",
        "bool lora_apply_preset_to_slot(RadioChannel* ch, const char* preset_id, int slot);",
        "size_t lora_list_preset_ids(char* buf, size_t cap);",
        "size_t lora_list_presets_json(char* buf, size_t cap);",
        "",
    ]
    return "\n".join(lines)


def gen_cpp(presets) -> str:
    entries = []
    for p in presets:
        freqs = p.get("slot_freqs_mhz", [915.0])
        while len(freqs) < 4:
            freqs.append(freqs[-1] if freqs else 915.0)
        freqs = freqs[:4]
        lw = p.get("lorawan_region") or ""
        entries.append(
            '    {"%s", "%s", "%s", %u, %u, %u, '
            "{%.6ff, %.6ff, %.6ff, %.6ff}, %u, %.3ff, \"%s\"},"
            % (
                c_escape(p["id"]),
                c_escape(p["region"]),
                c_escape(p["profile"]),
                int(p["sf"]),
                int(p["bw_hz"]),
                int(p.get("cr", 5)),
                freqs[0], freqs[1], freqs[2], freqs[3],
                len(p.get("slot_freqs_mhz", freqs)),
                float(p.get("sdr_center_mhz", 915.0)),
                c_escape(lw),
            )
        )

    cpp = [
        "/* AUTO-GENERATED from presets.toml */",
        '#include "lora_bands.h"',
        '#include "apps/ep_app_loradecoder.hpp"',
        "#include <cstring>",
        "#include <cstdio>",
        "",
        "const LoraBandPreset g_lora_presets[LORA_MAX_PRESETS] = {",
        *entries,
        "};",
        "",
        "static bool streq_ci(const char* a, const char* b) {",
        "    if (!a || !b) return false;",
        "    while (*a && *b) {",
        "        char ca = *a, cb = *b;",
        "        if (ca >= 'A' && ca <= 'Z') ca += 32;",
        "        if (cb >= 'A' && cb <= 'Z') cb += 32;",
        "        if (ca != cb) return false;",
        "        a++; b++;",
        "    }",
        "    return *a == *b;",
        "}",
        "",
        "const LoraBandPreset* lora_find_preset(const char* id) {",
        "    if (!id || !id[0]) return nullptr;",
        "    for (int i = 0; i < LORA_MAX_PRESETS; i++) {",
        "        if (strcmp(g_lora_presets[i].id, id) == 0) return &g_lora_presets[i];",
        "    }",
        "  for (int i = 0; i < LORA_MAX_PRESETS; i++) {",
        "        if (streq_ci(g_lora_presets[i].id, id)) return &g_lora_presets[i];",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "const char* lora_region_from_freq(float f) {",
        "    if (f >= 902.0f && f <= 928.0f) return \"US915\";",
        "    if (f >= 869.4f && f <= 869.65f) return \"EU868\";",
        "    if (f >= 433.0f && f <= 435.0f) return \"EU433\";",
        "    if (f >= 470.0f && f <= 510.0f) return \"CN470\";",
        "    if (f >= 920.0f && f <= 928.0f) return \"JP\";",
        "    if (f >= 920.0f && f <= 923.5f) return \"KR\";",
        "    if (f >= 865.0f && f <= 867.0f) return \"IN865\";",
        "    if (f >= 915.0f && f <= 928.0f) return \"ANZ\";",
        "    if (f >= 917.0f && f <= 925.0f) return \"AS923\";",
        "    if (f >= 863.0f && f <= 868.0f) return \"NZ865\";",
        "    return \"CUSTOM\";",
        "}",
        "",
        "bool lora_apply_preset_to_slot(RadioChannel* ch, const char* preset_id, int slot) {",
        "    const LoraBandPreset* p = lora_find_preset(preset_id);",
        "    if (!p || !ch || slot < 0 || slot >= LORA_MAX_SLOTS) return false;",
        "    ch->freq_mhz = p->slot_freqs_mhz[slot];",
        "    ch->sf = p->sf;",
        "    ch->bw_hz = p->bw_hz;",
        "    ch->cr = p->cr;",
        "    strncpy(ch->region, p->region, sizeof(ch->region) - 1);",
        "    strncpy(ch->profile, p->profile, sizeof(ch->profile) - 1);",
        "    strncpy(ch->preset_id, p->id, sizeof(ch->preset_id) - 1);",
        "    return true;",
        "}",
        "",
        "bool lora_apply_preset_to_config(LoraDecoderConfig* cfg, const char* preset_id) {",
        "    const LoraBandPreset* p = lora_find_preset(preset_id);",
        "    if (!p || !cfg) return false;",
        "    cfg->center_mhz = p->sdr_center_mhz;",
        "    int n = p->slot_count < LORA_MAX_SLOTS ? p->slot_count : LORA_MAX_SLOTS;",
        "    for (int i = 0; i < n; i++) {",
        "        lora_apply_preset_to_slot(&cfg->channels[i], preset_id, i);",
        "    }",
        "    strncpy(cfg->active_preset_id, p->id, sizeof(cfg->active_preset_id) - 1);",
        "    return true;",
        "}",
        "",
        "size_t lora_list_preset_ids(char* buf, size_t cap) {",
        "    if (!buf || cap == 0) return 0;",
        "    size_t off = 0;",
        "    for (int i = 0; i < LORA_MAX_PRESETS; i++) {",
        "        int n = snprintf(buf + off, cap - off, \"%s%s\", (i ? \",\" : \"\"), g_lora_presets[i].id);",
        "        if (n < 0 || (size_t)n >= cap - off) break;",
        "        off += (size_t)n;",
        "    }",
        "    return off;",
        "}",
        "",
        "size_t lora_list_presets_json(char* buf, size_t cap) {",
        "    if (!buf || cap < 4) return 0;",
        "    size_t off = 0;",
        "    off += (size_t)snprintf(buf + off, cap - off, \"[\");",
        "    for (int i = 0; i < LORA_MAX_PRESETS; i++) {",
        "        const LoraBandPreset* p = &g_lora_presets[i];",
        "        int n = snprintf(buf + off, cap - off,",
        "            \"%s{\\\"id\\\":\\\"%s\\\",\\\"region\\\":\\\"%s\\\",\\\"profile\\\":\\\"%s\\\",\\\"sf\\\":%u,\\\"bw\\\":%u}\",",
        "            (i ? \",\" : \"\"), p->id, p->region, p->profile, p->sf, (unsigned)p->bw_hz);",
        "        if (n < 0 || (size_t)n >= cap - off) break;",
        "        off += (size_t)n;",
        "    }",
        "    snprintf(buf + off, cap - off, \"]\");",
        "    return strlen(buf);",
        "}",
        "",
    ]
    return "\n".join(cpp)


def main():
    presets = load_presets()
    if not presets:
        print("No presets found", file=sys.stderr)
        return 1
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write(gen_header(presets))
    with open(OUT_CPP, "w", encoding="utf-8") as f:
        f.write(gen_cpp(presets))
    print("Wrote %s and %s (%d presets)" % (OUT_H, OUT_CPP, len(presets)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
