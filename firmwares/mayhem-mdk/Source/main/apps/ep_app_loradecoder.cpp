#include "ep_app_loradecoder.hpp"
#include "lora_decoder_feed.h"
#include "sx_manager.hpp"
#include "meshtonic_board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <inttypes.h>
#include "../lora_decode.h"
#include "driver/spi_master.h"  // for SPI2_HOST and spi_host_device_t in armLocalRadios

static std::vector<LoraPacket> g_global_lora_packets;
static constexpr size_t GLOBAL_MAX = 32;
static char g_lora_decode_keys[128] = {0};

static const char *TAG = "LoraDecApp";

extern SXRadioManager sxManager;  // from main.cpp

EPAppLoraDecoder::EPAppLoraDecoder() {
    packets.reserve(MAX_PACKETS);
    // Sensible Meshtonic H4M defaults (US915-ish, all SFs, host bridge primary)
    config.center_mhz = 915.0f;
    config.sf_mask = 0xFF; // 7-12
    config.bw_mask = 0x00FF;
    config.radio_count = 1;
    config.backend = 1; // default to onboard SX for Meshtonic H4M (backend=1); host bridge still supported via explicit set
    // Populate default per-channel (tied to radio_count at arm time)
    for (int i=0; i<4; i++) {
        config.channels[i].freq_mhz = config.center_mhz + (i-1)*0.2f;
        config.channels[i].sf = 12;
        config.channels[i].bw_hz = 125000;
        config.channels[i].cr = 1;
    }
    // Pre-init the DSP tables so backend=2 is immediately usable
    lora_dsp_init();

    // Load lora_cfg from NVS (center/radio/backend/keys minimal)
    nvs_handle_t h;
    if (nvs_open("lora_cfg", NVS_READONLY, &h) == ESP_OK) {
        uint8_t b=0, rc=0;
        if (nvs_get_u8(h, "backend", &b)==ESP_OK) config.backend = b;
        if (nvs_get_u8(h, "rcount", &rc)==ESP_OK && rc>0 && rc<=4) config.radio_count = rc;
        uint16_t cx = 0;
        if (nvs_get_u16(h, "center_x4", &cx) == ESP_OK) {
            float c = cx / 4.0f;
            if (c > 100 && c < 1000) config.center_mhz = c;
        }
        nvs_close(h);
    }
}

void EPAppLoraDecoder::pushPacket(const LoraPacket& pkt) {
    if (packets.size() >= MAX_PACKETS) {
        packets.erase(packets.begin());
    }
    packets.push_back(pkt);
    // push a compact update to web clients
    if (running) {
        sendPacketsToWeb(1);
    }
}

std::vector<LoraPacket> EPAppLoraDecoder::getRecentPackets(size_t maxCount) const {
    if (packets.size() <= maxCount) return packets;
    return std::vector<LoraPacket>(packets.end() - maxCount, packets.end());
}

void EPAppLoraDecoder::startSession() {
    running = true;
    if (config.backend == 1) {
        armLocalRadios();
    } else if (config.backend == 2) {
        lora_dsp_init();  // vendored DSP ready for HackRF / burst IQ (optional path)
        ESP_LOGI(TAG, "Embedded LoRa DSP backend armed (Schmidl-Cox + dechirp on ESP32)");
    } else if (config.backend == 1) {
        // Already armed in armLocalRadios()
    }
    ESP_LOGI(TAG, "LoRa decoder session started (backend=%u)", config.backend);
    sendStatusToWeb();
}

void EPAppLoraDecoder::stopSession() {
    running = false;
    disarmLocalRadios();
    ESP_LOGI(TAG, "LoRa decoder session stopped");
    sendStatusToWeb();
}

void EPAppLoraDecoder::setConfig(const LoraDecoderConfig& cfg) {
    config = cfg;
    strncpy(g_lora_decode_keys, config.keys, sizeof(g_lora_decode_keys) - 1);
    g_lora_decode_keys[sizeof(g_lora_decode_keys) - 1] = '\0';
    // Persist to lora_cfg NVS
    nvs_handle_t h;
    if (nvs_open("lora_cfg", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "backend", config.backend);
        nvs_set_u8(h, "rcount", config.radio_count);
        uint16_t cx = (uint16_t)(config.center_mhz * 4.0f + 0.5f);
        nvs_set_u16(h, "center_x4", cx);
        nvs_commit(h); nvs_close(h);
    }
    // If running and backend changed, re-arm
    if (running) {
        if (config.backend == 1) armLocalRadios();
        else disarmLocalRadios();
    }
    sendStatusToWeb();
}

