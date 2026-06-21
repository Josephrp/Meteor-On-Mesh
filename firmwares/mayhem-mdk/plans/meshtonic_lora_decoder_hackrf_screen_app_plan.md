# Plan: Meshtonic LoRa Decoder — Full HackRF/PortaPack Screen App (using WIO LoRa shields, no onboard RF)

**Date**: 2026-06-21  
**Context**: Meshtonic H4M (4× WIO SX1262) + ESP32PP (Mayhem MDK) addon for PortaPack/HackRF.  
**Goal**: A rich, native-feeling application that renders on the HackRF color screen (PortaPack UI), with all LoRa RF capture and primary decode performed by the Meshtonic WIO shields (controlled by the ESP32). Do **not** use the HackRF's own RF/baseband for this LoRa monitoring function.

This document is the result of:
- Cloning and investigating https://github.com/portapack-mayhem/mayhem-firmware (the "HackRF firmware").
- Studying ESP32PP integration code in this repo (`ppi2c/`, `apps/ep_app_loradecoder.*`, `AppManager`, `pp_*` structures/commands, extapps, etc.).
- Analyzing design patterns for "apps" that appear on the HackRF screen and how ESP32PP modules interact with them.

---

## 1. Investigation Summary — Design Patterns (SatTrack + Apps over I2C)

### How "Apps over I2C" Work (the SatTrack pattern — the proven seamless model)

This is the established way SatTrack (and WiFi settings, IR, ESP manager, etc.) deliver a full native UI on the HackRF/PortaPack color screen while the heavy work (and in our case all RF) lives on the ESP32PP module.

**Discovery & Menu Integration (PP side)**
- PP firmware detects the module via its I2C driver (`I2cDev_PPmod`, model `I2CDECMDL_PPMOD`).
- It calls `readDeviceInfo()` → gets `device_info` (api_version, module_version, module_name[20], application_count).
- For each app index it calls `getStandaloneAppInfo(i)` → gets `standalone_app_info`:
  - `header_version`
  - `app_name[16]`
  - `bitmap_data[32]` (icon)
  - `icon_color`
  - `menu_location` (RX, UTILITIES, etc.)
  - `binary_size`
- These appear automatically in the PP menus under the right category (see `ExternalItemsMenuLoader` and `ExternalModuleView`).

**App Binary Serving & Launch**
- The ESP32PP registers apps with `PPHandler::add_app(uint8_t* binary, uint32_t size)`.
  - The binary **must** start with a `standalone_app_info` struct (size must be multiple of 32 and >= sizeof(standalone_app_info)).
- When the user selects the app icon on the PP:
  - PP downloads the entire binary in 128-byte chunks using the standard module app transfer protocol (`COMMAND_APP_INFO` / `COMMAND_APP_TRANSFER` on the I2C slave).
  - ESP side implements this in `pp_handler.cpp` (`on_send_ISR` for app info/transfer + `on_command_ISR`).
  - PP then calls `run_module_app(nav, image, size)` which loads it into memory and runs it as a `StandaloneView` (or equivalent) on the HackRF screen.
- The launched app runs natively on the PP using the UI framework (widgets, Painter, encoder, buttons, etc.).

**Live Data & Control (while the app is on screen)**
- Structured bidirectional communication uses **custom commands** (high command IDs like 0xa0xx), registered on the ESP with:
  ```cpp
  PPHandler::add_custom_command(PPCMD_XXX, got_command_callback, send_command_callback);
  ```
  - `send_command` (ESP → PP): PP requests data, ESP fills the response buffer (e.g. SatTrack sends the whole `sattrackdata_t` struct).
  - `got_command` (PP → ESP): PP sends control payloads (e.g. set satellite name, set manual GPS).
- Examples from SatTrack:
  - `PPCMD_SATTRACK_DATA` (0xa000) — send-only response with current data.
  - `PPCMD_SATTRACK_SETSAT` (0xa001), `SETMGPS` (0xa002) — receive control.
- The PP-side app code (inside the downloaded binary) knows the command IDs and the struct layouts and polls or reacts accordingly.
- The actual computation (SGP4 for SatTrack, or WIO control + multi-protocol decode for LoRa) stays on the ESP/main loop or EPApp.

**Key structs (from this repo + Mayhem)**
- `device_info`, `standalone_app_info` (see `ppi2c/pp_structures.hpp`).
- Custom data structs are defined on both sides (e.g. `sattrackdata_t`).

