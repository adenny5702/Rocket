#pragma once

#include <array>

namespace rocket::ground::app::orientation_mapping {

using Quaternion = std::array<float, 4>;

[[nodiscard]] constexpr Quaternion to_rocket_model(const Quaternion &sensor_orientation) {
    return {sensor_orientation[0], sensor_orientation[3], sensor_orientation[1],
            sensor_orientation[2]};
}

} // namespace rocket::ground::app::orientation_mapping
