#pragma once
#include <cstdint>

namespace shared::proto {

enum class PacketType : uint8_t {
    // Bidirectional
    Heartbeat = 1,

    // Uplink
    Command = 10,

    // Downlink
    Sync      = 20,
    Telemetry = 21,
};

enum class CommandType : uint8_t {
    EnterStandby      = 0,
    EnterLaunchDetect = 1,
    EnterTesting      = 2,

    // Testing commands
    ActivatePyroChannel = 99,
};

enum class FlightState : uint8_t {
    Standby      = 0,
    LaunchDetect = 1,
    Testing      = 2,
    InFlight     = 3,
};

enum class AckResponse : uint8_t {
    Ok  = 0,
    Bad = 1,
};

#pragma pack(push, 1)

// Bidirectional

struct HeartbeatPayload {
    FlightState flight_state;
    int8_t      rx_rssi;
    int8_t      rx_snr;
};

// Uplink

struct CommandPayload {
    CommandType command;
    uint16_t    key;
};

// Downlink

struct SyncPayload {
    FlightState state;
};

struct TelemetryPayload {
    uint32_t timestamp_ms;
    int32_t  altitude_cm;     // Altitude in cm
    int16_t  velocity_cms[3]; // Velocity vector in cm/s
    int16_t  rotation[4];     // Scaled by 32767
};

#pragma pack(pop)

} // namespace shared::proto