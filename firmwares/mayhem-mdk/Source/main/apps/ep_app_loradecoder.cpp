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
#include "driver/spi_master.h"

static std::vector<LoraPacket> g_global_lora_packets;
static constexpr size_t GLOBAL_MAX = 32;

static const char *TAG = "LoraDecApp";

extern SXRadioManager sxManager;
extern PinConfig pinConfig;

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
    config.backend = 1;
    config.default_rx_mode = RadioRxMode::CAD;
    config.global_cad_after_rx = true;

    for (int i = 0; i < 4; i++) {
        config.channels[i].freq_mhz = config.center_mhz + (i * 0.2f);
        config.channels[i].sf = 12;
        config.channels[i].bw_hz = 125000;
        config.channels[i].cr = 1;
        config.channels[i].rx_mode = RadioRxMode::CAD;
        config.channels[i].cad_after_rx = true;
    }

    lora_dsp_init();
    loadConfigFromNvs();
    if (config.keys[0]) {
        lora_decode_keys_set_legacy_string(config.keys);
    }
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
            if (chb[i].cr >= 1 && chb[i].cr <= 4) config.channels[i].cr = chb[i].cr;
        }
    }

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
    if (config.keys[0]) nvs_set_str(h, "keys_legacy", config.keys);

    nvs_commit(h);
    nvs_close(h);
}

void EPAppLoraDecoder::pushPacket(const LoraPacket& pkt) {
    if (packets.size() >= MAX_PACKETS) packets.erase(packets.begin());
    packets.push_back(pkt);
    if (running) sendPacketsToWeb(1);
}

std::vector<LoraPacket> EPAppLoraDecoder::getRecentPackets(size_t maxCount) const {
    if (packets.size() <= maxCount) return packets;
    return std::vector<LoraPacket>(packets.end() - maxCount, packets.end());
}

void EPAppLoraDecoder::startSession() {
    running = true;
    lora_radio_set_monitor_mode(true);
    if (config.backend == 1) {
        armLocalRadios();
    } else if (config.backend == 2) {
        lora_dsp_init();
        ESP_LOGI(TAG, "Embedded LoRa DSP backend armed");
    }
    ESP_LOGI(TAG, "LoRa decoder session started (backend=%u)", config.backend);
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
    if (running && config.backend == 1) armLocalRadios();
    else if (running && config.backend != 1) disarmLocalRadios();
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
            ESP_LOGI(TAG, "Slot %d armed %.1fMHz SF%u mode=%s", i, ch.freq_mhz, ch.sf,
                     rxModeName(active_rx_mode[i]));
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
    if (config.backend != 2 || !running || count < 1024) return;

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
            }
            return true;
        }
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

    if (cmd.rfind("BACKEND:", 0) == 0) {
        int b = atoi(cmd.c_str() + 8);
        config.backend = (b >= 0 && b <= 2) ? (uint8_t)b : config.backend;
        persistConfigToNvs();
        if (running && config.backend == 2) lora_dsp_init();
        if (running && config.backend == 1) armLocalRadios();
        else if (running && config.backend != 1) disarmLocalRadios();
        sendStatusToWeb();
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
                    int cad = -1;
                    unsigned int bw_u = 0;
                    sscanf(vals.c_str(), "%f,%d,%u,%d,%15[^,],%d", &freq, &sf, &bw_u, &cr, mode, &cad);
                    bw = bw_u;
                    if (freq > 100.f) config.channels[slot].freq_mhz = freq;
                    if (sf >= 7 && sf <= 12) config.channels[slot].sf = (uint8_t)sf;
                    if (bw >= 62500) config.channels[slot].bw_hz = bw;
                    if (cr >= 1 && cr <= 4) config.channels[slot].cr = (uint8_t)cr;
                    if (mode[0]) config.channels[slot].rx_mode = parseRxModeToken(mode);
                    if (cad >= 0) config.channels[slot].cad_after_rx = cad != 0;
                }
            }
            if (semi == std::string::npos) break;
            start = semi + 1;
        }
        persistConfigToNvs();
        if (running && config.backend == 1) armLocalRadios();
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
        if (running && config.backend == 1) {
            for (int i = 0; i < numActiveRadios; i++) startRxForSlot(i);
        }
        sendStatusToWeb();
        return true;
    }

    if (cmd.rfind("KEYS:", 0) == 0) {
        const char* body = cmd.c_str() + 5;
        strncpy(config.keys, body, sizeof(config.keys) - 1);
        config.keys[sizeof(config.keys) - 1] = '\0';
        lora_decode_keys_parse_web(body);
        persistConfigToNvs();
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
    char buf[640];
    const char* hw = (config.backend == 1) ? "meshtonic_antennas"
                     : (config.backend == 2 ? "esp32_dsp_iq" : "host_bridge");
    int off = snprintf(buf, sizeof(buf),
        "{\"type\":\"loradec_status\",\"running\":%s,\"backend\":%u,\"radio_count\":%u,"
        "\"center\":%.3f,\"hw\":\"%s\",\"onboard_ants\":%d,\"def_rx_mode\":\"%s\","
        "\"cad_after\":%s,\"slot_present_mask\":%u,\"slots\":[",
        running ? "true" : "false", config.backend, config.radio_count, config.center_mhz,
        hw, numActiveRadios, rxModeName(config.default_rx_mode),
        config.global_cad_after_rx ? "true" : "false", config.slot_present_mask);

    for (int i = 0; i < MAX_RADIOS; i++) {
        if (i >= config.radio_count) break;
        off += snprintf(buf + off, sizeof(buf) - off,
            "%s{\"slot\":%d,\"present\":%s,\"freq\":%.3f,\"sf\":%u,\"bw\":%u,\"rx_mode\":\"%s\","
            "\"cad_after\":%s,\"last_rssi\":%d,\"last_key\":\"%s\"}",
            (i > 0 ? "," : ""), i,
            (config.slot_present_mask & (1u << i)) ? "true" : "false",
            config.channels[i].freq_mhz, config.channels[i].sf, (unsigned)config.channels[i].bw_hz,
            rxModeName((config.slot_present_mask & (1u << i)) ? active_rx_mode[i] : config.channels[i].rx_mode),
            config.channels[i].cad_after_rx ? "true" : "false",
            (int)last_rssi[i], last_key_label[i]);
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
        char pkt[384];
        snprintf(pkt, sizeof(pkt),
            "%s{\"ts\":%" PRIu32 ",\"f\":%.3f,\"sf\":%u,\"bw\":%" PRIu32 ",\"rssi\":%d,\"payload\":\"%s\",\"info\":\"%s\"}",
            (i > 0 ? "," : ""), p.ts_ms, p.freq_mhz, p.sf, p.bw_hz, p.rssi,
            p.payload_hex.c_str(), p.info.c_str());
        out += pkt;
    }
    out += "]}";
    SendDataToWeb(out);
}

