#pragma once

#include "pico/types.h"

namespace rocket::air::pins {

namespace i2c_bus_1 {

inline constexpr uint sda = 2;
inline constexpr uint scl = 3;

} // namespace i2c_bus_1

namespace imu {

inline constexpr uint interrupt = 25;

}

namespace indicators {

inline constexpr uint pyro_1_primary = 15;
inline constexpr uint pyro_1_secondary = 18;
inline constexpr uint pyro_2_primary = 16;
inline constexpr uint pyro_2_secondary = 19;

} // namespace indicators

} // namespace rocket::air::pins
