#include "ep_app_loradecoder.hpp"
#include "lora_decoder_feed.h"
#include "sx_manager.hpp"
#include "meshtonic_board.h"
#include "pinconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <algorithm>
#include <strings.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <inttypes.h>
#include "../lora_decode.h"
#include "../lora_bands.h"
#include "driver/spi_master.h"

static std::vector<LoraPacket> g_global_lora_packets;
static constexpr size_t GLOBAL_MAX = 32;

static const char *TAG = "LoraDecApp";

extern SXRadioManager sxManager;
extern PinConfig pinConfig;
extern bool i2p_pp_conn_state;

static const char* backendName(uint8_t backend) {
    switch (backend) {
        case LORA_BACKEND_HOST_BRIDGE: return "bridge";
        case LORA_BACKEND_WIO: return "wio";
        case LORA_BACKEND_HACKRF_DSP: return "hackrf";
        case LORA_BACKEND_HYBRID: return "hybrid";
        default: return "?";
    }
}

static bool backendUsesWio(uint8_t b) {
    return b == LORA_BACKEND_WIO || b == LORA_BACKEND_HYBRID;
}

static bool backendUsesHackrf(uint8_t b) {
    return b == LORA_BACKEND_HACKRF_DSP || b == LORA_BACKEND_HYBRID;
}

static RadioRxMode rxModeFromU8(uint8_t v) {
    if (v == 1) return RadioRxMode::CONTINUOUS;
    if (v == 2) return RadioRxMode::AUTO;
    return RadioRxMode::CAD;
}

static uint8_t rxModeToU8(RadioRxMode m) {
    switch (m) {
        case RadioRxMode::CAD: return 0;
        case RadioRxMode::CONTINUOUS: return 1;
        case RadioRxMode::AUTO: return 2;
        default: return 0;
    }
}

const char* EPAppLoraDecoder::rxModeName(RadioRxMode m) {
    switch (m) {
        case RadioRxMode::CAD: return "cad";
        case RadioRxMode::CONTINUOUS: return "cont";
        case RadioRxMode::AUTO: return "auto";
        default: return "cad";
    }
}

RadioRxMode EPAppLoraDecoder::parseRxModeToken(const char* tok) {
    if (!tok) return RadioRxMode::CAD;
    if (strcasecmp(tok, "cont") == 0 || strcasecmp(tok, "continuous") == 0) return RadioRxMode::CONTINUOUS;
    if (strcasecmp(tok, "auto") == 0) return RadioRxMode::AUTO;
    return RadioRxMode::CAD;
}

EPAppLoraDecoder::EPAppLoraDecoder() {
    packets.reserve(MAX_PACKETS);
    lora_decode_keys_init_defaults();
    lora_decode_keys_load_nvs();

    config.center_mhz = 915.0f;
    config.sf_mask = 0xFF;
    config.bw_mask = 0x00FF;
    config.radio_count = (uint8_t)(pinConfig.getProfile() == PinConfig::BoardProfile::MESHTONIC_H4M
                                   ? pinConfig.getRadioCount() : 1);
    if (config.radio_count < 1) config.radio_count = 1;
    if (config.radio_count > 4) config.radio_count = 4;
    config.backend = LORA_BACKEND_WIO;
    config.default_rx_mode = RadioRxMode::CAD;
    config.global_cad_after_rx = true;
    config.decode_mode = LORA_DECODE_AUTO;

    lora_apply_preset_to_config(&config, "US915-meshtastic");

    lora_dsp_init();
    loadConfigFromNvs();
    if (config.keys[0]) {
        lora_decode_keys_set_legacy_string(config.keys);
    }

    if (pinConfig.getProfile() == PinConfig::BoardProfile::MESHTONIC_H4M) {
        if (i2p_pp_conn_state) config.backend = LORA_BACKEND_HYBRID;
        startSession();
    }
}

const char* EPAppLoraDecoder::backendName(uint8_t backend) {
    return ::backendName(backend);
}

void EPAppLoraDecoder::maybeUpgradeHybridBackend() {
    if (!running) return;
    if (i2p_pp_conn_state && config.backend == LORA_BACKEND_WIO) {
        config.backend = LORA_BACKEND_HYBRID;
        lora_dsp_init();
        ESP_LOGI(TAG, "PortaPack linked — hybrid WIO+HackRF");
        sendStatusToWeb();
    }
}

void EPAppLoraDecoder::processIqBuffer() {
    if (!iq_received || iq_received < 512) {
        iq_active = false;
        iq_received = 0;
        return;
    }

    lora_dsp_cfg dcfg{};
    dcfg.center_hz = iq_center_mhz * 1e6f;
    dcfg.search_bw_hz = 500000;
    uint8_t sfs[6] = {7, 8, 9, 10, 11, 12};
    dcfg.num_sf = 6;
    memcpy(dcfg.sf_list, sfs, 6);
    uint32_t bws[3] = {125000, 250000, 500000};
    dcfg.num_bw = 3;
    memcpy(dcfg.bw_list, bws, sizeof(bws));
    dcfg.sc_threshold = 4.0f;
    dcfg.max_symbols = 32;

    lora_decoded_pkt pkts[4];
    int n = lora_dsp_feed_sc16(iq_buf, iq_received, iq_fs, iq_center_mhz, &dcfg, pkts, 4);
    hackrf_burst_count++;

    for (int i = 0; i < n; ++i) {
        LoraPacket p{};
        p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        p.freq_mhz = iq_center_mhz;
        p.bw_hz = pkts[i].bw_hz;
        p.sf = pkts[i].sf;
        p.rssi = pkts[i].rssi;
        p.snr = pkts[i].snr_q8 / 4;
        char hex[513] = {0};
        size_t plen = pkts[i].payload_len;
        if (plen > 256) plen = 256;
        for (size_t k = 0; k < plen; ++k) sprintf(hex + k * 2, "%02x", pkts[i].payload[k]);
        p.payload_hex = hex;
        strncpy(p.region, lora_region_from_freq(iq_center_mhz), sizeof(p.region) - 1);
        strncpy(p.band, p.region, sizeof(p.band) - 1);
        strncpy(p.decode_backend, "hackrf", sizeof(p.decode_backend) - 1);

        if (plen >= 4) {
            LoraDecodeContext dctx{};
            strncpy(dctx.region, p.region, sizeof(dctx.region) - 1);
            dctx.freq_mhz = p.freq_mhz;
            dctx.sf = p.sf;
            dctx.bw_hz = p.bw_hz;
            auto outcome = lora_decode_process_air_ex(pkts[i].payload, plen, p, &dctx);
            if (outcome.proto) p.proto = outcome.proto;
            strncpy(p.confidence, outcome.confidence, sizeof(p.confidence) - 1);
            p.decrypted = outcome.decrypted;
            if (outcome.key_label[0]) strncpy(p.key_label, outcome.key_label, sizeof(p.key_label) - 1);
            if (p.info.empty()) p.info = pkts[i].crc_ok ? "hackrf-dsp" : "hackrf";
        } else {
            p.proto = pkts[i].proto;
            p.info = pkts[i].crc_ok ? "hackrf-dsp" : "hackrf";
        }
        pushPacket(p);
    }

    iq_active = false;
    iq_received = 0;
    iq_expected = 0;
}

