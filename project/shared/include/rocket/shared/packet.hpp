#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rocket::protocol {

enum class PacketType : std::uint8_t {
    Command = 1,
    Telemetry = 2,
    CommandAck = 3,
};

enum class CommandType : std::uint8_t {
    EnterStandby = 0,
    EnterLaunchMode = 1,
    EnterDebugMode = 2,
    ZeroRotation = 3,
    TestPyroChannel = 99,
};

enum class FlightState : std::uint8_t {
    Standby = 0,
    LaunchMode = 1,
    DebugMode = 2,
    InFlight = 3,
};

enum class AckResponse : std::uint8_t {
    Accepted = 0,
    Rejected = 1,
    Simulated = 2,
};

enum TelemetryFlag : std::uint8_t {
    OrientationValid = 1 << 0,
    ImuDataValid = 1 << 1,
    AltitudeValid = 1 << 2,
    VerticalSpeedValid = 1 << 3,
};

#pragma pack(push, 1)

struct CommandPacket {
    CommandType command;
    std::uint8_t channel;
};

struct TelemetryPacket {
    std::uint32_t timestamp_ms;
    FlightState state;
    std::uint8_t flags;
    std::int32_t altitude_cm;
    std::int16_t vertical_speed_cms;
    std::int16_t rotation[4];
    std::int16_t gyro_dps_x10[3];
    std::int16_t acceleration_mg[3];
};

struct CommandAckPacket {
    CommandType command;
    AckResponse response;
    FlightState state;
    std::uint8_t channel;
};

union RadioPayload {
    CommandPacket command;
    TelemetryPacket telemetry;
    CommandAckPacket acknowledgement;
};

struct RadioFrame {
    PacketType type;
    std::uint16_t sequence;
    RadioPayload payload;
};

#pragma pack(pop)

static_assert(sizeof(CommandPacket) == 2);
static_assert(sizeof(TelemetryPacket) == 32);
static_assert(sizeof(CommandAckPacket) == 4);
static_assert(sizeof(RadioFrame) == 35);

constexpr std::size_t radio_frame_size(PacketType type) {
    constexpr std::size_t header_size = sizeof(PacketType) + sizeof(std::uint16_t);
    switch (type) {
    case PacketType::Command:
        return header_size + sizeof(CommandPacket);
    case PacketType::Telemetry:
        return header_size + sizeof(TelemetryPacket);
    case PacketType::CommandAck:
        return header_size + sizeof(CommandAckPacket);
    }
    return 0;
}

inline std::size_t encode_radio_frame(const RadioFrame &frame, std::uint8_t *bytes,
                                      std::size_t capacity) {
    constexpr std::size_t header_size = sizeof(PacketType) + sizeof(std::uint16_t);
    const std::size_t length = radio_frame_size(frame.type);
    if (bytes == nullptr || length == 0 || capacity < length) {
        return 0;
    }

    bytes[0] = static_cast<std::uint8_t>(frame.type);
    std::memcpy(bytes + sizeof(PacketType), &frame.sequence, sizeof(frame.sequence));
    void *payload = bytes + header_size;
    switch (frame.type) {
    case PacketType::Command:
        std::memcpy(payload, &frame.payload.command, sizeof(CommandPacket));
        break;
    case PacketType::Telemetry:
        std::memcpy(payload, &frame.payload.telemetry, sizeof(TelemetryPacket));
        break;
    case PacketType::CommandAck:
        std::memcpy(payload, &frame.payload.acknowledgement, sizeof(CommandAckPacket));
        break;
    }
    return length;
}

inline bool decode_radio_frame(const std::uint8_t *bytes, std::size_t length, RadioFrame &frame) {
    constexpr std::size_t header_size = sizeof(PacketType) + sizeof(std::uint16_t);
    if (bytes == nullptr || length < header_size) {
        return false;
    }

    const auto type = static_cast<PacketType>(bytes[0]);
    if (length != radio_frame_size(type)) {
        return false;
    }

    frame.type = type;
    std::memcpy(&frame.sequence, bytes + sizeof(PacketType), sizeof(frame.sequence));
    const auto *payload = bytes + header_size;

    switch (type) {
    case PacketType::Command:
        frame.payload.command = {};
        std::memcpy(&frame.payload.command, payload, sizeof(CommandPacket));
        return true;
    case PacketType::Telemetry:
        frame.payload.telemetry = {};
        std::memcpy(&frame.payload.telemetry, payload, sizeof(TelemetryPacket));
        return true;
    case PacketType::CommandAck:
        frame.payload.acknowledgement = {};
        std::memcpy(&frame.payload.acknowledgement, payload, sizeof(CommandAckPacket));
        return true;
    }

    return false;
}

} // namespace rocket::protocol
