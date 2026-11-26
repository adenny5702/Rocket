#pragma once

#include <cstdint>
#include <cstdio>

#include "rocket/shared/packet.hpp"

namespace rocket::ground::usb_protocol {

inline bool is_known_command(unsigned int value) {
    using rocket::protocol::CommandType;

    switch (static_cast<CommandType>(value)) {
    case CommandType::EnterStandby:
    case CommandType::EnterLaunchMode:
    case CommandType::EnterDebugMode:
    case CommandType::ZeroRotation:
    case CommandType::TestPyroChannel:
        return true;
    default:
        return false;
    }
}

inline bool parse_command_line(const char *line, rocket::protocol::RadioFrame &frame) {
    unsigned int sequence = 0;
    unsigned int command = 0;
    unsigned int channel = 0;
    char trailing_character = '\0';

    if (line == nullptr ||
        std::sscanf(line, "CMD %u %u %u %c", &sequence, &command, &channel, &trailing_character) !=
            3 ||
        sequence > UINT16_MAX || command > UINT8_MAX || channel > UINT8_MAX ||
        !is_known_command(command)) {
        return false;
    }

    frame = {};
    frame.type = rocket::protocol::PacketType::Command;
    frame.sequence = static_cast<std::uint16_t>(sequence);
    frame.payload.command = {
        .command = static_cast<rocket::protocol::CommandType>(command),
        .channel = static_cast<std::uint8_t>(channel),
    };
    return true;
}

inline void write_error(const char *message) {
    std::printf("ERR %s\n", message);
}

inline void write_telemetry(const rocket::protocol::RadioFrame &frame) {
    const auto &telemetry = frame.payload.telemetry;
    std::printf(
        "TEL %u %lu %u %u %ld %d "
        "%d %d %d %d %d %d %d %d %d %d\n",
        static_cast<unsigned>(frame.sequence), static_cast<unsigned long>(telemetry.timestamp_ms),
        static_cast<unsigned>(telemetry.state), static_cast<unsigned>(telemetry.flags),
        static_cast<long>(telemetry.altitude_cm), static_cast<int>(telemetry.vertical_speed_cms),
        static_cast<int>(telemetry.rotation[0]), static_cast<int>(telemetry.rotation[1]),
        static_cast<int>(telemetry.rotation[2]), static_cast<int>(telemetry.rotation[3]),
        static_cast<int>(telemetry.gyro_dps_x10[0]), static_cast<int>(telemetry.gyro_dps_x10[1]),
        static_cast<int>(telemetry.gyro_dps_x10[2]), static_cast<int>(telemetry.acceleration_mg[0]),
        static_cast<int>(telemetry.acceleration_mg[1]),
        static_cast<int>(telemetry.acceleration_mg[2]));
}

inline void write_acknowledgement(const rocket::protocol::RadioFrame &frame) {
    const auto &acknowledgement = frame.payload.acknowledgement;
    std::printf("ACK %u %u %u %u %u\n", static_cast<unsigned>(frame.sequence),
                static_cast<unsigned>(acknowledgement.command),
                static_cast<unsigned>(acknowledgement.response),
                static_cast<unsigned>(acknowledgement.state),
                static_cast<unsigned>(acknowledgement.channel));
}

} // namespace rocket::ground::usb_protocol