**Current LoRa state vs. the target**
- Today LoRa uses the on-ESP "EPApp" system (LORADECODER app id, `PPCMD_LORADEC_*` commands, simple `GETUI` text + polled packets/status).
- There is **no** `standalone_app_info` binary registered for a rich native PP screen UI.
- Goal: make it appear and behave exactly like SatTrack — icon in the module menu, one-tap launch of a full color app on the HackRF screen, seamless live packet feed and controls over I2C, while **all** RF (the 4 WIO shields) and decode logic remain on the Meshtonic/ESP side.

### PortaPack / Mayhem Firmware Side — Other Patterns (for context)

- Modern full standalone apps can also be built in `firmware/standalone/...` (digitalrain, pacman) and loaded from SD or served by a module. They receive the rich `standalone_application_api_t` vtable.
- Older external-app transfer and I2C device drivers exist but are secondary for rich UI apps.
- The module-provided apps (the "apps over I2C" pattern) are the direct match for what we need.

### ESP32PP / This Project Side (the "module")

(See original investigation points — they remain valid. The key addition is that SatTrack-style apps are the ones registered via `add_app` + custom commands, while EPApps provide the backing logic.)

### ESP32PP / This Project Side (the "module")

- **I2C Slave** (typically addr 0x51 on I2C1): `ppi2c/pp_handler.cpp`, `pp_structures.hpp`.  
  Responds to standard `Command` codes (INFO, APP_*, GETFEATURE_*, SHELL, POWER, RADIO_*) + custom `PPCMD_*` (APPMGR, LORADEC_*).
- **Feature Mask + Data**: Advertises `FEAT_EXT_APP`, GPS, orientation, environment, light, **MESHTONIC** board info (profile=10, radio_count, sensor_mask, ...), and radio slot status.
- **App Manager (on-ESP "EPApps")**: `apps/appmanager.*`, `ep_app.*`.  
  PP sends start (appid), ESP instantiates `EPAppLoraDecoder` etc., runs `Loop`, answers `OnPPReqData` / `OnPPData`.  
  Current LORADECODER (appid 04) already implements:
  - `PPCMD_LORADEC_GETSTATUS`, `GETPACKETS`, `SETCONFIG`, `CONTROL` (start/stop/arm, backend switch, preset apply).
  - `PPCMD_LORADEC_FEEDIQ` (for hypothetical bursts from PP).
  - `PPCMD_LORADEC_GETUI` (simple 6×20 char text lines for basic PP LCD status).
- **WIO / LoRa logic** is **fully on ESP**:
  - `ep_app_loradecoder.*` (4× SX1262 via MCP/TCA on H4M, CAD/continuous, preset application from `lora_bands` generated from `presets.toml`).
  - `lora_decode.*` (multi-protocol: Meshtastic full, structural for MeshCore/LoRaWAN/etc.).
  - `lora_dsp.*` (for IQ bursts if ever fed).
  - NVS persist of per-slot region/profile/preset/rx_mode.
  - Status aggregation (`bands_in_use[]`), packet tagging with slot/region/proto/confidence/decrypted.
- **Presets**: Single source `lora-wideband-decoder/presets.toml` → generated `lora_bands.h/cpp`.
- **No onboard RF dependency for LoRa** in the current LoraDecoder (backend 1 = WIO, 2/3 = hybrid with optional HackRF IQ, but user requirement is to avoid onboard RF).

**Current "LoRa on screen" state**:
- Basic text status via `GETUI` (for simple PP views).
- Polled packets/status via LORADEC commands.
- ESP OLED shows a summary.
- No rich color UI / packet table / controls on the HackRF screen yet.

---

## 2. Goals & Constraints for the New App (Seamless "App over I2C" like SatTrack)

- The app must appear in the PP module menu (auto-discovered via I2cDev_PPmod) with icon/name, just like SatTrack.
- One-tap launch downloads the PP UI binary over I2C and runs a full rich native UI on the HackRF screen (packet list/table, per-slot WIO status, preset selector, live decodes with proto/RSSI/confidence, controls).
- **All RF and decode on Meshtonic WIO shields only** (ESP controls the 4× SX1262; never use HackRF RX/baseband for LoRa in this app).
- Use the exact proven "apps over I2C" mechanism:
  - `PPHandler::add_app( binary_prefixed_with_standalone_app_info , size )`
  - Custom commands (`add_custom_command`) for bidirectional structured data (packets, status, presets, controls).
