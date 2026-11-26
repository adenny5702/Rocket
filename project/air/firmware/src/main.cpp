#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "config.hpp"
#include "inertial_unit.hpp"
#include "pins.hpp"
#include "radio_link.hpp"
#include "rocket/shared/packet.hpp"

#include "pico/stdlib.h"
#include "pico/util/queue.h"

namespace {

constexpr std::uint16_t command_queue_capacity = 16;
constexpr std::uint16_t telemetry_queue_capacity = 8;
constexpr std::uint16_t acknowledgement_queue_capacity = 4;

queue_t received_frames;
queue_t telemetry_frames;
queue_t acknowledgement_frames;

rocket::protocol::FlightState flight_state = rocket::protocol::FlightState::Standby;

void initialize_indicators() {
    constexpr std::array<uint, 4> indicator_pins{
        rocket::air::pins::indicators::pyro_1_primary,
        rocket::air::pins::indicators::pyro_1_secondary,
        rocket::air::pins::indicators::pyro_2_primary,
        rocket::air::pins::indicators::pyro_2_secondary,
    };

    for (const uint pin : indicator_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, false);
    }
}

void activate_channel_indicators(std::uint8_t channel) {
    using rocket::air::pins::indicators::pyro_1_primary;
    using rocket::air::pins::indicators::pyro_1_secondary;
    using rocket::air::pins::indicators::pyro_2_primary;
    using rocket::air::pins::indicators::pyro_2_secondary;

    if (channel == 0) {
        gpio_put(pyro_1_primary, true);
        gpio_put(pyro_1_secondary, true);
    } else if (channel == 1) {
        gpio_put(pyro_2_primary, true);
        gpio_put(pyro_2_secondary, true);
    }
}

std::int16_t scale_to_i16(float value, float scale) {
    const float scaled = std::round(value * scale);
    const float bounded = std::clamp(scaled, -32768.0f, 32767.0f);
    return static_cast<std::int16_t>(bounded);
}

void send_acknowledgement(const rocket::protocol::RadioFrame &command_frame,
                          rocket::protocol::AckResponse response) {
    rocket::protocol::RadioFrame acknowledgement{};
    acknowledgement.type = rocket::protocol::PacketType::CommandAck;
    acknowledgement.sequence = command_frame.sequence;
    acknowledgement.payload.acknowledgement = {
        .command = command_frame.payload.command.command,
        .response = response,
        .state = flight_state,
        .channel = command_frame.payload.command.channel,
    };
    if (!queue_try_add(&acknowledgement_frames, &acknowledgement)) {
        printf("Acknowledgement queue full; response dropped.\n");
    }
}

void handle_command(const rocket::protocol::RadioFrame &frame,
                    rocket::air::InertialUnit &inertial_unit) {
    using rocket::protocol::AckResponse;
    using rocket::protocol::CommandType;
    using rocket::protocol::FlightState;

    AckResponse response = AckResponse::Accepted;
    switch (frame.payload.command.command) {
    case CommandType::EnterStandby:
        flight_state = FlightState::Standby;
        break;
    case CommandType::EnterLaunchMode:
        flight_state = FlightState::LaunchMode;
        break;
    case CommandType::EnterDebugMode:
        flight_state = FlightState::DebugMode;
        break;
    case CommandType::ZeroRotation:
        inertial_unit.zero_rotation();
        break;
    case CommandType::TestPyroChannel:
        if (flight_state == FlightState::DebugMode &&
            frame.payload.command.channel < rocket::air::config::debug::pyro_channel_count) {
            activate_channel_indicators(frame.payload.command.channel);
        } else {
            response = AckResponse::Rejected;
        }
        break;
    default:
        response = AckResponse::Rejected;
        break;
    }

    send_acknowledgement(frame, response);
}

std::uint32_t telemetry_period_ms() {
    switch (flight_state) {
    case rocket::protocol::FlightState::LaunchMode:
        return rocket::air::config::telemetry::launch_period_ms;
    case rocket::protocol::FlightState::DebugMode:
        return rocket::air::config::telemetry::debug_period_ms;
    default:
        return rocket::air::config::telemetry::standby_period_ms;
    }
}

void queue_telemetry(std::uint32_t timestamp_ms, std::uint16_t sequence,
                     const rocket::air::InertialUnit &inertial_unit) {
    rocket::protocol::TelemetryPacket telemetry{};
    telemetry.timestamp_ms = timestamp_ms;
    telemetry.state = flight_state;
    telemetry.flags = 0;
    if (inertial_unit.has_data()) {
        telemetry.flags |= rocket::protocol::OrientationValid | rocket::protocol::ImuDataValid;
    }

    const auto &orientation = inertial_unit.orientation();
    const auto &gyro_dps = inertial_unit.gyro_dps();
    const auto &acceleration_g = inertial_unit.acceleration_g();
    telemetry.rotation[0] = scale_to_i16(orientation.w, 32767.0f);
    telemetry.rotation[1] = scale_to_i16(orientation.x, 32767.0f);
    telemetry.rotation[2] = scale_to_i16(orientation.y, 32767.0f);
    telemetry.rotation[3] = scale_to_i16(orientation.z, 32767.0f);

    telemetry.gyro_dps_x10[0] = scale_to_i16(gyro_dps.x, 10.0f);
    telemetry.gyro_dps_x10[1] = scale_to_i16(gyro_dps.y, 10.0f);
    telemetry.gyro_dps_x10[2] = scale_to_i16(gyro_dps.z, 10.0f);
    telemetry.acceleration_mg[0] = scale_to_i16(acceleration_g.x, 1000.0f);
    telemetry.acceleration_mg[1] = scale_to_i16(acceleration_g.y, 1000.0f);
    telemetry.acceleration_mg[2] = scale_to_i16(acceleration_g.z, 1000.0f);

    rocket::protocol::RadioFrame frame{};
    frame.type = rocket::protocol::PacketType::Telemetry;
    frame.sequence = sequence;
    frame.payload.telemetry = telemetry;
    if (!queue_try_add(&telemetry_frames, &frame)) {
        printf("Telemetry queue full; sample skipped.\n");
    }
}

} // namespace

int main() {
    stdio_init_all();
    initialize_indicators();
    queue_init(&received_frames, sizeof(rocket::protocol::RadioFrame), command_queue_capacity);
    queue_init(&telemetry_frames, sizeof(rocket::protocol::RadioFrame), telemetry_queue_capacity);
    queue_init(&acknowledgement_frames, sizeof(rocket::protocol::RadioFrame),
               acknowledgement_queue_capacity);

    rocket::air::InertialUnit inertial_unit;
    if (!inertial_unit.initialize()) {
        while (true) {
            tight_loop_contents();
        }
    }

    rocket::air::RadioLink::launch(received_frames, acknowledgement_frames, telemetry_frames);

    std::uint32_t last_telemetry_ms = 0;
    std::uint16_t next_sequence = 0;
    while (true) {
        const std::uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        inertial_unit.update(now_ms);

        rocket::protocol::RadioFrame command{};
        while (queue_try_remove(&received_frames, &command)) {
            handle_command(command, inertial_unit);
        }

        if (now_ms - last_telemetry_ms >= telemetry_period_ms()) {
            queue_telemetry(now_ms, next_sequence++, inertial_unit);
            last_telemetry_ms = now_ms;
        }

        tight_loop_contents();
    }
}
