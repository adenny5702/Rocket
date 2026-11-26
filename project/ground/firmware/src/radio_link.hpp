#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "rocket/shared/packet.hpp"
#include "rocket/shared/pins.hpp"
#include "rocket/shared/radio.hpp"

#include "hal/RPiPico/PicoHal.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

namespace rocket::ground {

class RadioLink {
public:
    static void launch(queue_t &outbound_frames, queue_t &received_frames, queue_t &error_queue) {
        outbound_frames_ = &outbound_frames;
        received_frames_ = &received_frames;
        error_queue_ = &error_queue;
        multicore_launch_core1(run);
    }

private:
    using FrameBuffer = std::array<std::uint8_t, sizeof(rocket::protocol::RadioFrame)>;

    inline static queue_t *outbound_frames_ = nullptr;
    inline static queue_t *received_frames_ = nullptr;
    inline static queue_t *error_queue_ = nullptr;
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

        const auto init_status = rocket::radio::initialize_sx1262(radio);
        if (init_status != RADIOLIB_ERR_NONE) {
            report_error(init_status);
            stop();
        }

        radio.setDio1Action(on_radio_interrupt);
        radio.startReceive();

        FrameBuffer buffer{};
        bool uplink_window_open = false;
        bool launch_uplink_window = false;
        std::uint64_t uplink_window_closes_us = 0;
        auto configured_state = rocket::protocol::FlightState::Standby;

        while (true) {
            if (radio_interrupt_.exchange(false, std::memory_order_acquire)) {
                rocket::protocol::RadioFrame received{};
                const bool received_telemetry = receive_frame(radio, buffer, received);
                bool receiver_restarted = false;
                if (received_telemetry ||
                    received.type == rocket::protocol::PacketType::CommandAck) {
                    const auto state = received_telemetry ? received.payload.telemetry.state
                                                          : received.payload.acknowledgement.state;
                    if (state != configured_state) {
                        const auto bandwidth_status =
                            rocket::radio::apply_bandwidth_for_state(radio, state);
                        if (bandwidth_status == RADIOLIB_ERR_NONE) {
                            configured_state = state;
                            receiver_restarted = true;
                        } else {
                            report_error(bandwidth_status);
                        }
                    }
                }
                if (!receiver_restarted) {
                    radio.startReceive();
                }
                if (received_telemetry) {
                    uplink_window_open = true;
                    const bool launch_mode = received.payload.telemetry.state ==
                                             rocket::protocol::FlightState::LaunchMode;
                    const bool launch_uplink_slot =
                        !launch_mode ||
                        received.sequence % rocket::config::radio::launch_uplink_interval_packets ==
                            0;
                    launch_uplink_window = launch_mode && launch_uplink_slot;
                    const std::uint32_t window_ms =
                        !launch_mode
                            ? rocket::config::radio::post_transmit_receive_window_ms
                            : (launch_uplink_slot ? rocket::config::radio::launch_uplink_window_ms
                                                  : 0);
                    uplink_window_closes_us = time_us_64() + window_ms * 1000;
                }
            }

            rocket::protocol::RadioFrame outbound{};
            const std::uint64_t now_us = time_us_64();
            const std::uint64_t airtime_budget_ms =
                launch_uplink_window ? rocket::config::radio::launch_command_airtime_budget_ms
                                     : rocket::config::radio::command_airtime_budget_ms;
            const std::uint64_t command_reserve_us =
                (rocket::config::radio::transmit_turnaround_guard_ms + airtime_budget_ms) * 1000;
            const bool inside_uplink_window =
                uplink_window_open && now_us <= uplink_window_closes_us &&
                uplink_window_closes_us - now_us >= command_reserve_us;
            if (inside_uplink_window && outbound_frames_ != nullptr &&
                queue_try_remove(outbound_frames_, &outbound)) {
                radio.standby();
                radio_interrupt_.store(false, std::memory_order_release);
                sleep_ms(rocket::config::radio::transmit_turnaround_guard_ms);

                FrameBuffer tx_buffer{};
                const std::size_t frame_length = rocket::protocol::encode_radio_frame(
                    outbound, tx_buffer.data(), tx_buffer.size());
                if (frame_length == 0) {
                    radio.startReceive();
                    uplink_window_open = false;
                    launch_uplink_window = false;
                    continue;
                }
                const auto tx_status = radio.startTransmit(tx_buffer.data(), frame_length);
                if (tx_status == RADIOLIB_ERR_NONE) {
                    while (!radio_interrupt_.exchange(false, std::memory_order_acquire)) {
                        tight_loop_contents();
                    }
                    radio.finishTransmit();
                } else {
                    report_error(tx_status);
                }

                radio.startReceive();
                uplink_window_open = false;
                launch_uplink_window = false;
                continue;
            }

            if (uplink_window_open && time_us_64() > uplink_window_closes_us) {
                uplink_window_open = false;
                launch_uplink_window = false;
            }

            tight_loop_contents();
        }
    }

    static bool receive_frame(SX1262 &radio, FrameBuffer &buffer,
                              rocket::protocol::RadioFrame &frame) {
        const std::size_t packet_length = radio.getPacketLength();
        if (packet_length < 1 || packet_length > buffer.size()) {
            return false;
        }

        if (radio.readData(buffer.data(), packet_length) != RADIOLIB_ERR_NONE) {
            return false;
        }

        if (!rocket::protocol::decode_radio_frame(buffer.data(), packet_length, frame)) {
            return false;
        }

        const auto type = frame.type;
        if (received_frames_ != nullptr && (type == rocket::protocol::PacketType::Telemetry ||
                                            type == rocket::protocol::PacketType::CommandAck)) {
            queue_try_add(received_frames_, &frame);
        }

        return type == rocket::protocol::PacketType::Telemetry;
    }

    static void report_error(std::int16_t status) {
        if (error_queue_ != nullptr) {
            queue_try_add(error_queue_, &status);
        }
    }

    [[noreturn]] static void stop() {
        while (true) {
            tight_loop_contents();
        }
    }
};

} // namespace rocket::ground