void EPAppLoraDecoder::armLocalRadios() {
    // Real onboard path: use the Meshtonic H4M's own Wio SX1262 modules + antennas.
    // The "heavy DSP" tasks (detection via CAD, receive, metrics, software decode) run on the ESP32.
    // Power/sequencing: default 1-2 radios; 4 active increases draw—consider CAD policy + optional per-radio reset on stuck.

    sxManager.setRadioCount(config.radio_count);
    sxManager.init(config.radio_count);

    // Tear down previous
    for (int i = 0; i < MAX_RADIOS; ++i) {
        if (radios[i]) {
            delete radios[i];
            radios[i] = nullptr;
        }
    }
    numActiveRadios = 0;

    // Basic SPI host for the shared bus (display may have initialized it; we attach devices).
    spi_host_device_t host = SPI2_HOST; // typical for ESP32-S3

    // Create one LoraRadio per physical slot and configure it.
    // Use explicit RadioChannel list (from config / future NVS/web) when available.
    for (int i = 0; i < config.radio_count && i < MAX_RADIOS; ++i) {
        auto* r = new LoraRadio(i);
        if (r->init(host, 11, 12, 13, 10000000) != ESP_OK) {
            ESP_LOGW(TAG, "LoraRadio %d SPI init failed (will retry)", i);
            delete r;
            continue;
        }
        radios[i] = r;
        sxManager.assignRadio(i, r); // wire for direct IRQ dispatch from MCP INTA

        RadioChannel ch = config.channels[i];
        if (ch.freq_mhz < 100.0f) ch = { config.center_mhz + (i-1)*0.2f , 12, 125000, 1 };

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
            r->startCad();   // fast activity detection (the chip's equivalent of preamble search)
            numActiveRadios++;
        }
    }
    ESP_LOGI(TAG, "Onboard LoRa radios armed: %d physical modules using Meshtonic antennas @ ~%.1f MHz (CAD listening)", numActiveRadios, config.center_mhz);
}

void EPAppLoraDecoder::feedIQ_sc16(const int16_t* iq, size_t count, float fs, float center_mhz) {
    if (config.backend != 2 || !running || count < 1024) return;

    lora_dsp_cfg dcfg{};
    dcfg.center_hz = center_mhz * 1e6f;
    dcfg.search_bw_hz = 500000;
    uint8_t sfs[6] = {7,8,9,10,11,12};
    dcfg.num_sf = 6; memcpy(dcfg.sf_list, sfs, 6);
    uint32_t bws[3] = {125000,250000,500000};
    dcfg.num_bw = 3; memcpy(dcfg.bw_list, bws, sizeof(bws));
    dcfg.sc_threshold = 4.0f;
    dcfg.max_symbols = 32;

    lora_decoded_pkt pkts[4];
    int n = lora_dsp_feed_sc16(iq, count, fs, center_mhz, &dcfg, pkts, 4);
    for (int i=0; i<n; ++i) {
        LoraPacket p{};
        p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        p.freq_mhz = center_mhz;
        p.bw_hz = pkts[i].bw_hz;
        p.sf = pkts[i].sf;
        p.rssi = pkts[i].rssi;
        p.snr = pkts[i].snr_q8 / 4;
        char hex[65] = {0};
        for (int k=0; k < pkts[i].payload_len && k < 32; ++k) {
            sprintf(hex + k*2, "%02x", pkts[i].payload[k]);
        }
        p.payload_hex = hex;
        p.proto = pkts[i].proto;
        p.info = (pkts[i].crc_ok ? "dsp-crcok" : "dsp");
        pushPacket(p);
    }
}