- Keep and extend the existing EPApp LoraDecoder logic on the ESP (WIO arming, decode, NVS, aggregation).
- Provide graceful fallback (the current GETUI + LORADEC commands still work for simple views).
- Compatible with H4M pinout and the generated presets.
- Performance: modest packet rates → efficient polling + custom command responses is sufficient.

---

## 3. Recommended Architecture (SatTrack-style "App over I2C")

```
HackRF / PortaPack color screen + controls
          │
          │ (PP firmware discovers module via I2cDev_PPmod)
          │   readDeviceInfo() → application_count, module name
          │   getStandaloneAppInfo(i) → name, icon, menu_location, binary_size
          │
          │ User taps the "Meshtonic LoRa" / "WIO LoRa" icon (appears in RX or Utilities)
          │   PP downloads the app binary 128B chunks via standard APP_INFO/APP_TRANSFER
          │   Runs it via run_module_app() → StandaloneView on the HackRF screen
          │
          │ While running: the PP app uses custom commands (0xa0xx range) + i2c for live data
          ▼
ESP32-S3 (ESP32PP) — I2C slave 0x51
          │
          │ PPHandler::add_app( the_binary_starting_with_standalone_app_info , size )
          │ + add_custom_command( LORA_*, got_cb, send_cb )
          │
          │ (ep_app_loradecoder + main SatTrack-style loop)
          │   • All WIO SX1262 control (4 slots via MCP/TCA, CAD, presets from lora_bands)
          │   • Multi-protocol decode (lora_decode)
          │   • Packet tagging (slot/region/profile/proto/confidence/...)
          │   • Custom command handlers: send full packets/status/presets, receive controls
          ▼
4× WIO SX1262 (Meshtonic H4M shields)  ←←← ONLY RF used for LoRa in this app
   (no HackRF baseband/RX for demodulation)
```

Exact SatTrack-style flow we will replicate:
1. ESP builds or embeds a PP-side UI binary (starts with `standalone_app_info`).
2. Registers it: `PPHandler::add_app(binary, size);`
3. PP menu auto-lists it because of the module protocol.
4. On launch PP downloads + executes the binary on the HackRF screen.
5. The running UI talks back to ESP using custom commands for:
   - Live packet feed (array of tagged packets + bands_in_use).
   - Preset list + apply.
   - Start/stop, slot details, key status, etc.
6. All actual radio work and decode stays on the ESP + WIO shields.

---

## 4. Complete Implementation Plan

### Phase 0 — Prep & Investigation Artifacts (done)
- [x] Clone https://github.com/portapack-mayhem/mayhem-firmware (shallow).
- [x] Study `firmware/standalone/...` examples + `firmware/common/standalone_app.hpp`.
- [x] Study I2C module protocol in ESP32PP (`ppi2c/`, `pp_commands.hpp`, `AppManager`, LORADEC commands, MESHTONIC feature).
- [x] Confirm WIO-only path (no HackRF RX) is already supported on ESP.

### Phase 1 — Protocol / Data Model Extensions (minimal, backward compatible)
**Repo**: this one (`firmwares/mayhem-mdk`).

1. Extend `LoraPacket` / status if needed for richer UI (already has most: slot, region, profile, preset_id, proto, confidence, decrypted, key_label, rssi, snr, info, payload_hex).
2. Enhance `PPCMD_LORADEC_GETPACKETS` response (or add `GETPACKETS2`) to return more structured data or more packets per call.
3. Add (optional) push mechanism or larger status blob (e.g., via a new sub-command or by making GETSTATUS return a longer structured record including recent packets).
4. Ensure `GETUI` remains for basic compatibility.
5. Add commands if useful:
   - Query full preset list (ESP already can generate JSON; expose over I2C).
   - Per-packet "detail" request (for deep view of a selected packet).
6. On ESP side, make LORADECODER EPApp expose a clean "data provider" interface that the I2C handlers can call (already close).

**Deliverable**: Updated `ep_app_loradecoder.hpp/cpp`, possibly small additions to `lora_bridge_json` or a dedicated binary serializer for I2C efficiency.

