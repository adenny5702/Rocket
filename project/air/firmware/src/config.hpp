#pragma once

#include <cstdint>

namespace rocket::air::config {

inline constexpr std::uint32_t startup_delay_ms = 5'000;

namespace telemetry {

inline constexpr std::uint32_t standby_period_ms = 1'000;
inline constexpr std::uint32_t launch_period_ms = 50;
inline constexpr std::uint32_t debug_period_ms = 250;

} // namespace telemetry

namespace debug {

inline constexpr std::uint8_t pyro_channel_count = 2;

}

namespace imu {

inline constexpr std::uint16_t max_lsb_per_axis = 32'768;
inline constexpr std::uint32_t fifo_poll_period_ms = 5;

namespace gyro {

inline constexpr std::uint16_t odr_hz = 800;
inline constexpr std::uint16_t full_scale_dps = 2'000;

} // namespace gyro

namespace accel {

inline constexpr std::uint16_t odr_hz = 800;
inline constexpr std::uint16_t full_scale_g = 32;

} // namespace accel

} // namespace imu

} // namespace rocket::air::config