void EPAppLoraDecoder::handleFeedIq(const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint8_t sub = data[0];

    if (sub == 0 && data.size() >= 13) {
        memcpy(&iq_center_mhz, &data[1], 4);
        memcpy(&iq_fs, &data[5], 4);
        memcpy(&iq_expected, &data[9], 4);
        if (iq_expected > IQ_MAX_SAMPLES) iq_expected = IQ_MAX_SAMPLES;
        iq_received = 0;
        iq_active = iq_expected > 0;
        if (!running) startSession();
        if (config.backend == LORA_BACKEND_WIO) config.backend = LORA_BACKEND_HYBRID;
        return;
    }

    if (sub == 1 && iq_active && data.size() > 1) {
        size_t off = 1;
        while (off + 3 < data.size() && iq_received < IQ_MAX_SAMPLES) {
            iq_buf[iq_received * 2] = (int16_t)(data[off] | (data[off + 1] << 8));
            iq_buf[iq_received * 2 + 1] = (int16_t)(data[off + 2] | (data[off + 3] << 8));
            off += 4;
            iq_received++;
        }
        if (iq_expected > 0 && iq_received >= iq_expected) processIqBuffer();
        return;
    }

    if (sub == 2) processIqBuffer();
}

bool EPAppLoraDecoder::loadConfigFromNvs() {
    nvs_handle_t h;
    if (nvs_open("lora_cfg", NVS_READONLY, &h) != ESP_OK) return false;

    uint8_t b = 0, rc = 0;
    if (nvs_get_u8(h, "backend", &b) == ESP_OK) config.backend = b;
    if (nvs_get_u8(h, "rcount", &rc) == ESP_OK && rc > 0 && rc <= 4) config.radio_count = rc;

    uint16_t cx = 0;
    if (nvs_get_u16(h, "center_x4", &cx) == ESP_OK) {
        float c = cx / 4.0f;
        if (c > 100 && c < 1000) config.center_mhz = c;
    }

    uint8_t drm = 0;
    if (nvs_get_u8(h, "def_rxmode", &drm) == ESP_OK) config.default_rx_mode = rxModeFromU8(drm);
    uint8_t cad_after = 1;
    if (nvs_get_u8(h, "cad_after", &cad_after) == ESP_OK) config.global_cad_after_rx = cad_after != 0;

    for (int i = 0; i < 4; i++) {
        char key[12];
        snprintf(key, sizeof(key), "rxmode%d", i);
        uint8_t rm = 255;
        if (nvs_get_u8(h, key, &rm) == ESP_OK && rm <= 2) config.channels[i].rx_mode = rxModeFromU8(rm);
        snprintf(key, sizeof(key), "chcad%d", i);
        uint8_t ca = 255;
        if (nvs_get_u8(h, key, &ca) == ESP_OK && ca <= 1) config.channels[i].cad_after_rx = ca != 0;
    }

    struct ChBlob {
        float freq;
        uint8_t sf;
        uint32_t bw;
        uint8_t cr;
    } chb[4];
    size_t sz = sizeof(chb);
    if (nvs_get_blob(h, "channels", chb, &sz) == ESP_OK && sz == sizeof(chb)) {
        for (int i = 0; i < 4; i++) {
            if (chb[i].freq > 100.f) config.channels[i].freq_mhz = chb[i].freq;
            if (chb[i].sf >= 7 && chb[i].sf <= 12) config.channels[i].sf = chb[i].sf;
            if (chb[i].bw >= 62500) config.channels[i].bw_hz = chb[i].bw;
            if (chb[i].cr >= 1 && chb[i].cr <= 8) config.channels[i].cr = chb[i].cr;
        }
    }

    for (int i = 0; i < 4; i++) {
        char key[16];
        snprintf(key, sizeof(key), "reg%d", i);
        size_t rlen = sizeof(config.channels[i].region);
        nvs_get_str(h, key, config.channels[i].region, &rlen);
        snprintf(key, sizeof(key), "prof%d", i);
        size_t plen = sizeof(config.channels[i].profile);
        nvs_get_str(h, key, config.channels[i].profile, &plen);
        snprintf(key, sizeof(key), "pid%d", i);
        size_t pidlen = sizeof(config.channels[i].preset_id);
        nvs_get_str(h, key, config.channels[i].preset_id, &pidlen);
        if (!config.channels[i].region[0]) {
            strncpy(config.channels[i].region, lora_region_from_freq(config.channels[i].freq_mhz),
                    sizeof(config.channels[i].region) - 1);
        }
    }

    size_t aplen = sizeof(config.active_preset_id);
    nvs_get_str(h, "active_preset", config.active_preset_id, &aplen);
    uint8_t dm = 0;
    if (nvs_get_u8(h, "decode_mode", &dm) == ESP_OK && dm <= 2) config.decode_mode = (LoraDecodeMode)dm;
    size_t surl = sizeof(config.sidecar_url);
    nvs_get_str(h, "sidecar_url", config.sidecar_url, &surl);

    size_t keys_len = sizeof(config.keys);
    if (nvs_get_str(h, "keys_legacy", config.keys, &keys_len) == ESP_OK) {
        lora_decode_keys_set_legacy_string(config.keys);
    }

    nvs_close(h);
    return true;
}

