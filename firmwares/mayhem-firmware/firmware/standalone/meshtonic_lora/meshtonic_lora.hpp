/*
 * Meshtonic LoRa decoder — PortaPack standalone module app (WIO RF on ESP32PP).
 */

#pragma once

#include "standalone_app.hpp"
#include "ui/ui_widget.hpp"
#include "ui/ui_painter.hpp"
#include "ui/ui_font_fixed_5x8.hpp"

void initialize(const standalone_application_api_t& api);
void on_event(const uint32_t& events);
void shutdown();
void OnFocus();
bool OnKeyEvent(uint8_t key_val);
bool OnEncoder(int32_t delta);
bool OnTouchEvent(int x, int y, uint32_t type);
bool OnKeyboad(uint8_t key);
void PaintViewMirror();

extern const standalone_application_api_t* _api;
extern uint16_t screen_height;
extern uint16_t screen_width;
