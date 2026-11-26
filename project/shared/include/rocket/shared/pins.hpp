#pragma once

#include "pico/types.h"

namespace rocket::board::radio_pins {

inline constexpr uint miso = 12;
inline constexpr uint mosi = 11;
inline constexpr uint sck = 10;
inline constexpr uint cs = 9;
inline constexpr uint irq = 5;
inline constexpr uint reset = 13;
inline constexpr uint gpio = 4;

} // namespace rocket::board::radio_pins
