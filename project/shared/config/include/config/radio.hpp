#pragma once
#include <cstdint>

namespace cfg::radio {

constexpr uint32_t Frequency       = 915'000'000;
constexpr uint32_t Bandwidth       = 125'000;
constexpr uint8_t  SpreadingFactor = 7;
constexpr uint8_t  CodingRate      = 7;
constexpr uint8_t  SyncWord        = 111;
constexpr int8_t   Power           = 22;
constexpr uint16_t PreambleLength  = 8;
constexpr uint8_t  CrcLength       = 2;

} // namespace cfg::radio