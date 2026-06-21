#pragma once
/* AUTO-GENERATED from lora-wideband-decoder/presets.toml — do not edit */

#include <cstddef>
#include <cstdint>

struct RadioChannel;
struct LoraDecoderConfig;

constexpr int LORA_MAX_PRESETS = 29;
constexpr int LORA_MAX_SLOTS = 4;

struct LoraBandPreset {
    const char* id;
    const char* region;
    const char* profile;
    uint8_t sf;
    uint32_t bw_hz;
    uint8_t cr;
    float slot_freqs_mhz[LORA_MAX_SLOTS];
    uint8_t slot_count;
    float sdr_center_mhz;
    const char* lorawan_region;
};

extern const LoraBandPreset g_lora_presets[LORA_MAX_PRESETS];

const LoraBandPreset* lora_find_preset(const char* id);
const char* lora_region_from_freq(float freq_mhz);
bool lora_apply_preset_to_config(LoraDecoderConfig* cfg, const char* preset_id);
bool lora_apply_preset_to_slot(RadioChannel* ch, const char* preset_id, int slot);
size_t lora_list_preset_ids(char* buf, size_t cap);
size_t lora_list_presets_json(char* buf, size_t cap);