### Phase 2 — Meshtonic / WIO Side Hardening (if gaps)
- Confirm auto-start of LORADECODER on H4M profile (already added in recent work).
- Ensure hybrid detection (when PP is attached via I2C) still prefers WIO for RF.
- Add any missing "set preset by id from PP command" robustness.
- Expose RSSI history or per-slot recent energy if the WIO driver provides it (for a mini "waterfall" or bar on PP screen).

### Phase 3 — Produce the PP-Side UI Binary + "Apps over I2C" Integration (SatTrack pattern)

The goal is to make the LoRa decoder appear and launch exactly like SatTrack.

**3.1 Create / obtain the PP UI binary (the thing that runs on the HackRF screen)**

Two practical routes (both end up as a blob the ESP can serve):

A. Build a minimal "module standalone" app (recommended for rich UI)
   - Use the same build style as the firmware's `standalone/` examples or the external app system.
   - The resulting image must start with a `standalone_app_info` header (see pp_structures.hpp):
     ```c
     typedef struct {
         uint32_t header_version;
         uint8_t  app_name[16];
         uint8_t  bitmap_data[32];
         uint32_t icon_color;
         app_location_t menu_location;   // e.g. RX
         uint32_t binary_size;
     } standalone_app_info;
     ```
   - Implement the UI (packet list, slot bars, preset picker, detail view) using the UI widgets + Painter that the StandaloneView environment provides.
   - For communication, the app will talk to the ESP module (I2C + custom commands). You can use direct I2C or the patterns already used by other module apps.

B. Simpler starting point (text + list first)
   - Port/adapt a small existing module app or start from the SatTrack binary structure, replace the SatTrack-specific drawing/logic with LoRa views.
   - Later evolve it to the full standalone API if desired.

The output of either route is a `.bin` (or C array) that the ESP will embed/serve.

**3.2 Register the app on the ESP (this repo)**

In `main.cpp` (around the other `add_app` calls):

```cpp
extern const uint8_t meshtonic_lora_app[];   // or #include "meshtonic_lora_app.h"
extern const uint32_t meshtonic_lora_app_size;

PPHandler::add_app((uint8_t*)meshtonic_lora_app, meshtonic_lora_app_size);
```

Also register the custom commands the PP UI will use (see below).

**3.3 Define & implement custom commands (bidirectional, SatTrack style)**

In `pp_commands.hpp` add:

```cpp
// Meshtonic / WIO LoRa (apps over I2C)
#define PPCMD_LORADEC_STATUS      0xa020   // PP requests → ESP sends current bands + preset + backend
#define PPCMD_LORADEC_PACKETS     0xa021   // PP requests N recent packets (full tagged records)
#define PPCMD_LORADEC_PRESETS     0xa022   // list of available presets
#define PPCMD_LORADEC_APPLY       0xa023   // PP sends preset id or slot config
#define PPCMD_LORADEC_CONTROL     0xa024   // start/stop, etc.
```

In `main.cpp` (or a dedicated lora_i2c_bridge.cpp):

```cpp
PPHandler::add_custom_command(PPCMD_LORADEC_STATUS,  nullptr, send_lora_status);
PPHandler::add_custom_command(PPCMD_LORADEC_PACKETS, nullptr, send_recent_packets);
PPHandler::add_custom_command(PPCMD_LORADEC_APPLY,   recv_apply_preset, nullptr);
// etc.
```

- `send_*` callbacks fill `*data.data` with the struct(s) the PP app expects.
- `recv_*` callbacks parse the incoming payload and call into `EPAppLoraDecoder` or the existing preset/arm functions.

Reuse/extend the existing `LoraDecodedRecord` / `LoraPacket` shapes and the preset registry.

**3.4 The PP-side app code (inside the binary)**

- On launch: detect the module, read device/app info if needed.
- Draw the rich UI (slots, live packet feed, controls).
- Periodically (or on events) issue the custom commands above to fetch fresh data.
- On user action (apply preset, start/stop) send the corresponding control command.
- Keep a local ring of recent packets for smooth scrolling.

This is the part that will live in the Mayhem firmware tree (or as a separately built blob that we then turn into a .h for embedding, exactly like `extapps/sattrack.h`).

**3.5 Fallback / coexistence**
- Keep the existing `PPCMD_LORADEC_GETSTATUS / GETPACKETS / GETUI / CONTROL` working so simple views and the on-ESP EPApp continue to function.
- The new high-level custom commands (0xa02x) are for the rich SatTrack-style UI only.

