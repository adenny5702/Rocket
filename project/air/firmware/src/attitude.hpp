#pragma once

#include <cmath>
#include <numbers>

namespace rocket::air::attitude {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quaternion {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Quaternion() noexcept = default;
    constexpr Quaternion(float w_value, float x_value, float y_value, float z_value) noexcept
        : w(w_value), x(x_value), y(y_value), z(z_value) {}

    [[nodiscard]] constexpr Quaternion operator*(const Quaternion &rhs) const noexcept {
        return {w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
                w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w};
    }

    [[nodiscard]] Quaternion normalized() const noexcept {
        const float norm = std::sqrt(w * w + x * x + y * y + z * z);
        if (norm == 0.0f) {
            return {};
        }

        const float inverse_norm = 1.0f / norm;
        return {w * inverse_norm, x * inverse_norm, y * inverse_norm, z * inverse_norm};
    }
};

[[nodiscard]] inline Quaternion integrate_gyro_dps(Quaternion orientation, Vec3 gyro_dps,
                                                   float dt) noexcept {
    constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;
    const float wx = gyro_dps.x * degrees_to_radians;
    const float wy = gyro_dps.y * degrees_to_radians;
    const float wz = gyro_dps.z * degrees_to_radians;
    const float angular_speed = std::sqrt(wx * wx + wy * wy + wz * wz);
    const float angle = angular_speed * dt;

    Quaternion delta;
    if (angle > 1.0e-6f) {
        const float half_angle = 0.5f * angle;
        const float scale = std::sin(half_angle) / angular_speed;
        delta = {std::cos(half_angle), wx * scale, wy * scale, wz * scale};
    } else {
        const float scale = 0.5f * dt;
        delta = {1.0f, wx * scale, wy * scale, wz * scale};
    }

    return (orientation * delta).normalized();
}

} // namespace rocket::air::attitude
