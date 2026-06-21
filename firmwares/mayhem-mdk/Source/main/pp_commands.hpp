#pragma once

// sattrack
#define PPCMD_SATTRACK_DATA 0xa000
#define PPCMD_SATTRACK_SETSAT 0xa001
#define PPCMD_SATTRACK_SETMGPS 0xa002
// ir
#define PPCMD_IRTX_SENDIR 0xa003
#define PPCMD_IRTX_GETLASTRCVIR 0xa004
// Wifi settings app
#define PPCMD_WIFI_SET_STA 0xa005
#define PPCMD_WIFI_SET_AP 0xa006
#define PPCMD_WIFI_GET_CONFIG 0xa007
#define PPCMD_WIFI_STARTSCAN 0xa008
#define PPCMD_WIFI_STOPSCAN 0xa009
#define PPCMD_WIFI_GETSCANRESULT 0xa00a
// esp manager
#define PPCMD_AIRPLANE_MODE 0xa00b
// appmgr - apps on esp
#define PPCMD_APPMGR_APPMGR 0xa00c
#define PPCMD_APPMGR_APPCMD 0xa00d
// lora wideband decoder (vendored LWD) app on esp
#define PPCMD_LORADEC_GETSTATUS   0xa00e
#define PPCMD_LORADEC_GETPACKETS  0xa00f
#define PPCMD_LORADEC_SETCONFIG   0xa010
#define PPCMD_LORADEC_CONTROL     0xa011   // start/stop, arm local radios, etc. (payload byte 0 = cmd)
#define PPCMD_LORADEC_FEEDIQ      0xa012   // HackRF IQ burst chunks from PortaPack (see docs/PORTAPACK_LORADEC.md)
#define PPCMD_LORADEC_GETUI       0xa013   // Multi-line UI text for PortaPack LCD

// Meshtonic / WIO LoRa — rich native "Apps over I2C" UI on HackRF/PortaPack screen (SatTrack pattern)
// These are high-level custom commands for the full color app (separate from the basic LORADEC EPApp commands).
#define PPCMD_LORADEC_STATUS      0xa020   // PP requests → ESP sends bands_in_use, active preset, backend, radio status summary
#define PPCMD_LORADEC_PACKETS     0xa021   // PP requests N → ESP sends recent full tagged LoraPacket records (slot/region/profile/proto/confidence/etc.)
#define PPCMD_LORADEC_PRESETS     0xa022   // PP requests → ESP sends list of available presets (id, region, profile, freq/sf/bw)
#define PPCMD_LORADEC_APPLY       0xa023   // PP sends preset_id (or slot overrides) to apply on the 4 WIOs
#define PPCMD_LORADEC_CONTROL     0xa024   // start/stop, clear, key sync hints, backend hints, etc. (payload-driven)