### Phase 4 — Integration & Polish
- Make sure when the standalone app is running, the ESP side is in the correct backend (WIO or hybrid as appropriate) and does not arm HackRF RF for LoRa.
- Add "Meshtonic LoRa" documentation in both repos (how to use, wiring reminder, that onboard RF is not used for this).
- Update ESP web UI / OLED to indicate "PP screen active" when the rich app is connected.
- Add a simple "test" or "self-check" that verifies WIOs are present and responding (already partially in status).
- Handle hot-plug of the module / PP attach (the hybrid upgrade logic on ESP already does some of this).

### Phase 5 — Testing & Validation
- H4M alone (no PP): existing behavior unchanged.
- H4M + PP attached: start the new app from PP menu → see rich UI, packets appear from WIOs only.
- Preset switching from PP screen works and affects the 4 WIOs.
- Multiple protocols surface correctly (Meshtastic text, MeshCore, LoRaWAN devaddr, etc.).
- Performance: no excessive I2C traffic; graceful under bursty traffic.
- Power: CAD mode on WIOs still respected.

---

## 5. Files Touched (high level) — SatTrack Pattern

**This repo (ESP32PP / MDK firmware)**:
- `Source/main/ppi2c/pp_structures.hpp` — ensure `standalone_app_info` and related structs are complete.
- `Source/main/ppi2c/pp_handler.cpp` — already handles APP_INFO/APP_TRANSFER and custom commands; may need tiny robustness tweaks.
- `Source/main/main.cpp` (and/or a new small bridge file) — `PPHandler::add_app( lora_ui_binary, size );` + `add_custom_command(...)` for the LORADEC_* data/control commands. Wire the send/got callbacks to the existing LoraDecoder logic.
- `Source/extapps/meshtonic_lora_app.h` (new, generated from the built PP binary, like `sattrack.h`).
- `Source/main/apps/ep_app_loradecoder.*` — minor enhancements for richer packet/status export if needed.
- `pp_commands.hpp` — add the new 0xa02x custom command IDs.
- `docs/PORTAPACK_LORADEC.md` — document the new seamless app flow and the custom commands.
- Update the plan.

