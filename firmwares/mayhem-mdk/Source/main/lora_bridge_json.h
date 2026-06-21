#ifndef LORA_BRIDGE_JSON_H
#define LORA_BRIDGE_JSON_H

#include <cstddef>
#include <cstdint>

// Minimal JSON field extraction for POST /lwd/packet (no full parser dependency).
bool lora_json_get_string(const char* json, const char* key, char* out, size_t out_cap);
bool lora_json_get_float(const char* json, const char* key, float* out);
bool lora_json_get_int(const char* json, const char* key, int* out);
bool lora_json_get_bool(const char* json, const char* key, bool* out);

#endif
