#ifndef PP_STRUCTURES_HPP
#define PP_STRUCTURES_HPP

#include <cstdint>
#include <vector>

#define PP_API_VERSION 1
#define ESP_SLAVE_ADDR 0x51

enum class SupportedFeatures : uint64_t {
    FEAT_NONE = 0,
    FEAT_EXT_APP = 1 << 0,      // provides ext app
    FEAT_UART = 1 << 1,         // can handle uart commands
    FEAT_GPS = 1 << 2,          // provides gps info
    FEAT_ORIENTATION = 1 << 3,  // provides orientation info
    FEAT_ENVIRONMENT = 1 << 4,  // provides environment info (temp || hum || pressure)
    FEAT_LIGHT = 1 << 5,        // provides light info (lux)
    FEAT_DISPLAY = 1 << 6,      // has display to be used by pp
    FEAT_SHELL = 1 << 7,        // can handle shell commands (polling)
    FEAT_MESHTONIC = 1 << 8,    // Meshtonic H4M board: TCA sensors + MCP radios + extra telemetry
};

enum class Command : uint16_t {
    COMMAND_NONE = 0,

    // will respond with device_info
    COMMAND_INFO = 1,

    // will respond with info of application
    COMMAND_APP_INFO,

    // will respond with application data
    COMMAND_APP_TRANSFER,
    // Sensor specific commands
    COMMAND_GETFEATURE_MASK,
    COMMAND_GETFEAT_DATA_GPS,
    COMMAND_GETFEAT_DATA_ORIENTATION,
    COMMAND_GETFEAT_DATA_ENVIRONMENT,
    COMMAND_GETFEAT_DATA_LIGHT,
    COMMAND_GETFEAT_DATA_MESHTONIC,  // Meshtonic H4M extended board info (radios, sensors, profile)

    // Radio slot control (Meshtonic)
    COMMAND_RADIO_GET_STATUS,   // returns radio_status_t for active or requested slot
    COMMAND_RADIO_SELECT,       // payload: uint8 slot; responds with status

    // Shell specific communication
    COMMAND_SHELL_PPTOMOD_DATA,       // pp shell to esp. size not defined
    COMMAND_SHELL_MODTOPP_DATA_SIZE,  // how many bytes the esp has to send to pp's shell
    COMMAND_SHELL_MODTOPP_DATA,       // the actual bytes sent by esp. 1st byte's 1st bit is the "hasmore" flag, the remaining 7 bits are the size of the data. exactly 64 byte follows.
    COMMAND_POWER_OFF,                // requests power off from the esp, and after it needs a full power cycle to get it back again
};

// data structures

typedef struct
{
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    uint8_t second;    /*!< Second */
    uint16_t thousand; /*!< Thousand */
} ppgps_time_t;

typedef struct
{
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    uint16_t year; /*!< Year (start from 2000) */
} ppgps_date_t;

typedef struct
{
    float latitude;       /*!< Latitude (degrees) */
    float longitude;      /*!< Longitude (degrees) */
    float altitude;       /*!< Altitude (meters) */
    uint8_t sats_in_use;  /*!< Number of satellites in use */
    uint8_t sats_in_view; /*!< Number of satellites in view */
    float speed;          /*!< Ground speed, unit: m/s */
    ppgps_date_t date;    /*!< Fix date */
    ppgps_time_t tim;     /*!< time in UTC */
} ppgpssmall_t;

typedef struct
{
    float angle;
    float tilt;
} orientation_t;

typedef struct
{
    float temperature;
    float humidity;
    float pressure;
} environment_t;

typedef struct
{
    float azimuth;
    float elevation;
    uint8_t day;    // data time for setting when the data last updated, and to let user check if it is ok or not
    uint8_t month;  // lat lon won't sent with this, since it is queried by driver
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t sat_day;  // sat last data
    uint8_t sat_month;
    uint16_t sat_year;
    uint8_t sat_hour;
    float lat;
    float lon;
    uint8_t time_method;
} sattrackdata_t;

// Extended Meshtonic board telemetry (sent over I2C to PortaPack)
typedef struct
{
    uint8_t profile;      // 10 = MESHTONIC_H4M
    uint8_t radio_count;  // 0-4
    uint32_t sensor_mask; // bitmask of discovered/enabled sensors
    uint8_t uart_mode;    // 0=gps, 1=ld2450
    uint16_t extra;       // reserved
} meshtonic_board_t;

