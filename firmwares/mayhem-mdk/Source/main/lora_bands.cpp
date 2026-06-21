/* AUTO-GENERATED from presets.toml */
#include "lora_bands.h"
#include "apps/ep_app_loradecoder.hpp"
#include <cstring>
#include <cstdio>

const LoraBandPreset g_lora_presets[LORA_MAX_PRESETS] = {
    {"US915-meshtastic", "US915", "meshtastic_longfast", 11, 250000, 5, {906.875000f, 907.125000f, 907.375000f, 907.625000f}, 4, 915.000f, ""},
    {"US-meshcore", "US915", "meshcore_narrow", 7, 62500, 5, {910.525000f, 910.600000f, 910.675000f, 910.750000f}, 4, 915.000f, ""},
    {"US915-lorawan", "US915", "lorawan_us915", 9, 125000, 5, {902.300000f, 902.700000f, 903.100000f, 903.500000f}, 4, 915.000f, "US915"},
    {"EU868-meshtastic", "EU868", "meshtastic_longfast", 11, 250000, 5, {869.525000f, 869.525000f, 869.525000f, 869.525000f}, 4, 868.000f, ""},
    {"EU-meshcore", "EU868", "meshcore_narrow", 7, 62500, 5, {869.525000f, 869.618000f, 869.525000f, 869.618000f}, 4, 868.000f, ""},
    {"EU868-lorawan", "EU868", "lorawan_eu868", 9, 125000, 5, {868.100000f, 868.300000f, 868.500000f, 867.100000f}, 4, 868.000f, "EU868"},
    {"EU433-meshtastic", "EU433", "meshtastic_longfast", 11, 250000, 5, {433.875000f, 433.875000f, 433.875000f, 433.875000f}, 4, 433.000f, ""},
    {"CN470-meshtastic", "CN470", "meshtastic_longfast", 11, 250000, 5, {486.300000f, 486.500000f, 486.700000f, 486.900000f}, 4, 490.000f, ""},
    {"CN470-lorawan", "CN470", "lorawan_cn470", 9, 125000, 5, {486.300000f, 486.700000f, 487.100000f, 487.500000f}, 4, 490.000f, "CN470"},
    {"JP-meshtastic", "JP", "meshtastic_longfast", 11, 250000, 5, {923.500000f, 923.750000f, 924.000000f, 924.250000f}, 4, 924.000f, ""},
    {"KR-meshtastic", "KR", "meshtastic_longfast", 11, 250000, 5, {921.900000f, 922.100000f, 922.300000f, 922.500000f}, 4, 922.000f, ""},
    {"IN865-meshtastic", "IN865", "meshtastic_longfast", 11, 250000, 5, {865.062500f, 865.242500f, 865.422500f, 865.602500f}, 4, 865.500f, ""},
    {"IN865-lorawan", "IN865", "lorawan_in865", 9, 125000, 5, {865.062500f, 865.402500f, 865.742500f, 866.082500f}, 4, 865.500f, "IN865"},
    {"ANZ-meshtastic", "ANZ", "meshtastic_longfast", 11, 250000, 5, {915.000000f, 915.250000f, 915.500000f, 915.750000f}, 4, 921.000f, ""},
    {"AU915-meshcore", "ANZ", "meshcore_narrow", 7, 62500, 5, {915.000000f, 915.075000f, 915.150000f, 915.225000f}, 4, 921.000f, ""},
    {"AS923-meshtastic", "AS923", "meshtastic_longfast", 11, 250000, 5, {923.200000f, 923.400000f, 922.200000f, 922.400000f}, 4, 923.000f, ""},
    {"AS923-lorawan", "AS923", "lorawan_as923", 9, 125000, 5, {923.200000f, 923.400000f, 923.600000f, 923.800000f}, 4, 923.000f, "AS923"},
    {"SG923-meshtastic", "SG923", "meshtastic_longfast", 11, 250000, 5, {917.500000f, 917.750000f, 918.000000f, 918.250000f}, 4, 920.000f, ""},
    {"NZ865-meshtastic", "NZ865", "meshtastic_longfast", 11, 250000, 5, {864.500000f, 864.750000f, 865.000000f, 865.250000f}, 4, 865.000f, ""},
    {"RU-meshtastic", "RU", "meshtastic_longfast", 11, 250000, 5, {868.950000f, 869.000000f, 869.050000f, 869.100000f}, 4, 869.000f, ""},
    {"TW-meshtastic", "TW", "meshtastic_longfast", 11, 250000, 5, {922.500000f, 922.750000f, 923.000000f, 923.250000f}, 4, 923.000f, ""},
    {"TH-meshtastic", "TH", "meshtastic_longfast", 11, 250000, 5, {922.500000f, 922.750000f, 923.000000f, 923.250000f}, 4, 923.000f, ""},
    {"MY919-meshtastic", "MY919", "meshtastic_longfast", 11, 250000, 5, {921.500000f, 921.750000f, 922.000000f, 922.250000f}, 4, 922.000f, ""},
    {"PH915-meshtastic", "PH915", "meshtastic_longfast", 11, 250000, 5, {916.500000f, 916.750000f, 917.000000f, 917.250000f}, 4, 917.000f, ""},
    {"UA868-meshtastic", "UA868", "meshtastic_longfast", 11, 250000, 5, {868.300000f, 868.350000f, 868.400000f, 868.450000f}, 4, 868.000f, ""},
    {"UA433-meshtastic", "UA433", "meshtastic_longfast", 11, 250000, 5, {433.500000f, 433.550000f, 433.600000f, 433.650000f}, 4, 433.000f, ""},
    {"BR902-meshtastic", "BR902", "meshtastic_longfast", 11, 250000, 5, {903.500000f, 903.750000f, 904.000000f, 904.250000f}, 4, 905.000f, ""},
    {"KZ863-meshtastic", "KZ863", "meshtastic_longfast", 11, 250000, 5, {865.500000f, 865.750000f, 866.000000f, 866.250000f}, 4, 866.000f, ""},
    {"NP865-meshtastic", "NP865", "meshtastic_longfast", 11, 250000, 5, {866.500000f, 866.750000f, 867.000000f, 867.250000f}, 4, 866.000f, ""},
};