void EPAppLoraDecoder::disarmLocalRadios() {
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
                // very simple wire: first 4 bytes float center, then uint8 radio_count, backend
                memcpy(&newcfg.center_mhz, data.data(), 4);
                if (data.size() > 4) newcfg.radio_count = data[4];
                if (data.size() > 5) newcfg.backend = data[5];
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
        // pack a small status blob
        data.clear();
        uint8_t st = running ? 1 : 0;
        data.push_back(st);
        data.push_back(config.backend);
        data.push_back(config.radio_count);
        // center as 2 bytes rough (MHz * 4 or something)
        uint16_t cf = (uint16_t)(config.center_mhz * 4.0f);
        data.push_back(cf & 0xFF);
        data.push_back((cf >> 8) & 0xFF);
        return true;
    }
    if (command == PPCMD_LORADEC_GETPACKETS) {
        data.clear();
        auto recent = getRecentPackets(4); // small for I2C
        for (const auto& p : recent) {
            // very compact: freq*4 (u16), sf, rssi+128, payload len (capped 8), first 8 of payload hex nibbles or raw
            uint16_t cf = (uint16_t)(p.freq_mhz * 4.0f);
            data.push_back(cf & 0xFF); data.push_back((cf>>8)&0xFF);
            data.push_back(p.sf);
            data.push_back((uint8_t)(p.rssi + 128));
            uint8_t n = std::min<uint8_t>((uint8_t)p.payload_hex.size()/2, 8);
            data.push_back(n);
            for (uint8_t i=0; i<n && i*2+1 < p.payload_hex.size(); ++i) {
                // take high nibble of each byte for tiny preview
                char c = p.payload_hex[i*2];
                uint8_t v = (c>='0'&&c<='9') ? c-'0' : (c>='a'&&c<='f'?10+c-'a':0);
                data.push_back(v);
            }
            if (n < 8) data.push_back(0);
        }
        return true;
    }
    return false;
}