typedef struct
{
    uint8_t slot;      // which slot (0..3)
    uint8_t present;   // 1/0
    uint8_t busy;
    uint8_t dio1;
    uint8_t rst;
    uint8_t active;    // is the currently selected slot
    float freq_mhz;    // last or configured
    uint8_t sf;
    uint16_t bw_hz;
    int8_t rssi;
    int8_t snr;
    uint8_t last_len;  // preview len
    uint8_t preview[8]; // first bytes of last packet
} radio_status_t;

// === Meshtonic LoRa rich "Apps over I2C" structures (SatTrack-style custom commands) ===
// Compact fixed-size for efficient I2C transfer of live decoded packets.
#pragma pack(push, 1)
typedef struct {
    uint32_t ts_ms;
    float    freq_mhz;
    uint32_t bw_hz;
    uint8_t  sf;
    int16_t  rssi;
    int8_t   snr;
    uint8_t  proto;
    uint8_t  slot;
    uint8_t  decrypted;           // 0/1
    uint8_t  confidence[8];       // e.g. "high\0" or "candidate"
    char     region[8];
    char     preset_id[20];
    uint8_t  payload_preview[16]; // first raw bytes (or hex nibbles if preferred)
    uint8_t  payload_preview_len;
    char     info[32];            // short decoded text / port / type
} lora_packet_compact_t;

// Preset entry (for preset list command)
typedef struct {
    char     id[32];
    char     region[8];
    char     profile[16];
    float    freq_mhz;
    uint8_t  sf;
    uint32_t bw_hz;
} lora_preset_entry_t;

// Rich status blob for the native PP screen app
typedef struct {
    uint8_t  running;
    uint8_t  backend;             // see LoraBackend in ep_app_loradecoder.hpp
    uint8_t  radio_count;
    uint8_t  slot_present_mask;
    char     active_preset[32];
    uint32_t total_packets;       // since start or last clear (mod 2^32 is fine)
    uint8_t  num_recent;          // how many compact packets follow in the response (or separate PACKETS cmd)
    uint8_t  pp_connected;        // 1 when a PortaPack is attached over I2C (rich UI likely active)
} lora_rich_status_t;
#pragma pack(pop)

typedef struct
{
    uint32_t api_version;
    uint32_t module_version;
    char module_name[20];
    uint32_t application_count;
} device_info;

enum app_location_t : uint32_t {
    UTILITIES = 0,
    RX,
    TX,
    DEBUG,
    HOME
};

typedef struct
{
    uint32_t header_version;
    uint8_t app_name[16];
    uint8_t bitmap_data[32];
    uint32_t icon_color;
    app_location_t menu_location;
    uint32_t binary_size;
} standalone_app_info;

typedef struct
{
    uint8_t* binary;
    uint32_t size;
} app_list_element_t;

typedef struct
{
    std::vector<uint8_t>* data;
} pp_command_data_t;

typedef void (*pp_i2c_command)(pp_command_data_t data);

typedef struct
{
    uint16_t command;
    pp_i2c_command got_command;
    pp_i2c_command send_command;
} pp_custom_command_list_element_t;

// callback typedefs

typedef void (*get_features_CB)(uint64_t& feat);
typedef void (*get_gps_data_CB)(ppgpssmall_t& gpsdata);
typedef void (*get_orientation_data_CB)(orientation_t& gpsdata);
typedef void (*get_environment_data_CB)(environment_t& envdata);
typedef void (*get_light_data_CB)(uint16_t& light);
typedef uint16_t (*get_shell_data_size_CB)();                                   // this wil be called when PP request MOD to send how many bytes it has in the outgoing (to shell) tx buffer. IRQ
typedef void (*got_shell_data_CB)(std::vector<uint8_t>& data);                  // this wil be called when got shell data from pp. IRQ
typedef void (*send_shell_data_CB)(std::vector<uint8_t>& data, bool& hasmore);  // this will be called when the module needs to send serial data to pp. Just pass the data, and set the hasmore. NO 0th byte set needed. IRQ
typedef void (*get_shutdown_command_CB)(std::vector<uint8_t>& data);            // this will be called when got shutdown command from pp. IRQ
#endif