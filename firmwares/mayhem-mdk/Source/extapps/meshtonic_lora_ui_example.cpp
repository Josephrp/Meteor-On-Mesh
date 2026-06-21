// Example / reference for the rich "Meshtonic LoRa" UI that runs on the HackRF/PortaPack screen.
// This code is NOT compiled here — it is an illustration of the SatTrack-style module app
// that you would build inside the portapack-mayhem/mayhem-firmware tree (as a standalone app
// or module app) and then turn into the .h binary we embed/serve from the ESP.
//
// Progress: ESP32PP side (commands 0xa020-0xa024, structs, registration of placeholder with
// valid standalone_app_info, data providers, WIO-only RF, apply/start from I2C) is complete.
// This file shows the shape the PP-side binary should implement.
//
// Key points:
// - The image MUST start with a standalone_app_info (placeholder in meshtonic_lora.h today).
// - While running, use the module I2C / custom command mechanism (add_custom_command on ESP)
//   to talk to the ESP32PP (STATUS, PACKETS, PRESETS, APPLY, CONTROL).
// - All RF is performed by the 4x WIO shields on the Meshtonic board. Never arm HackRF RX
//   for LoRa in this app. The ESP will stay in WIO (or hybrid only on explicit IQ feed).

#if 0  // not built in this repo

#include "standalone_application_api.hpp"   // provided by the PP firmware env
#include "pp_structures.hpp"                // or the equivalent in the PP tree
#include "portapack.hpp"
#include "ui_widget.hpp"
#include "ui_painter.hpp"
#include "i2cdev_ppmod.hpp"                 // or the live module handle

// Command IDs (must match the ESP side)
#define PPCMD_LORADEC_STATUS   0xa020
#define PPCMD_LORADEC_PACKETS  0xa021
#define PPCMD_LORADEC_PRESETS  0xa022
#define PPCMD_LORADEC_APPLY    0xa023
#define PPCMD_LORADEC_CONTROL  0xa024

struct LoraRichStatus { /* match lora_rich_status_t layout */ uint8_t running, backend, radio_count, mask; char active_preset[32]; uint32_t total; uint8_t num_recent; };
struct LoraCompactPkt { /* match lora_packet_compact_t */ ... };

class MeshtonicLoRaView : public View {
public:
    MeshtonicLoRaView(NavigationView& nav);
    void focus() override;
    void paint(Painter& painter) override;
    bool on_encoder(const EncoderEvent event) override;
    bool on_key(const KeyEvent key) override;

private:
    void fetch_status();
    void fetch_packets();
    void apply_selected_preset();

    // ring of recent packets shown on screen
    // std::vector<...> packets;

    // widgets: labels for status, list for packets, menu for presets, buttons
};

MeshtonicLoRaView::MeshtonicLoRaView(NavigationView& nav) {
    // build UI: title "Meshtonic LoRa (WIO)", status line, packet list, preset selector
}

void MeshtonicLoRaView::fetch_status() {
    // Use the module command path (similar to SatTrack)
    // e.g. module->send_command(PPCMD_LORADEC_STATUS, ... recv into struct
}

void MeshtonicLoRaView::fetch_packets() {
    // module->send_command(PPCMD_LORADEC_PACKETS, ... fill list
}

void MeshtonicLoRaView::apply_selected_preset() {
    // send PPCMD_LORADEC_APPLY with the chosen preset id bytes
}

// In the app's main registration (like SatTrack in main.cpp of Mayhem or standalone entry):
// PPHandler::add_app( the_built_image_starting_with_standalone_app_info , size );
// The custom commands are handled on the ESP side.

#endif

// When you have a real binary image from building this kind of code in the Mayhem repo:
//   xxd -i meshtonic_lora.bin > meshtonic_lora.h
// Then update/replace the placeholder in this repo and rebuild the ESP firmware.
// The app will then appear in the module menu and run fully on the HackRF screen.