#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "config.hpp"
#include "rocket/shared/packet.hpp"
#include "rocket/shared/pins.hpp"
#include "rocket/shared/radio.hpp"

#include "hal/RPiPico/PicoHal.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

namespace rocket::air {

class RadioLink {
public:
    static void launch(queue_t &received_frames, queue_t &acknowledgements,
                       queue_t &telemetry_frames) {
        received_frames_ = &received_frames;
        acknowledgements_ = &acknowledgements;
        telemetry_frames_ = &telemetry_frames;
        multicore_launch_core1(run);
    }

private:
    using FrameBuffer = std::array<std::uint8_t, sizeof(rocket::protocol::RadioFrame)>;

    inline static queue_t *received_frames_ = nullptr;
    inline static queue_t *acknowledgements_ = nullptr;
    inline static queue_t *telemetry_frames_ = nullptr;
    inline static std::atomic<bool> radio_interrupt_{false};

    static void on_radio_interrupt() {
        radio_interrupt_.store(true, std::memory_order_release);
    }

    static void run() {
        PicoHal hal(spi1, rocket::board::radio_pins::miso, rocket::board::radio_pins::mosi,
                    rocket::board::radio_pins::sck);
        Module module(&hal, rocket::board::radio_pins::cs, rocket::board::radio_pins::irq,
                      rocket::board::radio_pins::reset, rocket::board::radio_pins::gpio);
        SX1262 radio(&module);

        const auto status = rocket::radio::initialize_sx1262(radio);
        if (status != RADIOLIB_ERR_NONE) {
            printf("Air radio initialization failed: %d\n", status);
            stop();
        }

        radio.setDio1Action(on_radio_interrupt);
        radio.startReceive();

        FrameBuffer buffer{};
        std::uint64_t transmit_after_us = 0;
        auto configured_state = rocket::protocol::FlightState::Standby;
        while (true) {
            if (radio_interrupt_.exchange(false, std::memory_order_acquire)) {
                receive_frame(radio, buffer);
                radio.startReceive();
            }

            if (time_us_64() >= transmit_after_us) {
                rocket::protocol::RadioFrame outbound{};
                const bool has_ack =
                    acknowledgements_ != nullptr && queue_try_remove(acknowledgements_, &outbound);
                const bool has_telemetry = !has_ack && telemetry_frames_ != nullptr &&
                                           queue_try_remove(telemetry_frames_, &outbound);
                if (has_ack || has_telemetry) {
                    const auto state = has_ack ? outbound.payload.acknowledgement.state
                                               : outbound.payload.telemetry.state;
                    if (has_telemetry && state != configured_state) {
                        const auto bandwidth_status =
                            rocket::radio::apply_bandwidth_for_state(radio, state);
                        if (bandwidth_status != RADIOLIB_ERR_NONE) {
                            printf("Air radio bandwidth update failed: %d\n", bandwidth_status);
                        } else {
                            configured_state = state;
                        }
                    }
                    transmit_frame(radio, outbound, buffer);
                    if (has_ack && state != configured_state) {
                        const auto bandwidth_status =
                            rocket::radio::apply_bandwidth_for_state(radio, state);
                        if (bandwidth_status != RADIOLIB_ERR_NONE) {
                            printf("Air radio bandwidth update failed: %d\n", bandwidth_status);
                        } else {
                            configured_state = state;
                        }
                    }

                    const bool launch_mode = state == rocket::protocol::FlightState::LaunchMode;
                    const bool launch_uplink_slot =
                        has_telemetry && launch_mode &&
                        outbound.sequence % rocket::config::radio::launch_uplink_interval_packets ==
                            0;
                    const std::uint32_t window_ms =
                        launch_mode
                            ? (launch_uplink_slot ? rocket::config::radio::launch_uplink_window_ms
                                                  : 0)
                            : rocket::config::radio::post_transmit_receive_window_ms;
                    transmit_after_us = time_us_64() + window_ms * 1000;
                    continue;
                }
            }

            tight_loop_contents();
        }
    }

    static void receive_frame(SX1262 &radio, FrameBuffer &buffer) {
        const std::size_t packet_length = radio.getPacketLength();
        if (packet_length > buffer.size() || packet_length < 1) {
            return;
        }

        if (radio.readData(buffer.data(), packet_length) != RADIOLIB_ERR_NONE) {
            return;
        }

        rocket::protocol::RadioFrame received{};
        if (!rocket::protocol::decode_radio_frame(buffer.data(), packet_length, received)) {
            return;
        }

        const auto packet_type = received.type;
        if (packet_type == rocket::protocol::PacketType::Command && received_frames_ != nullptr) {
            queue_try_add(received_frames_, &received);
        }
    }

    static void transmit_frame(SX1262 &radio, const rocket::protocol::RadioFrame &frame,
                               FrameBuffer &buffer) {
        radio.standby();
        radio_interrupt_.store(false, std::memory_order_release);
        const std::size_t frame_length =
            rocket::protocol::encode_radio_frame(frame, buffer.data(), buffer.size());
        if (frame_length == 0) {
            radio.startReceive();
            return;
        }

        if (radio.startTransmit(buffer.data(), frame_length) == RADIOLIB_ERR_NONE) {
            while (!radio_interrupt_.exchange(false, std::memory_order_acquire)) {
                tight_loop_contents();
            }
            radio.finishTransmit();
        }

        radio.startReceive();
    }

    [[noreturn]] static void stop() {
        while (true) {
            tight_loop_contents();
        }
    }
};

} // namespace rocket::air
