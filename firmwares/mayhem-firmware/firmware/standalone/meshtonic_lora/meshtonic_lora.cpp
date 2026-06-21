/*
 * Meshtonic LoRa — rich UI on HackRF screen; RF stays on ESP32 WIO shields.
 */

#include "meshtonic_lora.hpp"

#include <cstring>
#include <cstdio>
#include "ui/ui.hpp"

// Must match firmwares/mayhem-mdk/Source/main/pp_commands.hpp (rich UI commands).
#define PPCMD_LORADEC_STATUS   0xa020
#define PPCMD_LORADEC_PACKETS  0xa021
#define PPCMD_LORADEC_PRESETS  0xa022
#define PPCMD_LORADEC_APPLY    0xa023
#define PPCMD_LORADEC_CONTROL  0xa024

#pragma pack(push, 1)
struct lora_packet_compact_t {
    uint32_t ts_ms;
    float freq_mhz;
    uint32_t bw_hz;
    uint8_t sf;
    int16_t rssi;
    int8_t snr;
    uint8_t proto;
    uint8_t slot;
    uint8_t decrypted;
    uint8_t confidence[8];
    char region[8];
    char preset_id[20];
    uint8_t payload_preview[16];
    uint8_t payload_preview_len;
    char info[32];
};

struct lora_preset_entry_t {
    char id[32];
    char region[8];
    char profile[16];
    float freq_mhz;
    uint8_t sf;
    uint32_t bw_hz;
};

struct lora_rich_status_t {
    uint8_t running;
    uint8_t backend;
    uint8_t radio_count;
    uint8_t slot_present_mask;
    char active_preset[32];
    uint32_t total_packets;
    uint8_t num_recent;
    uint8_t pp_connected;
};
#pragma pack(pop)

static constexpr uint16_t kBg = 0x1082;
static constexpr uint16_t kFg = 0xFFFF;
static constexpr uint16_t kAccent = 0x07E0;
static constexpr uint16_t kWarn = 0xFD20;

static lora_rich_status_t g_status{};
static lora_packet_compact_t g_packets[8];
static size_t g_packet_count = 0;
static lora_preset_entry_t g_presets[16];
static size_t g_preset_count = 0;
static size_t g_selected_preset = 0;
static uint32_t g_frame = 0;
static bool g_need_paint = true;

static bool i2c_cmd_read(uint16_t cmd, uint8_t* data, size_t len) {
    uint8_t c[2] = {static_cast<uint8_t>(cmd & 0xff), static_cast<uint8_t>(cmd >> 8)};
    return _api->i2c_read(c, 2, data, len);
}

static bool i2c_cmd_write(uint16_t cmd, const uint8_t* payload, size_t payload_len) {
    uint8_t buf[96];
    if (payload_len + 2 > sizeof(buf)) {
        return false;
    }
    buf[0] = static_cast<uint8_t>(cmd & 0xff);
    buf[1] = static_cast<uint8_t>(cmd >> 8);
    if (payload && payload_len > 0) {
        memcpy(buf + 2, payload, payload_len);
    }
    return _api->i2c_read(buf, 2 + payload_len, nullptr, 0);
}

static void send_control(uint8_t op) {
    i2c_cmd_write(PPCMD_LORADEC_CONTROL, &op, 1);
}

static void apply_preset_index(size_t idx) {
    if (idx >= g_preset_count) {
        return;
    }
    const char* id = g_presets[idx].id;
    size_t id_len = 0;
    while (id_len < sizeof(g_presets[idx].id) && id[id_len] != '\0') {
        ++id_len;
    }
    i2c_cmd_write(PPCMD_LORADEC_APPLY, reinterpret_cast<const uint8_t*>(id), id_len + 1);
}

static void poll_esp() {
    lora_rich_status_t st{};
    if (i2c_cmd_read(PPCMD_LORADEC_STATUS, reinterpret_cast<uint8_t*>(&st), sizeof(st))) {
        g_status = st;
    }

    uint8_t want = 6;
    uint8_t pkt_cmd[3] = {
        static_cast<uint8_t>(PPCMD_LORADEC_PACKETS & 0xff),
        static_cast<uint8_t>(PPCMD_LORADEC_PACKETS >> 8),
        want};
  _api->i2c_read(pkt_cmd, 3, nullptr, 0);

    g_packet_count = 0;
    lora_packet_compact_t tmp[8];
    size_t want_bytes = want * sizeof(lora_packet_compact_t);
    if (i2c_cmd_read(PPCMD_LORADEC_PACKETS, reinterpret_cast<uint8_t*>(tmp), want_bytes)) {
        g_packet_count = want_bytes / sizeof(lora_packet_compact_t);
        for (size_t i = 0; i < g_packet_count; ++i) {
            g_packets[i] = tmp[i];
        }
    }

    lora_preset_entry_t presets[16];
    if (i2c_cmd_read(PPCMD_LORADEC_PRESETS, reinterpret_cast<uint8_t*>(presets), sizeof(presets))) {
        g_preset_count = sizeof(presets) / sizeof(presets[0]);
        for (size_t i = 0; i < g_preset_count; ++i) {
            g_presets[i] = presets[i];
        }
        if (g_selected_preset >= g_preset_count) {
            g_selected_preset = 0;
        }
    }

    g_need_paint = true;
    _api->set_dirty();
}

