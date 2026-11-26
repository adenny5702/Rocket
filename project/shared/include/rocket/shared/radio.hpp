#pragma once

#include <cstdint>

#include "rocket/shared/config.hpp"
#include "rocket/shared/packet.hpp"

#include "RadioLib.h"

namespace rocket::radio {

inline std::int16_t initialize_sx1262(SX1262 &radio) {
    const auto begin_status = radio.begin({
        .frequency = rocket::config::radio::frequency_hz,
        .bandwidth = rocket::config::radio::bandwidth_hz,
        .spreadingFactor = rocket::config::radio::spreading_factor,
        .codingRate = rocket::config::radio::coding_rate,
        .syncWord = rocket::config::radio::sync_word,
        .power = rocket::config::radio::power_dbm,
        .preambleLength = rocket::config::radio::preamble_length_symbols,
    });
    if (begin_status != RADIOLIB_ERR_NONE) {
        return begin_status;
    }

    return radio.setCRC(rocket::config::radio::crc_length_bytes);
}

inline std::int16_t apply_bandwidth_for_state(SX1262 &radio, rocket::protocol::FlightState state) {
    const std::uint32_t bandwidth = state == rocket::protocol::FlightState::LaunchMode
                                        ? rocket::config::radio::launch_bandwidth_hz
                                        : rocket::config::radio::bandwidth_hz;
    const auto standby_status = radio.standby();
    if (standby_status != RADIOLIB_ERR_NONE) {
        return standby_status;
    }

    const auto bandwidth_status = radio.setBandwidth(bandwidth);
    if (bandwidth_status != RADIOLIB_ERR_NONE) {
        return bandwidth_status;
    }

    return radio.startReceive();
}

} // namespace rocket::radio
