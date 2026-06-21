#include "lora_bridge_json.h"
#include <cstdio>
#include <cstring>

static const char* find_key(const char* json, const char* key) {
    if (!json || !key) return nullptr;
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return nullptr;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    return p;
}

bool lora_json_get_string(const char* json, const char* key, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return false;
    out[0] = '\0';
    const char* p = find_key(json, key);
    if (!p || *p != '"') return false;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return false;
    size_t n = (size_t)(end - p);
    if (n >= out_cap) n = out_cap - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

bool lora_json_get_float(const char* json, const char* key, float* out) {
    if (!out) return false;
    const char* p = find_key(json, key);
    if (!p) return false;
    if (*p == '"') {
        char tmp[32];
        if (!lora_json_get_string(json, key, tmp, sizeof(tmp))) return false;
        *out = strtof(tmp, nullptr);
        return true;
    }
    *out = strtof(p, nullptr);
    return true;
}

bool lora_json_get_int(const char* json, const char* key, int* out) {
    if (!out) return false;
    const char* p = find_key(json, key);
    if (!p) return false;
    *out = atoi(p);
    return true;
}

bool lora_json_get_bool(const char* json, const char* key, bool* out) {
    if (!out) return false;
    const char* p = find_key(json, key);
    if (!p) return false;
    if (strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    *out = atoi(p) != 0;
    return true;
}