bool EPAppLoraDecoder::OnWebData(std::string& data) {
    // Accept simple text commands or JSON-ish over websocket
    // Examples from browser:
    //   LORA:START
    //   LORA:STOP
    //   LORA:CONFIG:center=915.0,backend=0,radio_count=2
    //   LORA:INJECT:f=915.0,sf=12,bw=125000,rssi=-80,p=deadbeef
    if (data.rfind("LORA:", 0) != 0) return false;

    std::string cmd = data.substr(5);
    if (cmd == "START") {
        startSession();
        return true;
    }
    if (cmd == "STOP") {
        stopSession();
        return true;
    }
    if (cmd.rfind("CONFIG:", 0) == 0) {
        // naive parse
        LoraDecoderConfig nc = config;
        // very small parser for key=value pairs
        size_t pos = 7;
        while (pos < cmd.size()) {
            size_t eq = cmd.find('=', pos);
            if (eq == std::string::npos) break;
            size_t comma = cmd.find(',', eq);
            std::string key = cmd.substr(pos, eq - pos);
            std::string val = cmd.substr(eq+1, (comma==std::string::npos ? cmd.size() : comma) - (eq+1));
            if (key == "center") nc.center_mhz = strtof(val.c_str(), nullptr);
            else if (key == "backend") nc.backend = (uint8_t)atoi(val.c_str());
            else if (key == "radio_count") nc.radio_count = (uint8_t)std::clamp(atoi(val.c_str()), 0, 4);
            pos = (comma == std::string::npos ? cmd.size() : comma + 1);
        }
        setConfig(nc);
        return true;
    }
    if (cmd.rfind("INJECT:", 0) == 0) {
        // quick test packet injection from web
        LoraPacket p{};
        p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000);
        p.freq_mhz = config.center_mhz;
        p.sf = 12;
        p.bw_hz = 125000;
        p.rssi = -90;
        p.snr = 5;
        p.payload_hex = "cafebabe"; // default
        p.proto = 1;
        p.info = "web-inject";

        // very crude key=value after INJECT:
        std::string rest = cmd.substr(7);
        // support p=hex
        size_t ppos = rest.find("p=");
        if (ppos != std::string::npos) {
            p.payload_hex = rest.substr(ppos+2, 64);
            // truncate at first comma if present
            size_t c = p.payload_hex.find(',');
            if (c != std::string::npos) p.payload_hex.resize(c);
        }
        pushPacket(p);
        return true;
    }
    if (cmd == "STATUS") {
        sendStatusToWeb();
        return true;
    }
    if (cmd == "PACKETS") {
        sendPacketsToWeb(16);
        return true;
    }
    if (cmd.rfind("BACKEND:", 0) == 0) {
        int b = atoi(cmd.c_str() + 8);
        config.backend = (b >= 0 && b <= 2) ? (uint8_t)b : config.backend;
        if (running && config.backend == 2) lora_dsp_init();
        sendStatusToWeb();
        return true;
    }
    if (cmd.rfind("CHLIST:", 0) == 0) {
        // LORA:CHLIST:0=915.0,12,125000,1;1=915.2,11,250000,1
        // naive fill first slots
        // for full parse split; here accept simple and rearm if running
        sendStatusToWeb();
        return true;
    }
    if (cmd.rfind("KEYS:", 0) == 0) {
        strncpy(config.keys, cmd.c_str()+5, sizeof(config.keys)-1);
        config.keys[sizeof(config.keys)-1] = '\0';
        strncpy(g_lora_decode_keys, config.keys, sizeof(g_lora_decode_keys) - 1);
        g_lora_decode_keys[sizeof(g_lora_decode_keys) - 1] = '\0';
        sendStatusToWeb();
        return true;
    }
    if (cmd.rfind("FEED_TEST", 0) == 0) {
        // Synthesize a simple burst that the DSP can see (for validation of the vendored path)
        // Real HackRF path: feed actual sc16 samples captured from HackRF (via USB host, WiFi upload, etc.)
        const size_t N = 8192;
        std::vector<int16_t> synth(2*N);
        // very crude: put energy + a downchirp-like pattern at ~1/4 of the buffer
        for (size_t i=0; i<N; ++i) {
            float t = (float)i / 1000000.0f;
            float ph = 2 * 3.14159f * 50000.0f * t; // some tone inside BW
            float re = cosf(ph) * 0.3f;
            float im = sinf(ph) * 0.3f;
            synth[2*i]   = (int16_t)(re * 20000);
            synth[2*i+1] = (int16_t)(im * 20000);
        }
        feedIQ_sc16(synth.data(), N, 2000000.0f /*2 Msps*/, config.center_mhz);
        return true;
    }
    if (cmd.rfind("FEED_IQ:", 0) == 0) {
        // Placeholder for base64 sc16 upload from a capturer. For now treat as test.
        // In production you'd base64-decode here and call feedIQ_sc16.
        feedIQ_sc16(nullptr, 0, 2000000.0f, config.center_mhz); // no-op demo
        return true;
    }
    return false;
}

void EPAppLoraDecoder::sendStatusToWeb() {
    char buf[160];
    const char* hw = (config.backend == 1) ? "meshtonic_antennas" : (config.backend == 2 ? "esp32_dsp_iq" : "host_bridge");
    int ants = (config.backend == 1) ? numActiveRadios : 0;
    snprintf(buf, sizeof(buf),
             "{\"type\":\"loradec_status\",\"running\":%s,\"backend\":%u,\"radio_count\":%u,\"center\":%.3f,\"hw\":\"%s\",\"onboard_ants\":%d,\"slot0_sf\":%u}",
             running ? "true" : "false", config.backend, config.radio_count, config.center_mhz, hw, ants, (numActiveRadios>0? radioCfgs[0].sf : 0));
    SendDataToWeb(std::string(buf));
}

