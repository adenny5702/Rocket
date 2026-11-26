#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "radio_link.hpp"
#include "rocket/shared/packet.hpp"
#include "usb_protocol.hpp"

#include "pico/stdlib.h"
#include "pico/util/queue.h"

namespace {

constexpr std::uint16_t queue_capacity = 16;
constexpr std::size_t usb_line_capacity = 64;

queue_t outbound_frames;
queue_t received_frames;
queue_t radio_errors;
char usb_line[usb_line_capacity]{};
std::size_t usb_line_length = 0;
bool discarding_oversized_line = false;

void process_usb_line(const char *line) {
    rocket::protocol::RadioFrame frame{};
    if (!rocket::ground::usb_protocol::parse_command_line(line, frame)) {
        rocket::ground::usb_protocol::write_error("INVALID_COMMAND");
        return;
    }

    if (!queue_try_add(&outbound_frames, &frame)) {
        rocket::ground::usb_protocol::write_error("COMMAND_QUEUE_FULL");
    }
}

void poll_usb_commands() {
    constexpr std::uint8_t max_bytes_per_poll = 32;
    for (std::uint8_t count = 0; count < max_bytes_per_poll; ++count) {
        const int input = getchar_timeout_us(0);
        if (input < 0) {
            return;
        }

        if (input == '\r') {
            continue;
        }
        if (input == '\n') {
            if (!discarding_oversized_line) {
                usb_line[usb_line_length] = '\0';
                process_usb_line(usb_line);
            }
            usb_line_length = 0;
            discarding_oversized_line = false;
            continue;
        }

        if (discarding_oversized_line) {
            continue;
        }

        if (usb_line_length + 1 < usb_line_capacity) {
            usb_line[usb_line_length++] = static_cast<char>(input);
        } else {
            usb_line_length = 0;
            discarding_oversized_line = true;
            rocket::ground::usb_protocol::write_error("COMMAND_TOO_LONG");
        }
    }
}

void forward_received_frames() {
    rocket::protocol::RadioFrame frame{};
    while (queue_try_remove(&received_frames, &frame)) {
        switch (frame.type) {
        case rocket::protocol::PacketType::Telemetry:
            rocket::ground::usb_protocol::write_telemetry(frame);
            break;
        case rocket::protocol::PacketType::CommandAck:
            rocket::ground::usb_protocol::write_acknowledgement(frame);
            break;
        default:
            break;
        }
    }
}

} // namespace

int main() {
    stdio_init_all();
    queue_init(&outbound_frames, sizeof(rocket::protocol::RadioFrame), queue_capacity);
    queue_init(&received_frames, sizeof(rocket::protocol::RadioFrame), queue_capacity);
    queue_init(&radio_errors, sizeof(std::int16_t), queue_capacity);

    rocket::ground::RadioLink::launch(outbound_frames, received_frames, radio_errors);

    while (true) {
        poll_usb_commands();
        forward_received_frames();
        std::int16_t radio_error = 0;
        while (queue_try_remove(&radio_errors, &radio_error)) {
            printf("ERR RADIO %d\n", static_cast<int>(radio_error));
        }
        tight_loop_contents();
    }
}