void EPAppLoraDecoder::OnDisplayRequest(DisplayGeneric* display) {
    display->showTitle("LoRa Decoder");
    if (!running) {
        display->showMainText("Stopped");
        return;
    }
    char line[64];
    snprintf(line, sizeof(line), "Ants:%d backend:%u mode:%s", numActiveRadios, config.backend,
             rxModeName(config.default_rx_mode));
    std::string text(line);

    for (int i = 0; i < numActiveRadios; i++) {
        snprintf(line, sizeof(line), "S%d:%s %.1f", i, rxModeName(active_rx_mode[i]),
                 config.channels[i].freq_mhz);
        text += "\n";
        text += line;
    }

    if (!packets.empty()) {
        const auto& last = packets.back();
        snprintf(line, sizeof(line), "Last SF%u %ddBm", last.sf, last.rssi);
        text += "\n";
        text += line;
        if (!last.info.empty()) {
            text += "\n";
            text += last.info.substr(0, 32);
        }
    } else {
        text += "\nNo packets yet";
    }
    display->showMainTextMultiline(text);
}

void EPAppLoraDecoder::Loop(uint32_t currentMillis) {
    if (!running || rearming) return;

    if (!g_global_lora_packets.empty()) {
        for (const auto& gp : g_global_lora_packets) pushPacket(gp);
        g_global_lora_packets.clear();
    }

    if (config.backend == 1) {
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
                p.proto = 1;

                auto outcome = lora_decode_process_air(buf, len, p);
                if (outcome.key_label[0]) {
                    strncpy(last_key_label[i], outcome.key_label, sizeof(last_key_label[i]) - 1);
                } else if (outcome.encrypted_only) {
                    strncpy(last_key_label[i], "encrypted", sizeof(last_key_label[i]) - 1);
                }

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

void lora_decoder_push_packet(uint32_t ts_ms,
                              float freq_mhz,
                              uint32_t bw_hz,
                              uint8_t sf,
                              int16_t rssi,
                              int8_t snr,
                              const char* payload_hex,
                              uint8_t proto,
                              const char* info) {
    LoraPacket p{};
    p.ts_ms = ts_ms ? ts_ms : (uint32_t)(esp_timer_get_time() / 1000ULL);
    p.freq_mhz = freq_mhz;
    p.bw_hz = bw_hz;
    p.sf = sf;
    p.rssi = rssi;
    p.snr = snr;
    p.payload_hex = payload_hex ? payload_hex : "";
    p.proto = proto;
    p.info = info ? info : "";

    uint8_t tmp[256] = {};
    size_t tlen = p.payload_hex.size() / 2;
    if (tlen & 1) tlen--; // guard odd hex length
    if (tlen > sizeof(tmp)) tlen = sizeof(tmp);
    for (size_t k = 0; k < tlen; k++) {
        unsigned v = 0;
        sscanf(p.payload_hex.c_str() + k * 2, "%02x", &v);
        tmp[k] = (uint8_t)v;
    }
    if (tlen >= 16) lora_decode_process_air(tmp, tlen, p);

    if (g_global_lora_packets.size() >= GLOBAL_MAX) {
        g_global_lora_packets.erase(g_global_lora_packets.begin());
    }
    g_global_lora_packets.push_back(p);
}