static bool streq_ci(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

const LoraBandPreset* lora_find_preset(const char* id) {
    if (!id || !id[0]) return nullptr;
    for (int i = 0; i < LORA_MAX_PRESETS; i++) {
        if (strcmp(g_lora_presets[i].id, id) == 0) return &g_lora_presets[i];
    }
  for (int i = 0; i < LORA_MAX_PRESETS; i++) {
        if (streq_ci(g_lora_presets[i].id, id)) return &g_lora_presets[i];
    }
    return nullptr;
}

const char* lora_region_from_freq(float f) {
    if (f >= 902.0f && f <= 928.0f) return "US915";
    if (f >= 869.4f && f <= 869.65f) return "EU868";
    if (f >= 433.0f && f <= 435.0f) return "EU433";
    if (f >= 470.0f && f <= 510.0f) return "CN470";
    if (f >= 920.0f && f <= 928.0f) return "JP";
    if (f >= 920.0f && f <= 923.5f) return "KR";
    if (f >= 865.0f && f <= 867.0f) return "IN865";
    if (f >= 915.0f && f <= 928.0f) return "ANZ";
    if (f >= 917.0f && f <= 925.0f) return "AS923";
    if (f >= 863.0f && f <= 868.0f) return "NZ865";
    return "CUSTOM";
}

bool lora_apply_preset_to_slot(RadioChannel* ch, const char* preset_id, int slot) {
    const LoraBandPreset* p = lora_find_preset(preset_id);
    if (!p || !ch || slot < 0 || slot >= LORA_MAX_SLOTS) return false;
    ch->freq_mhz = p->slot_freqs_mhz[slot];
    ch->sf = p->sf;
    ch->bw_hz = p->bw_hz;
    ch->cr = p->cr;
    strncpy(ch->region, p->region, sizeof(ch->region) - 1);
    strncpy(ch->profile, p->profile, sizeof(ch->profile) - 1);
    strncpy(ch->preset_id, p->id, sizeof(ch->preset_id) - 1);
    return true;
}

bool lora_apply_preset_to_config(LoraDecoderConfig* cfg, const char* preset_id) {
    const LoraBandPreset* p = lora_find_preset(preset_id);
    if (!p || !cfg) return false;
    cfg->center_mhz = p->sdr_center_mhz;
    int n = p->slot_count < LORA_MAX_SLOTS ? p->slot_count : LORA_MAX_SLOTS;
    for (int i = 0; i < n; i++) {
        lora_apply_preset_to_slot(&cfg->channels[i], preset_id, i);
    }
    strncpy(cfg->active_preset_id, p->id, sizeof(cfg->active_preset_id) - 1);
    return true;
}

size_t lora_list_preset_ids(char* buf, size_t cap) {
    if (!buf || cap == 0) return 0;
    size_t off = 0;
    for (int i = 0; i < LORA_MAX_PRESETS; i++) {
        int n = snprintf(buf + off, cap - off, "%s%s", (i ? "," : ""), g_lora_presets[i].id);
        if (n < 0 || (size_t)n >= cap - off) break;
        off += (size_t)n;
    }
    return off;
}

size_t lora_list_presets_json(char* buf, size_t cap) {
    if (!buf || cap < 4) return 0;
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "[");
    for (int i = 0; i < LORA_MAX_PRESETS; i++) {
        const LoraBandPreset* p = &g_lora_presets[i];
        int n = snprintf(buf + off, cap - off,
            "%s{\"id\":\"%s\",\"region\":\"%s\",\"profile\":\"%s\",\"sf\":%u,\"bw\":%u}",
            (i ? "," : ""), p->id, p->region, p->profile, p->sf, (unsigned)p->bw_hz);
        if (n < 0 || (size_t)n >= cap - off) break;
        off += (size_t)n;
    }
    snprintf(buf + off, cap - off, "]");
    return strlen(buf);
}
