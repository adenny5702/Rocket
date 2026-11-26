#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "attitude.hpp"
#include "config.hpp"
#include "pins.hpp"

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "ICM45686.h"

namespace rocket::air {

class InertialUnit {
public:
    InertialUnit() : driver_(i2c1, true) {}

    bool initialize() {
        configure_bus();
        sleep_ms(config::startup_delay_ms);

        int status = driver_.begin();
        if (status != 0) {
            std::printf("IMU initialization failed: %d\n", status);
            return false;
        }

        status = driver_.enableFifoInterrupt(pins::imu::interrupt, on_fifo_interrupt, 10);
        if (status != 0) {
            std::printf("IMU FIFO setup failed: %d\n", status);
            return false;
        }

        status = driver_.startGyro(config::imu::gyro::odr_hz, config::imu::gyro::full_scale_dps);
        if (status != 0) {
            std::printf("IMU gyro setup failed: %d\n", status);
            return false;
        }

        status = driver_.startAccel(config::imu::accel::odr_hz, config::imu::accel::full_scale_g);
        if (status != 0) {
            std::printf("IMU accelerometer setup failed: %d\n", status);
            return false;
        }

        return true;
    }

    void update(std::uint32_t now_ms) {
        const bool interrupt_pending = fifo_ready_.exchange(false, std::memory_order_acquire);
        const bool poll_due = now_ms - last_fifo_poll_ms_ >= config::imu::fifo_poll_period_ms;
        if (!interrupt_pending && !poll_due) {
            return;
        }

        last_fifo_poll_ms_ = now_ms;
        drain_fifo();
    }

    void zero_rotation() {
        orientation_ = {};
    }

    [[nodiscard]] const attitude::Quaternion &orientation() const {
        return orientation_;
    }

    [[nodiscard]] const attitude::Vec3 &gyro_dps() const {
        return latest_gyro_dps_;
    }

    [[nodiscard]] const attitude::Vec3 &acceleration_g() const {
        return latest_acceleration_g_;
    }

    [[nodiscard]] bool has_data() const {
        return has_data_;
    }

private:
    ICM456xx driver_;
    attitude::Quaternion orientation_{};
    attitude::Vec3 latest_gyro_dps_{};
    attitude::Vec3 latest_acceleration_g_{};
    std::uint32_t last_fifo_poll_ms_ = 0;
    bool has_data_ = false;

    inline static std::atomic<bool> fifo_ready_{false};

    static void on_fifo_interrupt(uint, std::uint32_t) {
        fifo_ready_.store(true, std::memory_order_release);
    }

    static void configure_bus() {
        i2c_init(i2c1, 400 * 1000);
        gpio_set_function(pins::i2c_bus_1::sda, GPIO_FUNC_I2C);
        gpio_set_function(pins::i2c_bus_1::scl, GPIO_FUNC_I2C);
        gpio_pull_up(pins::i2c_bus_1::sda);
        gpio_pull_up(pins::i2c_bus_1::scl);
    }

    bool drain_fifo() {
        constexpr float sample_period = 1.0f / config::imu::gyro::odr_hz;
        constexpr float gyro_scale =
            static_cast<float>(config::imu::gyro::full_scale_dps) / config::imu::max_lsb_per_axis;
        constexpr float accel_scale =
            static_cast<float>(config::imu::accel::full_scale_g) / config::imu::max_lsb_per_axis;

        std::uint16_t frame_count = 0;
        if (driver_.getFifoFrameCount(&frame_count) != 0) {
            return false;
        }

        for (std::uint16_t index = 0; index < frame_count; ++index) {
            inv_imu_fifo_data_t sample{};
            if (driver_.getDataFromFifo(sample) != 0) {
                return false;
            }

            latest_gyro_dps_ = {
                sample.byte_16.gyro_data[0] * gyro_scale,
                sample.byte_16.gyro_data[1] * gyro_scale,
                sample.byte_16.gyro_data[2] * gyro_scale,
            };
            latest_acceleration_g_ = {
                sample.byte_16.accel_data[0] * accel_scale,
                sample.byte_16.accel_data[1] * accel_scale,
                sample.byte_16.accel_data[2] * accel_scale,
            };
            orientation_ =
                attitude::integrate_gyro_dps(orientation_, latest_gyro_dps_, sample_period);
            has_data_ = true;
        }

        return true;
    }
};

} // namespace rocket::air
