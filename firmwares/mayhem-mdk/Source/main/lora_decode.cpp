#include "lora_decode.h"
#include "apps/ep_app_loradecoder.hpp"
#include "nvs.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <strings.h>

#if defined(ESP_PLATFORM)
#include "mbedtls/aes.h"
#endif

static LoraDecodeKeyConfig g_key_cfg;
static char g_legacy_keys[128] = {};

static const uint8_t MESH_AES_KEY[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
};

static const int KNOWN_PORTNUMS[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 32, 33, 34, 64, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 256, 257,
};
static const int TEXT_PORTNUMS[] = {1, 10, 32, 66};

static inline uint16_t crc16_ccitt(const uint8_t* d, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)d[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

static inline uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static bool parse_hex_key(const char* s, uint8_t* out, size_t* out_len, size_t max_len) {
    size_t n = 0;
    int hi = -1;
    for (; *s; s++) {
        if (*s == ' ' || *s == ',' || *s == ';') continue;
        int v = hex_nibble(*s);
        if (v < 0) return false;
        if (hi < 0) { hi = v; continue; }
        if (n >= max_len) return false;
        out[n++] = (uint8_t)((hi << 4) | v);
        hi = -1;
    }
    if (hi >= 0) return false;
    *out_len = n;
    return n > 0;
}

static bool parse_base64_key(const char* s, uint8_t* out, size_t* out_len, size_t max_len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (; *s; s++) {
        if (*s == '=' || *s == ' ' || *s == '\n' || *s == '\r') continue;
        const char* p = strchr(tbl, *s);
        if (!p) return false;
        acc = (acc << 6) | (uint32_t)(p - tbl);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= max_len) return false;
            out[n++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    *out_len = n;
    return n > 0;
}

void lora_decode_keys_init_defaults() {
    memset(&g_key_cfg, 0, sizeof(g_key_cfg));
    g_key_cfg.try_default = true;
    g_key_cfg.default_priority = 50;
    g_key_cfg.entry_count = 0;
    g_legacy_keys[0] = '\0';
}

const LoraDecodeKeyConfig& lora_decode_keys_get() { return g_key_cfg; }

void lora_decode_keys_set(const LoraDecodeKeyConfig& cfg) {
    g_key_cfg = cfg;
    if (g_key_cfg.entry_count > LORA_DECODE_MAX_KEYS) g_key_cfg.entry_count = LORA_DECODE_MAX_KEYS;
}

static bool save_keys_blob() {
    nvs_handle_t h;
    if (nvs_open("lora_keys", NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_blob(h, "cfg", &g_key_cfg, sizeof(g_key_cfg));
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool lora_decode_keys_save_nvs() { return save_keys_blob(); }

bool lora_decode_keys_load_nvs() {
    lora_decode_keys_init_defaults();
    nvs_handle_t h;
    if (nvs_open("lora_keys", NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = sizeof(g_key_cfg);
    esp_err_t e = nvs_get_blob(h, "cfg", &g_key_cfg, &sz);
    nvs_close(h);
    if (e != ESP_OK || sz != sizeof(g_key_cfg)) {
        lora_decode_keys_init_defaults();
        return false;
    }
    if (g_key_cfg.entry_count > LORA_DECODE_MAX_KEYS) g_key_cfg.entry_count = LORA_DECODE_MAX_KEYS;
    return true;
}

void lora_decode_keys_set_legacy_string(const char* keys) {
    if (!keys) {
        g_legacy_keys[0] = '\0';
        return;
    }
    strncpy(g_legacy_keys, keys, sizeof(g_legacy_keys) - 1);
    g_legacy_keys[sizeof(g_legacy_keys) - 1] = '\0';

    if (g_legacy_keys[0] == '!' || g_legacy_keys[0] == '\0') return;

    LoraDecodeKeyEntry e{};
    strncpy(e.label, "legacy", sizeof(e.label) - 1);
    size_t klen = 0;
    if (strchr(g_legacy_keys, '+') || strchr(g_legacy_keys, '/') || strchr(g_legacy_keys, '=')) {
        if (!parse_base64_key(g_legacy_keys, e.key, &klen, sizeof(e.key))) return;
    } else {
        if (!parse_hex_key(g_legacy_keys, e.key, &klen, sizeof(e.key))) return;
    }
    e.key_len = (uint8_t)klen;
    e.scope = LORA_KEY_SCOPE_ALL;
    e.priority = 60;
    e.enabled = true;
    if (g_key_cfg.entry_count < LORA_DECODE_MAX_KEYS) {
        g_key_cfg.entries[g_key_cfg.entry_count++] = e;
    }
}

static void trim_inplace(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static bool parse_key_entry_kv(const char* segment, LoraDecodeKeyEntry& e) {
    char buf[256];
    strncpy(buf, segment, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    memset(&e, 0, sizeof(e));
    e.enabled = true;
    e.priority = 50;
    e.scope = LORA_KEY_SCOPE_ALL;

    char* save = nullptr;
    char* tok = strtok_r(buf, ",", &save);
    while (tok) {
        trim_inplace(tok);
        char* eq = strchr(tok, '=');
        if (!eq) { tok = strtok_r(nullptr, ",", &save); continue; }
        *eq = '\0';
        char* key = tok;
        char* val = eq + 1;
        trim_inplace(key);
        trim_inplace(val);
        if (strcmp(key, "label") == 0) {
            strncpy(e.label, val, sizeof(e.label) - 1);
        } else if (strcmp(key, "key") == 0) {
            size_t klen = 0;
            if (strchr(val, '+') || strchr(val, '/') || strchr(val, '=')) {
                if (!parse_base64_key(val, e.key, &klen, sizeof(e.key))) return false;
            } else {
                if (!parse_hex_key(val, e.key, &klen, sizeof(e.key))) return false;
            }
            e.key_len = (uint8_t)klen;
        } else if (strcmp(key, "scope") == 0) {
            e.scope = (strcmp(val, "nodes") == 0) ? LORA_KEY_SCOPE_NODES : LORA_KEY_SCOPE_ALL;
        } else if (strcmp(key, "priority") == 0) {
            e.priority = (uint8_t)atoi(val);
        } else if (strcmp(key, "enabled") == 0) {
            e.enabled = (val[0] == '1' || val[0] == 't' || val[0] == 'T' || val[0] == 'y');
        } else if (strcmp(key, "nodes") == 0) {
            char nb[256];
            strncpy(nb, val, sizeof(nb) - 1);
            char* ns = nullptr;
            char* nt = strtok_r(nb, "|", &ns);
            while (nt && e.node_count < LORA_DECODE_MAX_NODES_PER_KEY) {
                trim_inplace(nt);
                if (nt[0] != '!') {
                    char tmp[16];
                    snprintf(tmp, sizeof(tmp), "!%s", nt);
                    strncpy(e.node_ids[e.node_count], tmp, 11);
                } else {
                    strncpy(e.node_ids[e.node_count], nt, 11);
                }
                e.node_ids[e.node_count][11] = '\0';
                e.node_count++;
                nt = strtok_r(nullptr, "|", &ns);
            }
        }
        tok = strtok_r(nullptr, ",", &save);
    }
    return e.key_len > 0;
}

bool lora_decode_keys_parse_web(const char* keys_body) {
    if (!keys_body) return false;
    if (keys_body[0] == '!') {
        lora_decode_keys_init_defaults();
        g_legacy_keys[0] = '!';
        g_legacy_keys[1] = '\0';
        save_keys_blob();
        return true;
    }
    if (strcmp(keys_body, "default") == 0) {
        lora_decode_keys_init_defaults();
        save_keys_blob();
        return true;
    }
    if (strncmp(keys_body, "legacy=", 7) == 0) {
        lora_decode_keys_init_defaults();
        lora_decode_keys_set_legacy_string(keys_body + 7);
        save_keys_blob();
        return true;
    }

  // Plain base64/hex only (no semicolons)
    if (strchr(keys_body, ';') == nullptr && strchr(keys_body, '=') == nullptr) {
        lora_decode_keys_init_defaults();
        lora_decode_keys_set_legacy_string(keys_body);
        save_keys_blob();
        return true;
    }

    LoraDecodeKeyConfig nc;
    memset(&nc, 0, sizeof(nc));
    nc.try_default = true;
    nc.default_priority = 50;
    char buf[512];
    strncpy(buf, keys_body, sizeof(buf) - 1);
    char* save = nullptr;
    char* seg = strtok_r(buf, ";", &save);
    while (seg && nc.entry_count < LORA_DECODE_MAX_KEYS) {
        trim_inplace(seg);
        if (strncmp(seg, "try_default=", 12) == 0) {
            nc.try_default = (seg[12] != '0' && seg[12] != 'f');
        } else if (strncmp(seg, "default_priority=", 17) == 0) {
            nc.default_priority = (uint8_t)atoi(seg + 17);
        } else {
            LoraDecodeKeyEntry e{};
            if (parse_key_entry_kv(seg, e)) nc.entries[nc.entry_count++] = e;
        }
        seg = strtok_r(nullptr, ";", &save);
    }
    g_key_cfg = nc;
    save_keys_blob();
    return true;
}

void lora_build_meshtastic_nonce(uint32_t pkt_id, uint32_t sender, uint8_t nonce[16]) {
    memset(nonce, 0, 16);
    nonce[0] = (uint8_t)(pkt_id & 0xFF);
    nonce[1] = (uint8_t)((pkt_id >> 8) & 0xFF);
    nonce[2] = (uint8_t)((pkt_id >> 16) & 0xFF);
    nonce[3] = (uint8_t)((pkt_id >> 24) & 0xFF);
    nonce[8] = (uint8_t)(sender & 0xFF);
    nonce[9] = (uint8_t)((sender >> 8) & 0xFF);
    nonce[10] = (uint8_t)((sender >> 16) & 0xFF);
    nonce[11] = (uint8_t)((sender >> 24) & 0xFF);
}

bool lora_aes_ctr_decrypt(const uint8_t* key, size_t key_len,
                          const uint8_t nonce[16],
                          const uint8_t* in, size_t in_len, uint8_t* out) {
    if (!key || !nonce || !in || !out || key_len != 16) return false;
#if defined(ESP_PLATFORM)
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    if (mbedtls_aes_setkey_enc(&ctx, key, 128) != 0) {
        mbedtls_aes_free(&ctx);
        return false;
    }
    uint8_t stream_block[16] = {};
    uint8_t nc[16];
    memcpy(nc, nonce, 16);
    size_t nc_off = 0;
    int rc = mbedtls_aes_crypt_ctr(&ctx, in_len, &nc_off, nc, stream_block, in, out);
    mbedtls_aes_free(&ctx);
    return rc == 0;
#else
    (void)key_len;
    return false;
#endif
}

bool lora_is_known_portnum(int portnum) {
    for (size_t i = 0; i < sizeof(KNOWN_PORTNUMS) / sizeof(KNOWN_PORTNUMS[0]); i++) {
        if (KNOWN_PORTNUMS[i] == portnum) return true;
    }
    return false;
}

bool lora_is_text_portnum(int portnum) {
    for (size_t i = 0; i < sizeof(TEXT_PORTNUMS) / sizeof(TEXT_PORTNUMS[0]); i++) {
        if (TEXT_PORTNUMS[i] == portnum) return true;
    }
    return false;
}

bool lora_utf8_valid(const uint8_t* data, size_t len) {
    if (!data) return false;
    size_t i = 0;
    while (i < len) {
        uint8_t c = data[i];
        if (c <= 0x7F) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len || data[i + 1] < 0x80 || data[i + 1] > 0xBF) return false;
            i += 2; continue;
        }
        if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= len) return false;
            uint8_t c1 = data[i + 1], c2 = data[i + 2];
            if (c1 < 0x80 || c1 > 0xBF || c2 < 0x80 || c2 > 0xBF) return false;
            if (c == 0xE0 && c1 < 0xA0) return false;
            if (c == 0xED && c1 > 0x9F) return false;
            i += 3; continue;
        }
        if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= len) return false;
            uint8_t c1 = data[i + 1], c2 = data[i + 2], c3 = data[i + 3];
            if (c1 < 0x80 || c1 > 0xBF || c2 < 0x80 || c2 > 0xBF || c3 < 0x80 || c3 > 0xBF) return false;
            if (c == 0xF0 && c1 < 0x90) return false;
            if (c == 0xF4 && c1 > 0x8F) return false;
            i += 4; continue;
        }
        return false;
    }
    return true;
}

size_t lora_parse_protobuf(const uint8_t* data, size_t len,
                           LoraProtoField* fields, size_t max_fields) {
    size_t count = 0;
    size_t pos = 0;
    while (pos < len && count < max_fields) {
        uint32_t tag = 0;
        int shift = 0;
        while (pos < len) {
            uint8_t b = data[pos++];
            tag |= (uint32_t)(b & 0x7F) << shift;
            shift += 7;
            if ((b & 0x80) == 0) break;
            if (shift > 28) return count;
        }
        uint32_t field_num = tag >> 3;
        uint8_t wire_type = (uint8_t)(tag & 7);
        LoraProtoField& f = fields[count];
        f.field_num = field_num;
        f.wire_type = wire_type;
        f.varint_val = 0;
        f.bytes_val = nullptr;
        f.bytes_len = 0;

        if (wire_type == 0) {
            uint32_t val = 0;
            shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                val |= (uint32_t)(b & 0x7F) << shift;
                shift += 7;
                if ((b & 0x80) == 0) break;
            }
            f.varint_val = val;
        } else if (wire_type == 2) {
            uint32_t flen = 0;
            shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                flen |= (uint32_t)(b & 0x7F) << shift;
                shift += 7;
                if ((b & 0x80) == 0) break;
            }
            if (pos + flen > len) return count;
            f.bytes_val = data + pos;
            f.bytes_len = flen;
            pos += flen;
        } else if (wire_type == 5) {
            if (pos + 4 > len) return count;
            f.varint_val = read_le32(data + pos);
            pos += 4;
        } else {
            return count;
        }
        count++;
    }
    return count;
}

void lora_node_id_str(const uint8_t air4[4], char* out, size_t out_cap) {
    if (!out || out_cap < 10 || !air4) return;
    snprintf(out, out_cap, "!%02x%02x%02x%02x", air4[3], air4[2], air4[1], air4[0]);
}

static bool key_applies(const LoraDecodeKeyEntry& e, const char* sender_str) {
    if (!e.enabled || e.key_len == 0) return false;
    if (e.scope == LORA_KEY_SCOPE_ALL) return true;
    if (!sender_str) return false;
    for (uint8_t i = 0; i < e.node_count; i++) {
        if (strcasecmp(e.node_ids[i], sender_str) == 0) return true;
    }
    return false;
}

struct KeyCand {
    uint8_t priority;
    int tie;
    const uint8_t* key;
    uint8_t key_len;
    const char* label;
};

static void collect_key_candidates(const char* sender_str, KeyCand* cands, size_t* n_cands, size_t max_cands) {
    *n_cands = 0;
    for (uint8_t i = 0; i < g_key_cfg.entry_count && *n_cands < max_cands; i++) {
        const auto& e = g_key_cfg.entries[i];
        if (!key_applies(e, sender_str)) continue;
        uint8_t prio = e.priority;
        if (prio == 0 && e.scope == LORA_KEY_SCOPE_ALL) prio = 100;
        KeyCand& c = cands[*n_cands];
        c.priority = prio;
        c.tie = (int)i;
        c.key = e.key;
        c.key_len = e.key_len;
        c.label = e.label[0] ? e.label : "custom";
        (*n_cands)++;
    }
    if (g_key_cfg.try_default && *n_cands < max_cands) {
        KeyCand& c = cands[*n_cands];
        c.priority = g_key_cfg.default_priority;
        c.tie = -1;
        c.key = MESH_AES_KEY;
        c.key_len = 16;
        c.label = "default";
        (*n_cands)++;
    }
    std::sort(cands, cands + *n_cands, [](const KeyCand& a, const KeyCand& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.tie < b.tie;
    });
}

static const char* portnum_short_name(int pn) {
    switch (pn) {
        case 1: return "TEXT";
        case 3: return "POSITION";
        case 4: return "NODEINFO";
        case 5: return "ROUTING";
        case 67: return "TELEMETRY";
        case 70: return "TRACEROUTE";
        case 71: return "NEIGHBORINFO";
        default: return nullptr;
    }
}

static void enrich_inner(int portnum, const uint8_t* inner, size_t inner_len, char* summary, size_t cap) {
    if (!summary || cap == 0) return;
    if (!inner || inner_len == 0) return;

    if (lora_is_text_portnum(portnum)) {
        size_t n = inner_len < cap - 8 ? inner_len : cap - 8;
        snprintf(summary, cap, " text:\"");
        size_t base = strlen(summary);
        for (size_t i = 0; i < n && base + 1 < cap; i++) {
            char c = (char)inner[i];
            if (c >= 32 && c < 127) summary[base++] = c;
            else summary[base++] = '.';
        }
        if (base + 2 < cap) { summary[base++] = '"'; summary[base] = '\0'; }
        return;
    }

    LoraProtoField fields[24];
    size_t nf = lora_parse_protobuf(inner, inner_len, fields, 24);

    if (portnum == 3) {
        int32_t lat_i = 0, lon_i = 0;
        int alt = 0;
        for (size_t i = 0; i < nf; i++) {
            if (fields[i].field_num == 1 && fields[i].wire_type == 5) lat_i = (int32_t)fields[i].varint_val;
            if (fields[i].field_num == 2 && fields[i].wire_type == 5) lon_i = (int32_t)fields[i].varint_val;
            if (fields[i].field_num == 3 && fields[i].wire_type == 0) alt = (int)fields[i].varint_val;
        }
        snprintf(summary, cap, " pos:%.5f,%.5f alt:%d", lat_i * 1e-7, lon_i * 1e-7, alt);
        return;
    }

    if (portnum == 4) {
        char longname[32] = {}, shortname[12] = {};
        for (size_t i = 0; i < nf; i++) {
            if (fields[i].field_num == 2 && fields[i].wire_type == 2 && fields[i].bytes_len < 31) {
                memcpy(longname, fields[i].bytes_val, fields[i].bytes_len);
                longname[fields[i].bytes_len] = '\0';
            }
            if (fields[i].field_num == 3 && fields[i].wire_type == 2 && fields[i].bytes_len < 11) {
                memcpy(shortname, fields[i].bytes_val, fields[i].bytes_len);
                shortname[fields[i].bytes_len] = '\0';
            }
        }
        snprintf(summary, cap, " node:%s/%s", shortname[0] ? shortname : "?", longname[0] ? longname : "?");
        return;
    }

    if (portnum == 67) {
        for (size_t i = 0; i < nf; i++) {
            if (fields[i].field_num == 1 && fields[i].wire_type == 2) {
                LoraProtoField sub[12];
                size_t sn = lora_parse_protobuf(fields[i].bytes_val, fields[i].bytes_len, sub, 12);
                for (size_t j = 0; j < sn; j++) {
                    if (sub[j].field_num == 1 && sub[j].wire_type == 0) {
                        snprintf(summary, cap, " batt:%u%%", (unsigned)sub[j].varint_val);
                        return;
                    }
                }
            }
        }
        snprintf(summary, cap, " telemetry");
        return;
    }

    const char* pn = portnum_short_name(portnum);
    if (pn) snprintf(summary, cap, " %s", pn);
}

static bool try_decrypt_validate(const uint8_t* enc, size_t enc_len,
                                 uint32_t pkt_id, uint32_t sender_le, const char* sender_str,
                                 uint8_t* decrypted, size_t* dec_len,
                                 int* portnum, const uint8_t** inner, size_t* inner_len,
                                 char* key_label, size_t label_cap) {
    uint8_t nonce[16];
    lora_build_meshtastic_nonce(pkt_id, sender_le, nonce);

    KeyCand cands[16];
    size_t n_cands = 0;
    collect_key_candidates(sender_str, cands, &n_cands, 16);

    for (size_t ci = 0; ci < n_cands; ci++) {
        if (cands[ci].key_len != 16) continue;
        if (!lora_aes_ctr_decrypt(cands[ci].key, 16, nonce, enc, enc_len, decrypted)) continue;

        LoraProtoField fields[16];
        size_t nf = lora_parse_protobuf(decrypted, enc_len, fields, 16);
        int pn = -1;
        const uint8_t* in_payload = nullptr;
        size_t in_len_v = 0;
        for (size_t i = 0; i < nf; i++) {
            if (fields[i].field_num == 1 && fields[i].wire_type == 0) pn = (int)fields[i].varint_val;
            if (fields[i].field_num == 2 && fields[i].wire_type == 2) {
                in_payload = fields[i].bytes_val;
                in_len_v = fields[i].bytes_len;
            }
        }
        if (pn < 0 || !lora_is_known_portnum(pn)) continue;
        if (lora_is_text_portnum(pn) && in_payload && in_len_v > 0 && !lora_utf8_valid(in_payload, in_len_v)) continue;

        if (portnum) *portnum = pn;
        if (inner) *inner = in_payload;
        if (inner_len) *inner_len = in_len_v;
        if (dec_len) *dec_len = enc_len;
        if (key_label && label_cap) snprintf(key_label, label_cap, "%s", cands[ci].label);
        return true;
    }
    return false;
}

LoraDecodeOutcome lora_decode_process_air(const uint8_t* raw, size_t rawLen, LoraPacket& pkt) {
    LoraDecodeOutcome out{};
    if (!raw || rawLen < 16) return out;

    out.header_ok = true;
    out.to = read_le32(raw);
    out.from = read_le32(raw + 4);
    out.pkt_id = read_le32(raw + 8);
    out.flags = raw[12];
    out.channel_hash = raw[13];
    lora_node_id_str(raw + 4, out.node_from, sizeof(out.node_from));
    bool broadcast = (raw[0] == 0xFF && raw[1] == 0xFF && raw[2] == 0xFF && raw[3] == 0xFF);
    if (broadcast) snprintf(out.node_to, sizeof(out.node_to), "broadcast");
    else lora_node_id_str(raw, out.node_to, sizeof(out.node_to));

    char info[200];
    snprintf(info, sizeof(info), "%s->%s id:%08x ch:%02x",
             out.node_from, out.node_to, (unsigned)out.pkt_id, out.channel_hash);

    const uint8_t* enc = raw + 16;
    size_t enc_len = rawLen - 16;
    if (enc_len == 0) {
        pkt.info = info;
        pkt.proto = 1;
        return out;
    }

    uint8_t decrypted[256];
    size_t dec_len = 0;
    int portnum = -1;
    const uint8_t* inner = nullptr;
    size_t inner_len = 0;
    char key_label[24] = {};

    if (g_legacy_keys[0] == '!') {
        out.decrypted = true;
        out.valid = true;
        pkt.info = std::string(out.node_from) + "->plaintext";
        pkt.proto = 1;
        return out;
    }

    if (try_decrypt_validate(enc, enc_len, out.pkt_id, out.from, out.node_from,
                             decrypted, &dec_len, &portnum, &inner, &inner_len,
                             key_label, sizeof(key_label))) {
        out.decrypted = true;
        out.valid = true;
        out.portnum = portnum;
        strncpy(out.key_label, key_label, sizeof(out.key_label) - 1);

        const char* pn = portnum_short_name(portnum);
        snprintf(info, sizeof(info), "%s->%s %s key:%s",
                 out.node_from, out.node_to, pn ? pn : "port", key_label);

        char enrich[96] = {};
        enrich_inner(portnum, inner, inner_len, enrich, sizeof(enrich));
        if (enrich[0]) strncat(info, enrich, sizeof(info) - strlen(info) - 1);

        // Update payload_hex to decrypted bytes for web display
        char hex[513] = {};
        size_t hl = dec_len < 256 ? dec_len : 256;
        for (size_t k = 0; k < hl; k++) sprintf(hex + k * 2, "%02x", decrypted[k]);
        pkt.payload_hex = hex;
    } else {
        out.encrypted_only = true;
        snprintf(info, sizeof(info), "%s->%s encrypted (no matching key)",
                 out.node_from, out.node_to);
    }

    pkt.info = info;
    pkt.proto = 1;
    return out;
}

bool lora_decode_parse_meshtastic(const uint8_t* raw, size_t rawLen, LoraPacket& pkt) {
    auto out = lora_decode_process_air(raw, rawLen, pkt);
    return out.header_ok;
}

bool lora_decode_try_decrypt(uint8_t* buf, size_t len, const char* keys, int /*hint*/) {
    if (!buf || len < 16) return false;
    if (keys) lora_decode_keys_set_legacy_string(keys);
    LoraPacket dummy{};
    auto out = lora_decode_process_air(buf, len, dummy);
    if (out.valid && out.decrypted) {
        uint8_t nonce[16];
        lora_build_meshtastic_nonce(out.pkt_id, out.from, nonce);
        const uint8_t* enc = buf + 16;
        size_t enc_len = len - 16;
        KeyCand cands[16];
        size_t n = 0;
        collect_key_candidates(out.node_from, cands, &n, 16);
        for (size_t i = 0; i < n; i++) {
            if (strcmp(cands[i].label, out.key_label) == 0 && cands[i].key_len == 16) {
                return lora_aes_ctr_decrypt(cands[i].key, 16, nonce, enc, enc_len, buf + 16);
            }
        }
    }
    return out.valid;
}

void lora_decode_dispatch_telemetry(uint8_t port, const uint8_t* payload, size_t len, std::string& outInfo) {
    char s[96] = {};
    enrich_inner(port, payload, len, s, sizeof(s));
    if (s[0]) outInfo += s;
}

void lora_dewhiten(uint8_t* data, size_t len, uint16_t /*nonce*/) {
    (void)data; (void)len;
}

bool lora_crc16_ok(const uint8_t* data, size_t len, uint16_t rxCrc) {
    if (len < 2) return true;
    return crc16_ccitt(data, len - 2) == rxCrc;
}

void lora_make_preview(const uint8_t* data, size_t len, char* out, size_t outCap) {
    if (!out || outCap == 0) return;
    size_t n = (len < outCap / 2 - 1) ? len : outCap / 2 - 1;
    for (size_t i = 0; i < n; i++) sprintf(out + i * 2, "%02x", data[i]);
    out[n * 2] = 0;
}

#if defined(LORA_DECODE_HOST_TEST)
int lora_decode_run_selftests() {
    int fails = 0;
    uint8_t nonce[16];
    lora_build_meshtastic_nonce(0xAABBCCDD, 0x12345678, nonce);
    const uint8_t expect_nonce[] = {0xDD, 0xCC, 0xBB, 0xAA, 0, 0, 0, 0, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    if (memcmp(nonce, expect_nonce, 16) != 0) fails++;

    const uint8_t enc[] = {0x89, 0x42, 0xc5, 0xb5, 0x6a, 0xe2};
    uint8_t dec[16] = {};
    if (!lora_aes_ctr_decrypt(MESH_AES_KEY, 16, nonce, enc, sizeof(enc), dec)) fails++;
    const uint8_t expect_plain[] = {0x08, 0x01, 0x12, 0x02, 0x68, 0x69};
    if (memcmp(dec, expect_plain, sizeof(expect_plain)) != 0) fails++;

    LoraProtoField fields[8];
    size_t nf = lora_parse_protobuf(dec, sizeof(expect_plain), fields, 8);
    int pn = -1;
    const uint8_t* inner = nullptr;
    size_t inner_len = 0;
    for (size_t i = 0; i < nf; i++) {
        if (fields[i].field_num == 1 && fields[i].wire_type == 0) pn = (int)fields[i].varint_val;
        if (fields[i].field_num == 2 && fields[i].wire_type == 2) {
            inner = fields[i].bytes_val;
            inner_len = fields[i].bytes_len;
        }
    }
    if (pn != 1 || !inner || inner_len != 2 || inner[0] != 'h' || inner[1] != 'i') fails++;

    lora_decode_keys_init_defaults();
    const uint8_t pkt[] = {
        0xff, 0xff, 0xff, 0xff, 0x78, 0x56, 0x34, 0x12,
        0xdd, 0xcc, 0xbb, 0xaa, 0x07, 0x00, 0x00, 0x00,
        0x89, 0x42, 0xc5, 0xb5, 0x6a, 0xe2
    };
    LoraPacket lp{};
    auto outcome = lora_decode_process_air(pkt, sizeof(pkt), lp);
    if (!outcome.valid || outcome.portnum != 1) fails++;
    if (strstr(lp.info.c_str(), "hi") == nullptr) fails++;

    return fails;
}
#endif
