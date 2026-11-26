#pragma once

#include <cstdint>

namespace rocket::config::radio {

inline constexpr std::uint32_t frequency_hz = 915'000'000;
inline constexpr std::uint32_t bandwidth_hz = 125'000;
inline constexpr std::uint32_t launch_bandwidth_hz = 500'000;
inline constexpr std::uint8_t spreading_factor = 7;
inline constexpr std::uint8_t coding_rate = 7;
inline constexpr std::uint8_t sync_word = 111;
inline constexpr std::int8_t power_dbm = 22;
inline constexpr std::uint16_t preamble_length_symbols = 8;
inline constexpr std::uint8_t crc_length_bytes = 2;
inline constexpr std::uint32_t post_transmit_receive_window_ms = 60;
inline constexpr std::uint32_t launch_uplink_window_ms = 20;
inline constexpr std::uint16_t launch_uplink_interval_packets = 20;
inline constexpr std::uint32_t transmit_turnaround_guard_ms = 2;
inline constexpr std::uint32_t command_airtime_budget_ms = 35;
inline constexpr std::uint32_t launch_command_airtime_budget_ms = 8;

} // namespace rocket::config::radio