**PortaPack Mayhem firmware** (https://github.com/portapack-mayhem/mayhem-firmware):
- Produce the UI binary that will be turned into the `.h` above.
  - Either add `firmware/standalone/meshtonic_lora/` (full modern standalone app) and build a loadable image, **or**
  - Create a minimal module app that follows the same download + run path used by SatTrack.
- The binary must start with a valid `standalone_app_info` so `getStandaloneAppInfo()` and the menu loader recognize it.
- The app code will issue the custom LORA commands and render the rich view (packet list, slots, presets, etc.).
- No changes needed in the core I2C device driver (`I2cDev_PPmod`) — it already supports module apps.

**Shared / Generated**:
- The LoRa UI binary (once built) becomes the single artifact that both the ESP (embedded) and the build process consume.
- Keep `presets.toml` as the single source of truth for bands (already used on ESP).

---

## 6. Risks & Mitigations

- **I2C API limitations in standalone**: The current `i2c_read` may be read-only or limited. Mitigation: extend the api (add `i2c_write` or a higher-level `module_command`) in a backward-compatible way (append fields, bump version), or implement the needed transactions via existing PP I2C master paths inside the app if the module is modeled as an I2cDev.
- **Binary size / RAM on PP side**: Standalone apps have 64k RAM region by default in the ld. Keep state reasonable (ring of ~64–256 recent packets is plenty).
- **Protocol drift**: Pin the structures used over I2C (version them if they grow).
- **Upstream acceptance**: The app can initially live in a vendor/ or meshtonic/ subdir and be proposed upstream later.
- **"No onboard RF" enforcement**: The app simply never calls into baseband RX for LoRa bands; document it clearly. The ESP side can ignore or reject FEEDIQ for LoRa if in pure WIO mode.

---

## 7. Suggested First Steps (actionable, SatTrack pattern)

1. Define the exact structs for the custom LORA commands (status, packet record, preset entry) and add the command IDs in `pp_commands.hpp`.
2. On the ESP side: implement the send/receive callbacks + call `PPHandler::add_app(...)` with a placeholder or minimal binary (even a tiny one that just shows "LoRa" so we can validate discovery + launch).
3. Build (or stub) the PP-side UI binary that:
   - Declares a correct `standalone_app_info` at the front.
   - On launch draws a basic list or status.
   - Issues the custom commands to fetch live data from the ESP.
4. Wire the live packet/status path end-to-end (ESP WIO packets → custom command response → PP screen list).
5. Implement preset apply + start/stop from the PP UI.
6. Add a proper icon bitmap, place it under RX, polish, document.
7. Replace the placeholder binary with the real rich one (can evolve from a simple module app to a full standalone one).

Reference implementation to copy style from: SatTrack (binary in `extapps/sattrack.h`, custom commands in main.cpp, data struct in pp_structures.hpp, PP menu integration via I2cDev_PPmod).

---

## Appendix: Key References (from SatTrack + Apps-over-I2C investigation)

**This repo**
- `Source/extapps/sattrack.h` + usage in `main.cpp` (the canonical example of a module-provided app binary + custom commands).
- `Source/main/ppi2c/pp_structures.hpp` — `device_info`, `standalone_app_info`, `sattrackdata_t`, etc.
- `Source/main/ppi2c/pp_handler.cpp` — `add_app`, `add_custom_command`, APP_INFO/APP_TRANSFER handling, custom command dispatch.
- `Source/main/pp_commands.hpp` — the 0xa0xx custom command range.
- `Source/main/main.cpp` — registration of SatTrack (and others) and the timer loop that feeds data.
- Existing Lora: `ep_app_loradecoder.*`, LORADEC commands, presets.

**Mayhem firmware (the PP side that consumes module apps)**
- `firmware/application/apps/ui_external_module_view.*`
- `firmware/application/ui_external_items_menu_loader.cpp`
- `firmware/common/external_app.hpp` and the standalone app structures.
- `I2cDev_PPmod` (the driver that does readDeviceInfo / getStandaloneAppInfo / downloadStandaloneApp).

This revised plan follows the exact SatTrack "apps over I2C" recipe so the Meshtonic LoRa decoder will appear, launch, and feel native on the HackRF screen while all RF work stays on the WIO shields.

## Implementation Status (updated 2026-06-21)

**ESP32PP side (this repo)** — Phases 1–4 protocol, data model, integration, and polish **complete** (builds cleanly):
- Phase 1: Compact I2C structs (`lora_packet_compact_t`, `lora_preset_entry_t`, `lora_rich_status_t`); new high-level commands 0xa020–0xa024 (STATUS/PACKETS/PRESETS/APPLY/CONTROL); legacy GET* commands + GETUI preserved for compatibility; rich `lora_*` accessors + variable-count PACKETS; clean data provider interface.
- Phase 2: Auto-start on H4M; WIO-preferred (hybrid only on real IQ activity); robust preset apply from I2C with immediate re-arm; dynamic `slot_present_mask` + `numActiveRadios` from actual arm results (self-check); per-packet RSSI/slot already available for PP-side bars.
- Phase 3: `standalone_app_info` header placeholder registered via `PPHandler::add_app` (appears in module menu like SatTrack); custom command callbacks implemented and wired; `convert_bin_to_h.py` helper + `meshtonic_lora_ui_example.cpp` reference for the real Mayhem-side binary; coexistence with basic EPApp path.
- Phase 4: Backend locked to WIO for this app (no HackRF LoRa RX); docs updated (PORTAPACK_LORADEC.md, plan, README); OLED shows "PP screen active"; web status JSON includes `pp_connected`; real slot mask exposed in rich + legacy status (self-check); hot-plug/attach handled by existing conn-state + logic.

**PP-side rich UI binary**:
- Placeholder (with correct header version, icon, size) registered for discovery/transfer.
- Real rich UI (packet list/table, slot energy bars, preset picker, live polling of 0xa02x cmds) is implemented in the Mayhem firmware tree following the SatTrack pattern, then converted (use the helper) and dropped in.
- See `extapps/meshtonic_lora_ui_example.cpp` and `convert_bin_to_h.py`.

Remaining (hardware / other repo):
- Build and integrate the real rich binary from the Mayhem repo.
- End-to-end on H4M+PP hardware (launch from menu, live WIO packets only, presets, multiple protocols, CAD power).

All checklist items for Phases 1–4 in this repo are addressed.

---

The plan below is retained for reference.

Ready to implement when you are. (phases below are the original checklist)