void EPAppLoraDecoder::sendPacketsToWeb(size_t count) {
    auto recent = getRecentPackets(count);
    std::string out = "{\"type\":\"loradec_packets\",\"packets\":[";
    for (size_t i=0; i<recent.size(); ++i) {
        const auto& p = recent[i];
        char pkt[256];
        snprintf(pkt, sizeof(pkt),
                 "%s{\"ts\":%" PRIu32 ",\"f\":%.3f,\"sf\":%u,\"bw\":%" PRIu32 ",\"rssi\":%d,\"payload\":\"%s\",\"info\":\"%s\"}",
                 (i>0?",":""), p.ts_ms, p.freq_mhz, p.sf, p.bw_hz, p.rssi,
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
    int ants = (config.backend==1 ? numActiveRadios : 0);
    snprintf(line, sizeof(line), "Onboard ants:%d  backend:%u", ants, config.backend);
    std::string text(line);

    if (!packets.empty()) {
        const auto& last = packets.back();
        snprintf(line, sizeof(line), "Last: SF%u %.1fMHz %ddBm", last.sf, last.freq_mhz, last.rssi);
        text += "\n";
        text += line;
    } else {
        text += "\nNo packets yet";
    }
    display->showMainTextMultiline(text);
}

void EPAppLoraDecoder::Loop(uint32_t currentMillis) {
    if (!running) return;

    // Drain any globally pushed packets (from HTTP bridge or other)
    if (!g_global_lora_packets.empty()) {
        for (const auto& gp : g_global_lora_packets) {
            pushPacket(gp);
        }
        g_global_lora_packets.clear();
    }

    // Onboard radios (Meshtonic antennas) polling when backend==1
    if (config.backend == 1) {
        // Drain wired IRQs (from MCP INTA) and poll as fallback
        sxManager.servicePendingIrqs();

        for (int i = 0; i < numActiveRadios; ++i) {
            auto* r = radios[i];
            if (!r) continue;

            bool activity = r->readDio1() || sxManager.hasPendingIrq(i);
            if (sxManager.hasPendingIrq(i)) sxManager.clearPendingIrq(i);

            if (activity) {
                uint8_t len = 0;
                uint8_t buf[256];
                int8_t rssi = -90, snr = 0;

                if (r->checkRxDone(&len, 255, buf, &rssi, &snr) && len > 0) {
                    LoraPacket p{};
                    p.ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
                    p.freq_mhz = radioCfgs[i].freq_hz / 1e6f;
                    p.bw_hz = radioCfgs[i].bw_hz;
                    p.sf = radioCfgs[i].sf;
                    p.rssi = rssi;
                    p.snr = snr;

                    char hex[513] = {0};
                    for (int k = 0; k < len && k < 256; ++k) {
                        sprintf(hex + k*2, "%02x", buf[k]);
                    }
                    p.payload_hex = hex;
                    p.proto = 1; // assume meshtastic-like for now
                    p.info = "onboard-cad-rx";

                    // Enrich via decode layer (port of LWD core)
                    uint8_t tmp[256];
                    size_t tlen = len;
                    if (tlen > sizeof(tmp)) tlen = sizeof(tmp);
                    memcpy(tmp, buf, tlen);
                    lora_decode_try_decrypt(tmp, tlen, g_lora_decode_keys[0] ? g_lora_decode_keys : nullptr);
                    lora_decode_parse_meshtastic(tmp, tlen, p);
                    lora_decode_dispatch_telemetry(0 /*port unknown here*/, tmp, tlen, p.info);

                    pushPacket(p);

                    // CAD policy (configurable in future NVS): prefer CAD for power; fallback continuous RX on busy channel.
                    // Default here: return to CAD after each packet.
                    r->startCad();
                } else {
                    // CAD done without full packet, or header only — restart CAD
                    r->startCad();
                }
            }
        }
    }

    // Periodic status push (throttled)
    if (currentMillis - lastPushMs > 1500) {
        lastPushMs = currentMillis;
        sendStatusToWeb();
    }
}

// Global feed implementation (called from webserver POST bridge etc.)
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

    // Also run onboard decode enrichment for host-bridge packets (unify path)
    uint8_t tmp[256] = {};
    size_t tlen = p.payload_hex.size()/2;
    if (tlen > sizeof(tmp)) tlen = sizeof(tmp);
    for (size_t k=0; k<tlen; k++) {
        unsigned v=0; sscanf(p.payload_hex.c_str()+k*2, "%02x", &v); tmp[k]=(uint8_t)v;
    }
    lora_decode_try_decrypt(tmp, tlen, g_lora_decode_keys[0] ? g_lora_decode_keys : nullptr);
    lora_decode_parse_meshtastic(tmp, tlen, p);

    if (g_global_lora_packets.size() >= GLOBAL_MAX) {
        g_global_lora_packets.erase(g_global_lora_packets.begin());
    }
    g_global_lora_packets.push_back(p);
}