void EPAppLoraDecoder::persistConfigToNvs() {
    nvs_handle_t h;
    if (nvs_open("lora_cfg", NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_u8(h, "backend", config.backend);
    nvs_set_u8(h, "rcount", config.radio_count);
    nvs_set_u16(h, "center_x4", (uint16_t)(config.center_mhz * 4.0f + 0.5f));
    nvs_set_u8(h, "def_rxmode", rxModeToU8(config.default_rx_mode));
    nvs_set_u8(h, "cad_after", config.global_cad_after_rx ? 1 : 0);

    for (int i = 0; i < 4; i++) {
        char key[12];
        snprintf(key, sizeof(key), "rxmode%d", i);
        nvs_set_u8(h, key, rxModeToU8(config.channels[i].rx_mode));
        snprintf(key, sizeof(key), "chcad%d", i);
        nvs_set_u8(h, key, config.channels[i].cad_after_rx ? 1 : 0);
    }

    struct ChBlob {
        float freq;
        uint8_t sf;
        uint32_t bw;
        uint8_t cr;
    } chb[4];
    for (int i = 0; i < 4; i++) {
        chb[i].freq = config.channels[i].freq_mhz;
        chb[i].sf = config.channels[i].sf;
        chb[i].bw = config.channels[i].bw_hz;
        chb[i].cr = config.channels[i].cr;
    }
    nvs_set_blob(h, "channels", chb, sizeof(chb));
    for (int i = 0; i < 4; i++) {
        char key[16];
        snprintf(key, sizeof(key), "reg%d", i);
        nvs_set_str(h, key, config.channels[i].region);
        snprintf(key, sizeof(key), "prof%d", i);
        nvs_set_str(h, key, config.channels[i].profile);
        snprintf(key, sizeof(key), "pid%d", i);
        nvs_set_str(h, key, config.channels[i].preset_id);
    }
    if (config.active_preset_id[0]) nvs_set_str(h, "active_preset", config.active_preset_id);
    nvs_set_u8(h, "decode_mode", (uint8_t)config.decode_mode);
    if (config.sidecar_url[0]) nvs_set_str(h, "sidecar_url", config.sidecar_url);
    if (config.keys[0]) nvs_set_str(h, "keys_legacy", config.keys);

    nvs_commit(h);
    nvs_close(h);
}

void EPAppLoraDecoder::pushPacket(const LoraPacket& pkt) {
    if (packets.size() >= MAX_PACKETS) packets.erase(packets.begin());
    packets.push_back(pkt);
    updateBandAgg(pkt);
    if (running) sendPacketsToWeb(1);
}

const char* EPAppLoraDecoder::protoName(uint8_t proto) {
    return lora_proto_name(proto);
}

void EPAppLoraDecoder::resetBandAgg() {
    num_band_agg = 0;
    memset(band_agg, 0, sizeof(band_agg));
}

void EPAppLoraDecoder::updateBandAgg(const LoraPacket& p) {
    const char* reg = p.region[0] ? p.region : lora_region_from_freq(p.freq_mhz);
    int idx = -1;
    for (int i = 0; i < num_band_agg; i++) {
        if (strcmp(band_agg[i].region, reg) == 0 &&
            strcmp(band_agg[i].profile, p.profile[0] ? p.profile : "custom") == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && num_band_agg < MAX_BAND_AGG) {
        idx = num_band_agg++;
        strncpy(band_agg[idx].region, reg, sizeof(band_agg[idx].region) - 1);
        strncpy(band_agg[idx].profile, p.profile[0] ? p.profile : "custom",
                sizeof(band_agg[idx].profile) - 1);
    }
    if (idx < 0) return;
    band_agg[idx].pkts++;
    band_agg[idx].last_ts = p.ts_ms;
    if (p.slot < 4) band_agg[idx].slots_mask |= (uint8_t)(1u << p.slot);
    if (p.proto < 10) band_agg[idx].proto_counts[p.proto]++;
}

void EPAppLoraDecoder::tagPacketFromSlot(LoraPacket& p, int slot) {
    if (slot < 0 || slot >= 4) return;
    p.slot = (uint8_t)slot;
    strncpy(p.region, config.channels[slot].region, sizeof(p.region) - 1);
    strncpy(p.profile, config.channels[slot].profile, sizeof(p.profile) - 1);
    strncpy(p.preset_id, config.channels[slot].preset_id, sizeof(p.preset_id) - 1);
    strncpy(p.band, config.channels[slot].region, sizeof(p.band) - 1);
}

bool EPAppLoraDecoder::applyPresetToSlot(int slot, const char* preset_id) {
    return lora_apply_preset_to_slot(&config.channels[slot], preset_id, slot);
}

bool EPAppLoraDecoder::applyPresetId(const char* preset_id) {
    return lora_apply_preset_to_config(&config, preset_id);
}

std::vector<LoraPacket> EPAppLoraDecoder::getRecentPackets(size_t maxCount) const {
    if (packets.size() <= maxCount) return packets;
    return std::vector<LoraPacket>(packets.end() - maxCount, packets.end());
}

void EPAppLoraDecoder::startSession() {
    running = true;
    resetBandAgg();
    lora_radio_set_monitor_mode(true);
    if (config.backend == LORA_BACKEND_WIO || config.backend == LORA_BACKEND_HYBRID) {
        armLocalRadios();
    }
    if (config.backend == LORA_BACKEND_HACKRF_DSP || config.backend == LORA_BACKEND_HYBRID) {
        lora_dsp_init();
        ESP_LOGI(TAG, "HackRF DSP backend armed (IQ via PortaPack I2C)");
    }
    ESP_LOGI(TAG, "LoRa decoder started backend=%s preset=%s",
             backendName(config.backend), config.active_preset_id[0] ? config.active_preset_id : "default");
    sendStatusToWeb();
}

void EPAppLoraDecoder::stopSession() {
    running = false;
    lora_radio_set_monitor_mode(false);
    disarmLocalRadios();
    ESP_LOGI(TAG, "LoRa decoder session stopped");
    sendStatusToWeb();
}

void EPAppLoraDecoder::setConfig(const LoraDecoderConfig& cfg) {
  if (running && cfg.backend != config.backend) {
        disarmLocalRadios();
    }
    config = cfg;
    if (config.keys[0]) lora_decode_keys_set_legacy_string(config.keys);
    persistConfigToNvs();
    if (running && backendUsesWio(config.backend)) armLocalRadios();
    else if (running) disarmLocalRadios();
    if (running && backendUsesHackrf(config.backend)) lora_dsp_init();
    sendStatusToWeb();
}

RadioRxMode EPAppLoraDecoder::resolveRxMode(int slot) const {
    if (slot < 0 || slot >= MAX_RADIOS) return config.default_rx_mode;
    RadioRxMode m = config.channels[slot].rx_mode;
    if (m == RadioRxMode::CAD && config.default_rx_mode != RadioRxMode::CAD) {
        // Per-slot unset uses global when channel mode equals CAD sentinel at load — use explicit per-slot only
    }
    return m;
}

void EPAppLoraDecoder::startRxForSlot(int slot) {
    if (slot < 0 || slot >= MAX_RADIOS || !radios[slot]) return;
    RadioRxMode mode = resolveRxMode(slot);
    if (mode == RadioRxMode::AUTO) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (last_rx_ms[slot] && (now - last_rx_ms[slot]) < 3000) mode = RadioRxMode::CONTINUOUS;
        else mode = RadioRxMode::CAD;
    }
    active_rx_mode[slot] = mode;
    if (mode == RadioRxMode::CONTINUOUS) radios[slot]->startRxContinuous();
    else radios[slot]->startCad();
}

void EPAppLoraDecoder::applyRxPolicyAfterPacket(int slot, uint32_t now_ms) {
    if (slot < 0 || slot >= MAX_RADIOS || !radios[slot]) return;
    last_rx_ms[slot] = now_ms;
    RadioRxMode mode = resolveRxMode(slot);
    bool cad_after = config.channels[slot].cad_after_rx;
    if (mode == RadioRxMode::AUTO) {
        startRxForSlot(slot);
        return;
    }
    if (mode == RadioRxMode::CONTINUOUS) {
        active_rx_mode[slot] = RadioRxMode::CONTINUOUS;
        radios[slot]->startRxContinuous();
        return;
    }
    if (cad_after || config.global_cad_after_rx) {
        active_rx_mode[slot] = RadioRxMode::CAD;
        radios[slot]->startCad();
    } else {
        active_rx_mode[slot] = RadioRxMode::CONTINUOUS;
        radios[slot]->startRxContinuous();
    }
}

void EPAppLoraDecoder::armLocalRadios() {
    rearming = true;
    arm_generation++;
    const uint32_t gen = arm_generation;

    sxManager.setRadioCount(config.radio_count);
    sxManager.init(config.radio_count);

    for (int i = 0; i < MAX_RADIOS; ++i) {
        if (radios[i]) {
            sxManager.assignRadio(i, nullptr);
            radios[i]->deinit();
            delete radios[i];
            radios[i] = nullptr;
        }
        sxManager.clearPendingIrq(i);
        last_rssi[i] = 0;
        last_rx_ms[i] = 0;
        last_key_label[i][0] = '\0';
        active_rx_mode[i] = RadioRxMode::CAD;
    }
    numActiveRadios = 0;
    config.slot_present_mask = 0;

    if (gen != arm_generation) {
        rearming = false;
        return;
    }

    spi_host_device_t host = SPI2_HOST;

    for (int i = 0; i < config.radio_count && i < MAX_RADIOS; ++i) {
        auto* r = new LoraRadio(i);
        if (r->init(host, 11, 12, 13, 10000000) != ESP_OK) {
            ESP_LOGW(TAG, "LoraRadio %d SPI init failed", i);
            delete r;
            continue;
        }
        if (gen != arm_generation) {
            delete r;
            rearming = false;
            return;
        }

        radios[i] = r;
        sxManager.assignRadio(i, r);

        RadioChannel ch = config.channels[i];
        if (ch.freq_mhz < 100.0f) {
            ch.freq_mhz = config.center_mhz + (i * 0.2f);
            ch.sf = 12;
            ch.bw_hz = 125000;
            ch.cr = 1;
        }

        LoraConfig c{};
        c.freq_hz = (uint32_t)(ch.freq_mhz * 1000000.0f);
        c.sf = ch.sf;
        c.bw_hz = ch.bw_hz;
        c.cr = ch.cr;
        c.ldro = (ch.bw_hz <= 62500 && ch.sf >= 11) ? 1 : 0;
        c.preamble_syms = 8;
        c.explicit_header = true;
        c.crc_on = true;

        radioCfgs[i] = c;
        if (r->configureFor(c) == ESP_OK) {
            config.slot_present_mask |= (uint8_t)(1u << i);
            numActiveRadios++;
            startRxForSlot(i);
            ESP_LOGI(TAG, "Slot %d band=%s armed %.3fMHz SF%u mode=%s", i,
                     config.channels[i].region, ch.freq_mhz, ch.sf, rxModeName(active_rx_mode[i]));
        } else if (i == 3) {
            ESP_LOGW(TAG, "WIO4 slot3 configure failed — skipping 4th radio");
        }
    }

    if (config.radio_count >= 4 && (config.slot_present_mask & 0x08) == 0) {
        ESP_LOGW(TAG, "4-radio requested but slot3 not present; using %d radios", numActiveRadios);
    }

    ESP_LOGI(TAG, "Onboard LoRa radios armed: %d @ ~%.1f MHz (CAD preferred for power)", numActiveRadios, config.center_mhz);
    rearming = false;
    sendStatusToWeb();
}

void EPAppLoraDecoder::feedIQ_sc16(const int16_t* iq, size_t count, float fs, float center_mhz) {
    if (!running) return;
    if (config.backend != LORA_BACKEND_HACKRF_DSP && config.backend != LORA_BACKEND_HYBRID) return;
    if (!iq || count < 512) return;

    lora_dsp_cfg dcfg{};
    dcfg.center_hz = center_mhz * 1e6f;
    dcfg.search_bw_hz = 500000;
    uint8_t sfs[6] = {7, 8, 9, 10, 11, 12};
    dcfg.num_sf = 6;
    memcpy(dcfg.sf_list, sfs, 6);
    uint32_t bws[3] = {125000, 250000, 500000};
    dcfg.num_bw = 3;
    memcpy(dcfg.bw_list, bws, sizeof(bws));
    dcfg.sc_threshold = 4.0f;
    dcfg.max_symbols = 32;

    lora_decoded_pkt pkts[4];
    int n = lora_dsp_feed_sc16(iq, count, fs, center_mhz, &dcfg, pkts, 4);
    for (int i = 0; i < n; ++i) {
        LoraPacket p{};
        p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        p.freq_mhz = center_mhz;
        p.bw_hz = pkts[i].bw_hz;
        p.sf = pkts[i].sf;
        p.rssi = pkts[i].rssi;
        p.snr = pkts[i].snr_q8 / 4;
        char hex[65] = {0};
        for (int k = 0; k < pkts[i].payload_len && k < 32; ++k) {
            sprintf(hex + k * 2, "%02x", pkts[i].payload[k]);
        }
        p.payload_hex = hex;
        p.proto = pkts[i].proto;
        p.info = (pkts[i].crc_ok ? "dsp-crcok" : "dsp");
        strncpy(p.region, lora_region_from_freq(center_mhz), sizeof(p.region) - 1);
        strncpy(p.band, p.region, sizeof(p.band) - 1);
        strncpy(p.decode_backend, "cpp", sizeof(p.decode_backend) - 1);
        pushPacket(p);
    }
}

void EPAppLoraDecoder::disarmLocalRadios() {
    rearming = true;
    arm_generation++;
    sxManager.spiDeselectAll();
    for (int i = 0; i < MAX_RADIOS; ++i) {
        if (radios[i]) {
            sxManager.assignRadio(i, nullptr);
            radios[i]->deinit();
            delete radios[i];
            radios[i] = nullptr;
        }
        sxManager.clearPendingIrq(i);
    }
    numActiveRadios = 0;
    rearming = false;
    ESP_LOGI(TAG, "Local radios disarmed");
}

bool EPAppLoraDecoder::OnPPData(uint16_t command, std::vector<uint8_t>& data) {
    switch (command) {
        case PPCMD_LORADEC_CONTROL: {
            if (!data.empty()) {
                uint8_t op = data[0];
                if (op == 1) startSession();
                else if (op == 0) stopSession();
                else if (op == 2) armLocalRadios();
                else if (op == 3) disarmLocalRadios();
                else if (op == 4) { config.backend = LORA_BACKEND_HACKRF_DSP; setConfig(config); }
                else if (op == 5) { config.backend = LORA_BACKEND_WIO; setConfig(config); }
                else if (op == 6) { config.backend = LORA_BACKEND_HYBRID; setConfig(config); }
                else if (op == 7 && data.size() > 1) {
                    std::string pid((const char*)&data[1], data.size() - 1);
                    applyPresetId(pid.c_str());
                    if (!running) startSession();
                }
            }
            return true;
        }
        case PPCMD_LORADEC_FEEDIQ:
            handleFeedIq(data);
            return true;
        case PPCMD_LORADEC_SETCONFIG: {
            if (data.size() >= sizeof(float) + 4) {
                LoraDecoderConfig newcfg = config;
                memcpy(&newcfg.center_mhz, data.data(), 4);
                if (data.size() > 4) newcfg.radio_count = data[4];
                if (data.size() > 5) newcfg.backend = data[5];
                if (data.size() > 6) newcfg.default_rx_mode = rxModeFromU8(data[6]);
                setConfig(newcfg);
            }
            return true;
        }
        default:
            break;
    }
    return false;
}

bool EPAppLoraDecoder::OnPPReqData(uint16_t command, std::vector<uint8_t>& data) {
    if (command == PPCMD_LORADEC_GETSTATUS) {
        data.clear();
        data.push_back(running ? 1 : 0);
        data.push_back(config.backend);
        data.push_back(config.radio_count);
        uint16_t cf = (uint16_t)(config.center_mhz * 4.0f);
        data.push_back(cf & 0xFF);
        data.push_back((cf >> 8) & 0xFF);
        data.push_back(rxModeToU8(config.default_rx_mode));
        data.push_back(config.global_cad_after_rx ? 1 : 0);
        data.push_back(config.slot_present_mask);
        return true;
    }
    if (command == PPCMD_LORADEC_GETPACKETS) {
        data.clear();
        auto recent = getRecentPackets(4);
        for (const auto& p : recent) {
            uint16_t cf = (uint16_t)(p.freq_mhz * 4.0f);
            data.push_back(cf & 0xFF);
            data.push_back((cf >> 8) & 0xFF);
        data.push_back(p.sf);
        data.push_back(p.proto);
        data.push_back(p.slot);
        data.push_back((uint8_t)(p.rssi + 128));
            uint8_t n = std::min<uint8_t>((uint8_t)p.payload_hex.size() / 2, 8);
            data.push_back(n);
            for (uint8_t i = 0; i < n && i * 2 + 1 < p.payload_hex.size(); ++i) {
                char c = p.payload_hex[i * 2];
                uint8_t v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f' ? 10 + c - 'a' : 0);
                data.push_back(v);
            }
            if (n < 8) data.push_back(0);
        }
        return true;
    }
    if (command == PPCMD_LORADEC_GETUI) {
        data.clear();
        data.push_back(running ? 1 : 0);
        data.push_back(config.backend);
        data.push_back((uint8_t)std::min(packets.size(), size_t(255)));
        data.push_back(i2p_pp_conn_state ? 1 : 0);
        data.push_back((uint8_t)numActiveRadios);
        data.push_back((uint8_t)(hackrf_burst_count & 0xFF));

        char lines[6][21] = {};
        snprintf(lines[0], 21, "LoRa %s", running ? "ON" : "OFF");
        snprintf(lines[1], 21, "%.8s", config.active_preset_id[0] ? config.active_preset_id : "preset");
        snprintf(lines[2], 21, "%s W:%d P:%s", backendName(config.backend), numActiveRadios,
                 i2p_pp_conn_state ? "Y" : "N");
        snprintf(lines[3], 21, "Pkts:%u HF:%u", (unsigned)packets.size(), (unsigned)hackrf_burst_count);
        if (!packets.empty()) {
            const auto& last = packets.back();
            snprintf(lines[4], 21, "%s SF%u %ddBm", protoName(last.proto), last.sf, last.rssi);
            if (!last.info.empty()) snprintf(lines[5], 21, "%.20s", last.info.c_str());
        } else {
            snprintf(lines[4], 21, "Listening...");
        }
        for (int i = 0; i < 6; i++) {
            for (int c = 0; c < 20; c++) data.push_back((uint8_t)lines[i][c]);
        }
        return true;
    }
    return false;
}

bool EPAppLoraDecoder::OnWebData(std::string& data) {
    if (data.rfind("LORA:", 0) != 0) return false;

    std::string cmd = data.substr(5);
    if (cmd == "START") { startSession(); return true; }
    if (cmd == "STOP") { stopSession(); return true; }

    if (cmd.rfind("CONFIG:", 0) == 0) {
        LoraDecoderConfig nc = config;
        size_t pos = 7;
        while (pos < cmd.size()) {
            size_t eq = cmd.find('=', pos);
            if (eq == std::string::npos) break;
            size_t comma = cmd.find(',', eq);
            std::string key = cmd.substr(pos, eq - pos);
            std::string val = cmd.substr(eq + 1, (comma == std::string::npos ? cmd.size() : comma) - (eq + 1));
            if (key == "center") nc.center_mhz = strtof(val.c_str(), nullptr);
            else if (key == "backend") nc.backend = (uint8_t)atoi(val.c_str());
            else if (key == "radio_count") nc.radio_count = (uint8_t)std::clamp(atoi(val.c_str()), 1, 4);
            else if (key == "rx_mode") nc.default_rx_mode = parseRxModeToken(val.c_str());
            else if (key == "cad_after") nc.global_cad_after_rx = val != "0" && val != "false";
            pos = (comma == std::string::npos ? cmd.size() : comma + 1);
        }
        setConfig(nc);
        return true;
    }

    if (cmd.rfind("INJECT:", 0) == 0) {
        LoraPacket p{};
        p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000);
        p.freq_mhz = config.center_mhz;
        p.sf = 12;
        p.bw_hz = 125000;
        p.rssi = -90;
        p.snr = 5;
        p.payload_hex = "cafebabe";
        p.proto = 1;
        p.info = "web-inject";
        size_t ppos = cmd.find("p=");
        if (ppos != std::string::npos) {
            p.payload_hex = cmd.substr(ppos + 2, 64);
            size_t c = p.payload_hex.find(',');
            if (c != std::string::npos) p.payload_hex.resize(c);
        }
        pushPacket(p);
        return true;
    }

    if (cmd == "STATUS") { sendStatusToWeb(); return true; }
    if (cmd == "PACKETS") { sendPacketsToWeb(16); return true; }
    if (cmd == "PRESETS") {
        char buf[1024];
        lora_list_presets_json(buf, sizeof(buf));
        SendDataToWeb(std::string("{\"type\":\"loradec_presets\",\"presets\":") + buf + "}");
        return true;
    }

    if (cmd.rfind("PRESET:", 0) == 0) {
        const char* pid = cmd.c_str() + 7;
        if (applyPresetId(pid)) {
            persistConfigToNvs();
            if (running && backendUsesWio(config.backend)) armLocalRadios();
            sendStatusToWeb();
        }
        return true;
    }

    if (cmd.rfind("REGION:", 0) == 0) {
        std::string reg = cmd.substr(7);
        char preset[48];
        snprintf(preset, sizeof(preset), "%s-meshtastic", reg.c_str());
        if (!lora_find_preset(preset)) snprintf(preset, sizeof(preset), "%s", reg.c_str());
        applyPresetId(preset);
        persistConfigToNvs();
        if (running && backendUsesWio(config.backend)) armLocalRadios();
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("SLOT:", 0) == 0) {
        size_t eq = cmd.find('=');
        if (eq != std::string::npos) {
            int slot = atoi(cmd.c_str() + 5);
            std::string pid = cmd.substr(eq + 1);
            if (slot >= 0 && slot < 4 && applyPresetToSlot(slot, pid.c_str())) {
                persistConfigToNvs();
                if (running && backendUsesWio(config.backend)) armLocalRadios();
                sendStatusToWeb();
            }
        }
        return true;
    }

    if (cmd.rfind("DECODE:", 0) == 0) {
        std::string rest = cmd.substr(7);
        if (rest.rfind("mode=", 0) == 0) {
            const char* m = rest.c_str() + 5;
            if (strcasecmp(m, "python") == 0) config.decode_mode = LORA_DECODE_PYTHON;
            else if (strcasecmp(m, "auto") == 0) config.decode_mode = LORA_DECODE_AUTO;
            else config.decode_mode = LORA_DECODE_CPP;
        } else if (rest.rfind("sidecar=", 0) == 0) {
            strncpy(config.sidecar_url, rest.c_str() + 8, sizeof(config.sidecar_url) - 1);
        }
        persistConfigToNvs();
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("KEYS:", 0) == 0) {
        const char* body = cmd.c_str() + 5;
        if (strcmp(body, "sync") == 0) {
            char kj[256];
            lora_decode_keys_export_json(kj, sizeof(kj));
            SendDataToWeb(std::string(kj));
            return true;
        }
        strncpy(config.keys, body, sizeof(config.keys) - 1);
        config.keys[sizeof(config.keys) - 1] = '\0';
        lora_decode_keys_parse_web(body);
        persistConfigToNvs();
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("BACKEND:", 0) == 0) {
        const char* tok = cmd.c_str() + 8;
        if (strcasecmp(tok, "wio") == 0) config.backend = LORA_BACKEND_WIO;
        else if (strcasecmp(tok, "hackrf") == 0) config.backend = LORA_BACKEND_HACKRF_DSP;
        else if (strcasecmp(tok, "hybrid") == 0) config.backend = LORA_BACKEND_HYBRID;
        else if (strcasecmp(tok, "bridge") == 0) config.backend = LORA_BACKEND_HOST_BRIDGE;
        else {
            int b = atoi(tok);
            if (b >= 0 && b <= 3) config.backend = (uint8_t)b;
        }
        setConfig(config);
        return true;
    }

    if (cmd.rfind("CHLIST:", 0) == 0) {
        std::string rest = cmd.substr(7);
        size_t start = 0;
        while (start < rest.size()) {
            size_t semi = rest.find(';', start);
            std::string item = rest.substr(start, semi == std::string::npos ? rest.size() - start : semi - start);
            size_t eq = item.find('=');
            if (eq != std::string::npos) {
                int slot = atoi(item.c_str());
                if (slot >= 0 && slot < 4) {
                    std::string vals = item.substr(eq + 1);
                    float freq = 0;
                    int sf = 0, cr = 0;
                    uint32_t bw = 0;
                    char mode[16] = {};
                    char preset_tok[40] = {};
                    int cad = -1;
                    unsigned int bw_u = 0;
                    int n = sscanf(vals.c_str(), "%f,%d,%u,%d,%15[^,],%d,%39s",
                                   &freq, &sf, &bw_u, &cr, mode, &cad, preset_tok);
                    if (n < 4) {
                        sscanf(vals.c_str(), "%f,%d,%u,%d,%15[^,]", &freq, &sf, &bw_u, &cr, mode);
                    }
                    bw = bw_u;
                    if (freq > 100.f) config.channels[slot].freq_mhz = freq;
                    if (sf >= 7 && sf <= 12) config.channels[slot].sf = (uint8_t)sf;
                    if (bw >= 62500) config.channels[slot].bw_hz = bw;
                    if (cr >= 1 && cr <= 8) config.channels[slot].cr = (uint8_t)cr;
                    if (mode[0]) config.channels[slot].rx_mode = parseRxModeToken(mode);
                    if (cad >= 0) config.channels[slot].cad_after_rx = cad != 0;
                    if (preset_tok[0]) {
                        lora_apply_preset_to_slot(&config.channels[slot], preset_tok, slot);
                    } else {
                        strncpy(config.channels[slot].region,
                                lora_region_from_freq(config.channels[slot].freq_mhz),
                                sizeof(config.channels[slot].region) - 1);
                    }
                }
            }
            if (semi == std::string::npos) break;
            start = semi + 1;
        }
        persistConfigToNvs();
        if (running && backendUsesWio(config.backend)) armLocalRadios();
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("POLICY:", 0) == 0) {
        std::string rest = cmd.substr(7);
        size_t start = 0;
        while (start < rest.size()) {
            size_t comma = rest.find(',', start);
            std::string item = rest.substr(start, comma == std::string::npos ? rest.size() - start : comma - start);
            size_t eq = item.find('=');
            if (eq != std::string::npos) {
                std::string key = item.substr(0, eq);
                std::string val = item.substr(eq + 1);
                if (key == "global" || key == "default") {
                    config.default_rx_mode = parseRxModeToken(val.c_str());
                } else if (key == "cad_after") {
                    config.global_cad_after_rx = val != "0" && val != "false";
                } else {
                    int slot = atoi(key.c_str());
                    if (slot >= 0 && slot < 4) config.channels[slot].rx_mode = parseRxModeToken(val.c_str());
                }
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        persistConfigToNvs();
        if (running && backendUsesWio(config.backend)) {
            for (int i = 0; i < numActiveRadios; i++) startRxForSlot(i);
        }
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("FEED_TEST", 0) == 0) {
        const size_t N = 8192;
        std::vector<int16_t> synth(2 * N);
        for (size_t i = 0; i < N; ++i) {
            float t = (float)i / 1000000.0f;
            float ph = 2 * 3.14159f * 50000.0f * t;
            synth[2 * i] = (int16_t)(cosf(ph) * 0.3f * 20000);
            synth[2 * i + 1] = (int16_t)(sinf(ph) * 0.3f * 20000);
        }
        feedIQ_sc16(synth.data(), N, 2000000.0f, config.center_mhz);
        return true;
    }

    if (cmd.rfind("FEED_IQ:", 0) == 0) {
        feedIQ_sc16(nullptr, 0, 2000000.0f, config.center_mhz);
        return true;
    }
    return false;
}

void EPAppLoraDecoder::sendStatusToWeb() {
    char buf[1400];
    char presets[512];
    lora_list_presets_json(presets, sizeof(presets));
    const char* dm = (config.decode_mode == LORA_DECODE_PYTHON) ? "python"
                     : (config.decode_mode == LORA_DECODE_AUTO) ? "auto" : "cpp";
    const char* hw = backendName(config.backend);
    int off = snprintf(buf, sizeof(buf),
        "{\"type\":\"loradec_status\",\"running\":%s,\"backend\":%u,\"radio_count\":%u,"
        "\"center\":%.3f,\"hw\":\"%s\",\"onboard_ants\":%d,\"def_rx_mode\":\"%s\","
        "\"cad_after\":%s,\"slot_present_mask\":%u,\"decode_mode\":\"%s\","
        "\"active_preset\":\"%s\",\"presets_available\":%s,\"slots\":[",
        running ? "true" : "false", config.backend, config.radio_count, config.center_mhz,
        hw, numActiveRadios, rxModeName(config.default_rx_mode),
        config.global_cad_after_rx ? "true" : "false", config.slot_present_mask, dm,
        config.active_preset_id, presets);

    for (int i = 0; i < MAX_RADIOS; i++) {
        if (i >= config.radio_count) break;
        off += snprintf(buf + off, sizeof(buf) - off,
            "%s{\"slot\":%d,\"present\":%s,\"region\":\"%s\",\"profile\":\"%s\","
            "\"preset_id\":\"%s\",\"band\":\"%s\",\"freq\":%.3f,\"sf\":%u,\"bw\":%u,"
            "\"rx_mode\":\"%s\",\"cad_after\":%s,\"last_rssi\":%d,\"last_key\":\"%s\"}",
            (i > 0 ? "," : ""), i,
            (config.slot_present_mask & (1u << i)) ? "true" : "false",
            config.channels[i].region, config.channels[i].profile,
            config.channels[i].preset_id, config.channels[i].region,
            config.channels[i].freq_mhz, config.channels[i].sf, (unsigned)config.channels[i].bw_hz,
            rxModeName((config.slot_present_mask & (1u << i)) ? active_rx_mode[i] : config.channels[i].rx_mode),
            config.channels[i].cad_after_rx ? "true" : "false",
            (int)last_rssi[i], last_key_label[i]);
        if (off >= (int)sizeof(buf) - 8) break;
    }
    off += snprintf(buf + off, sizeof(buf) - off, "],\"bands_in_use\":[");
    for (int i = 0; i < num_band_agg; i++) {
        const auto& b = band_agg[i];
        off += snprintf(buf + off, sizeof(buf) - off,
            "%s{\"region\":\"%s\",\"profile\":\"%s\",\"pkts\":%" PRIu32 ",\"last_ts\":%" PRIu32 ","
            "\"active\":%s,\"slots_mask\":%u,\"proto_counts\":{\"meshtastic\":%" PRIu32 ","
            "\"meshcore\":%" PRIu32 ",\"lorawan\":%" PRIu32 "}}",
            (i > 0 ? "," : ""), b.region, b.profile, b.pkts, b.last_ts,
            (b.last_ts > 0) ? "true" : "false", b.slots_mask,
            b.proto_counts[1], b.proto_counts[2], b.proto_counts[3]);
        if (off >= (int)sizeof(buf) - 4) break;
    }
    snprintf(buf + off, sizeof(buf) - off, "]}");
    SendDataToWeb(std::string(buf));
}

void EPAppLoraDecoder::sendPacketsToWeb(size_t count) {
    auto recent = getRecentPackets(count);
    std::string out = "{\"type\":\"loradec_packets\",\"packets\":[";
    for (size_t i = 0; i < recent.size(); ++i) {
        const auto& p = recent[i];
        char pkt[640];
        snprintf(pkt, sizeof(pkt),
            "%s{\"ts\":%" PRIu32 ",\"freq\":%.3f,\"sf\":%u,\"bw\":%" PRIu32 ",\"rssi\":%d,"
            "\"slot\":%u,\"band\":\"%s\",\"region\":\"%s\",\"profile\":\"%s\","
            "\"proto\":\"%s\",\"confidence\":\"%s\",\"decrypted\":%s,"
            "\"payload\":\"%s\",\"info\":\"%s\",\"decode_backend\":\"%s\"}",
            (i > 0 ? "," : ""), p.ts_ms, p.freq_mhz, p.sf, p.bw_hz, p.rssi,
            p.slot, p.band, p.region, p.profile, protoName(p.proto),
            p.confidence, p.decrypted ? "true" : "false",
            p.payload_hex.c_str(), p.info.c_str(), p.decode_backend);
        out += pkt;
    }
    out += "]}";
    SendDataToWeb(out);
}

void EPAppLoraDecoder::OnDisplayRequest(DisplayGeneric* display) {
    display->showTitle("Meshtonic LoRa");
    if (!running) {
        display->showMainText("Stopped\nLORA:START");
        return;
    }
    char line[64];
    snprintf(line, sizeof(line), "%s | %s", backendName(config.backend),
             config.active_preset_id[0] ? config.active_preset_id : "preset");
    std::string text(line);
    snprintf(line, sizeof(line), "WIO:%d PP:%s HF:%u", numActiveRadios,
             i2p_pp_conn_state ? "on" : "off", (unsigned)hackrf_burst_count);
    text += "\n";
    text += line;

    for (int i = 0; i < numActiveRadios && i < 2; i++) {
        snprintf(line, sizeof(line), "S%d %s %.1f SF%u", i, config.channels[i].region,
                 config.channels[i].freq_mhz, config.channels[i].sf);
        text += "\n";
        text += line;
    }

    snprintf(line, sizeof(line), "Pkts:%u", (unsigned)packets.size());
    text += "\n";
    text += line;

    if (!packets.empty()) {
        const auto& last = packets.back();
        snprintf(line, sizeof(line), "%s SF%u %ddBm", protoName(last.proto), last.sf, last.rssi);
        text += "\n";
        text += line;
        if (!last.info.empty()) {
            text += "\n";
            text += last.info.substr(0, 28);
        }
    } else if (backendUsesHackrf(config.backend) && !i2p_pp_conn_state) {
        text += "\nAttach PortaPack";
        text += "\nfor HackRF wideband";
    } else {
        text += "\nListening...";
    }
    display->showMainTextMultiline(text);
}

void EPAppLoraDecoder::Loop(uint32_t currentMillis) {
    if (!running || rearming) return;

    maybeUpgradeHybridBackend();

    if (!g_global_lora_packets.empty()) {
        for (const auto& gp : g_global_lora_packets) pushPacket(gp);
        g_global_lora_packets.clear();
    }

    if (backendUsesWio(config.backend)) {
        sxManager.servicePendingIrqs();
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        for (int i = 0; i < numActiveRadios; ++i) {
            auto* r = radios[i];
            if (!r || rearming) continue;

            bool activity = r->readDio1() || sxManager.hasPendingIrq(i);
            if (sxManager.hasPendingIrq(i)) sxManager.clearPendingIrq(i);

            if (!activity) continue;

            uint8_t len = 0;
            uint8_t buf[256];
            int8_t rssi = -90, snr = 0;

            if (r->checkRxDone(&len, 255, buf, &rssi, &snr) && len > 0) {
                last_rssi[i] = rssi;

                LoraPacket p{};
                p.ts_ms = now_ms;
                p.freq_mhz = radioCfgs[i].freq_hz / 1e6f;
                p.bw_hz = radioCfgs[i].bw_hz;
                p.sf = radioCfgs[i].sf;
                p.rssi = rssi;
                p.snr = snr;

                char hex[513] = {0};
                size_t hl = (size_t)len;
                if (hl > 256) hl = 256;
                for (size_t k = 0; k < hl; ++k) sprintf(hex + k * 2, "%02x", buf[k]);
                p.payload_hex = hex;
                tagPacketFromSlot(p, i);

                LoraDecodeContext dctx{};
                strncpy(dctx.region, config.channels[i].region, sizeof(dctx.region) - 1);
                dctx.freq_mhz = p.freq_mhz;
                dctx.sf = p.sf;
                dctx.bw_hz = p.bw_hz;

                auto outcome = lora_decode_process_air_ex(buf, len, p, &dctx);
                if (outcome.key_label[0]) {
                    strncpy(last_key_label[i], outcome.key_label, sizeof(last_key_label[i]) - 1);
                    strncpy(p.key_label, outcome.key_label, sizeof(p.key_label) - 1);
                } else if (outcome.encrypted_only) {
                    strncpy(last_key_label[i], "encrypted", sizeof(last_key_label[i]) - 1);
                }
                if (outcome.proto) p.proto = outcome.proto;
                strncpy(p.confidence, outcome.confidence, sizeof(p.confidence) - 1);
                p.decrypted = outcome.decrypted;
                strncpy(p.decode_backend, "cpp", sizeof(p.decode_backend) - 1);

                pushPacket(p);
                applyRxPolicyAfterPacket(i, now_ms);
            } else {
                startRxForSlot(i);
            }
        }
    }

    if (currentMillis - lastPushMs > 1500) {
        lastPushMs = currentMillis;
        sendStatusToWeb();
    }
}

void lora_decoder_push_record(const LoraDecodedRecord& rec) {
    LoraPacket p{};
    p.ts_ms = rec.ts_ms ? rec.ts_ms : (uint32_t)(esp_timer_get_time() / 1000ULL);
    p.freq_mhz = rec.freq_mhz;
    p.bw_hz = rec.bw_hz;
    p.sf = rec.sf;
    p.rssi = rec.rssi;
    p.snr = rec.snr;
    p.slot = rec.slot;
    p.proto = rec.proto;
    p.decrypted = rec.decrypted;
    p.payload_hex = rec.payload_hex;
    p.info = rec.info;
    strncpy(p.region, rec.region, sizeof(p.region) - 1);
    strncpy(p.profile, rec.profile, sizeof(p.profile) - 1);
    strncpy(p.preset_id, rec.preset_id, sizeof(p.preset_id) - 1);
    strncpy(p.band, rec.band[0] ? rec.band : rec.region, sizeof(p.band) - 1);
    strncpy(p.confidence, rec.confidence, sizeof(p.confidence) - 1);
    strncpy(p.key_label, rec.key_label, sizeof(p.key_label) - 1);
    strncpy(p.decode_backend, rec.decode_backend[0] ? rec.decode_backend : "bridge",
            sizeof(p.decode_backend) - 1);

    if (g_global_lora_packets.size() >= GLOBAL_MAX) {
        g_global_lora_packets.erase(g_global_lora_packets.begin());
    }
    g_global_lora_packets.push_back(p);
}

void lora_decoder_push_packet(uint32_t ts_ms,
                              float freq_mhz,
                              uint32_t bw_hz,
                              uint8_t sf,
                              int16_t rssi,
                              int8_t snr,
                              const char* payload_hex,
                              uint8_t proto,
                              const char* info) {
    LoraDecodedRecord rec{};
    rec.ts_ms = ts_ms;
    rec.freq_mhz = freq_mhz;
    rec.bw_hz = bw_hz;
    rec.sf = sf;
    rec.rssi = rssi;
    rec.snr = snr;
    rec.proto = proto;
    strncpy(rec.payload_hex, payload_hex ? payload_hex : "", sizeof(rec.payload_hex) - 1);
    strncpy(rec.info, info ? info : "", sizeof(rec.info) - 1);
    strncpy(rec.region, lora_region_from_freq(freq_mhz), sizeof(rec.region) - 1);
    strncpy(rec.band, rec.region, sizeof(rec.band) - 1);
    strncpy(rec.decode_backend, "bridge", sizeof(rec.decode_backend) - 1);
    strncpy(rec.confidence, "candidate", sizeof(rec.confidence) - 1);

    uint8_t tmp[256] = {};
    size_t tlen = strlen(rec.payload_hex) / 2;
    if (tlen > sizeof(tmp)) tlen = sizeof(tmp);
    for (size_t k = 0; k < tlen; k++) {
        unsigned v = 0;
        sscanf(rec.payload_hex + k * 2, "%02x", &v);
        tmp[k] = (uint8_t)v;
    }
    if (tlen >= 4 && proto == 0) {
        LoraPacket lp{};
        LoraDecodeContext ctx{};
        strncpy(ctx.region, rec.region, sizeof(ctx.region) - 1);
        ctx.freq_mhz = freq_mhz;
        ctx.sf = sf;
        ctx.bw_hz = bw_hz;
        auto outcome = lora_decode_process_air_ex(tmp, tlen, lp, &ctx);
        rec.proto = lp.proto ? lp.proto : outcome.proto;
        if (lp.info.size()) strncpy(rec.info, lp.info.c_str(), sizeof(rec.info) - 1);
        strncpy(rec.confidence, lp.confidence, sizeof(rec.confidence) - 1);
        rec.decrypted = lp.decrypted;
        strncpy(rec.decode_backend, "cpp", sizeof(rec.decode_backend) - 1);
    }

    lora_decoder_push_record(rec);
}