static void draw_line(ui::Painter& painter, int y, const char* text, uint16_t color) {
    painter.draw_string({4, y}, ui::font::fixed_5x8(), ui::Color(color), ui::Color(kBg), text);
}

void initialize(const standalone_application_api_t& api) {
    _api = &api;
    screen_height = *(_api->screen_height);
    screen_width = *(_api->screen_width);
    memset(&g_status, 0, sizeof(g_status));
    g_packet_count = 0;
    g_preset_count = 0;
    g_selected_preset = 0;
    g_frame = 0;
    g_need_paint = true;
    poll_esp();
}

void on_event(const uint32_t& events) {
    if ((events & 1) == 1) {
        ++g_frame;
        if (g_frame % 30 == 0) {
            poll_esp();
        }
        if (g_need_paint) {
            _api->set_dirty();
        }
    }
}

void shutdown() {
}

void PaintViewMirror() {
    ui::Painter painter;
    _api->fill_rectangle(0, 16, screen_width, screen_height - 16, kBg);

    draw_line(painter, 20, "Meshtonic LoRa (WIO)", kAccent);

    char line[64];
    snprintf(line, sizeof(line), "%s preset:%s slots:0x%02x",
             g_status.running ? "RUN" : "STOP",
             g_status.active_preset[0] ? g_status.active_preset : "-",
             g_status.slot_present_mask);
    draw_line(painter, 30, line, kFg);

    snprintf(line, sizeof(line), "pkts:%u radios:%u backend:%u",
             static_cast<unsigned>(g_status.total_packets),
             static_cast<unsigned>(g_status.radio_count),
             static_cast<unsigned>(g_status.backend));
    draw_line(painter, 40, line, kFg);

    if (g_preset_count > 0) {
        snprintf(line, sizeof(line), "preset[%u]: %s",
                 static_cast<unsigned>(g_selected_preset + 1),
                 g_presets[g_selected_preset].id);
        draw_line(painter, 50, line, kWarn);
    } else {
        draw_line(painter, 50, "preset: (none)", kWarn);
    }

    draw_line(painter, 60, "L=start R=stop SEL=apply enc=cycle", kFg);

    int y = 72;
    for (size_t i = 0; i < g_packet_count && y < static_cast<int>(screen_height) - 10; ++i) {
        const auto& p = g_packets[i];
        snprintf(line, sizeof(line), "S%u %.3f %ddB %s",
                 static_cast<unsigned>(p.slot),
                 static_cast<double>(p.freq_mhz),
                 static_cast<int>(p.rssi),
                 p.info[0] ? p.info : "-");
        draw_line(painter, y, line, kFg);
        y += 10;
    }

    g_need_paint = false;
}

void OnFocus() {
    g_need_paint = true;
    _api->set_dirty();
}

bool OnKeyEvent(uint8_t key_val) {
    ui::KeyEvent key = static_cast<ui::KeyEvent>(key_val);
    if (key == ui::KeyEvent::Left) {
        send_control(0);
        poll_esp();
        return true;
    }
    if (key == ui::KeyEvent::Right) {
        send_control(1);
        poll_esp();
        return true;
    }
    if (key == ui::KeyEvent::Select) {
        apply_preset_index(g_selected_preset);
        poll_esp();
        return true;
    }
    if (key == ui::KeyEvent::Up) {
        if (g_preset_count > 0) {
            g_selected_preset = (g_selected_preset + g_preset_count - 1) % g_preset_count;
            g_need_paint = true;
            _api->set_dirty();
        }
        return true;
    }
    if (key == ui::KeyEvent::Down) {
        if (g_preset_count > 0) {
            g_selected_preset = (g_selected_preset + 1) % g_preset_count;
            g_need_paint = true;
            _api->set_dirty();
        }
        return true;
    }
    return false;
}

bool OnEncoder(int32_t delta) {
    if (g_preset_count == 0 || delta == 0) {
        return false;
    }
    if (delta > 0) {
        g_selected_preset = (g_selected_preset + 1) % g_preset_count;
    } else {
        g_selected_preset = (g_selected_preset + g_preset_count - 1) % g_preset_count;
    }
    g_need_paint = true;
    _api->set_dirty();
    return true;
}

bool OnTouchEvent(int, int, uint32_t) {
    return false;
}

bool OnKeyboad(uint8_t) {
    return false;
}
