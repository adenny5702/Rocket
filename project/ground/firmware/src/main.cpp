#include <atomic>
#include <cstring>

#include "config/radio.hpp"
#include "pins.hpp"
#include "protocol/packet.hpp"

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "RadioLib.h"
#include "hal/RPiPico/PicoHal.h"

static queue_t    g_tx_queue;
std::atomic<bool> g_tx_done_irq{false};

void radio_dio1_isr() {
    g_tx_done_irq.store(true, std::memory_order_release);
}

void core1Main() {
    PicoHal hal(spi1, pins::radio::Miso, pins::radio::Mosi, pins::radio::Sck);
    Module  mod(&hal, pins::radio::Cs, pins::radio::Irq, pins::radio::Rst, pins::radio::Gpio);
    SX1262  radio(&mod);

    auto state = radio.begin({
        cfg::radio::Frequency,
        cfg::radio::Bandwidth,
        cfg::radio::SpreadingFactor,
        cfg::radio::CodingRate,
        cfg::radio::SyncWord,
        cfg::radio::Power,
        cfg::radio::PreambleLength,
    });

    if (state != RADIOLIB_ERR_NONE) {
        printf("Ground Radio init failed: %d\n", state);
        while (true) {
            tight_loop_contents();
        }
    }

    radio.setCRC(cfg::radio::CrcLength);
    radio.setDio1Action(radio_dio1_isr);

    bool    is_transmitting = false;
    uint8_t tx_buffer[sizeof(CommandPacket)];

    while (true) {
        if (is_transmitting) {
            if (g_tx_done_irq.exchange(false, std::memory_order_acquire)) {
                radio.finishTransmit();
                is_transmitting = false;
            }
            tight_loop_contents();
            continue;
        }

        CommandPacket pkt;
        if (queue_try_remove(&g_tx_queue, &pkt)) {
            std::memcpy(tx_buffer, &pkt, sizeof(CommandPacket));

            if (radio.startTransmit(tx_buffer, sizeof(CommandPacket)) == RADIOLIB_ERR_NONE) {
                is_transmitting = true;
            }

            printf("Sent packet -> CommandType: %u | Key: %u\n", pkt.command, pkt.key);
        }

        tight_loop_contents();
    }
}

int main() {
    stdio_init_all();
    queue_init(&g_tx_queue, sizeof(CommandPacket), 16);

    sleep_ms(3000);

    printf("Launching Ground Core 1...\n");
    multicore_launch_core1(core1Main);

    uint8_t key_val = 0;

    while (true) {
        CommandPacket cmd{};
        cmd.command = CommandType::EnterTesting;
        cmd.key     = key_val++;

        if (queue_try_add(&g_tx_queue, &cmd)) {
            printf("Queued send -> CommandType: %u | Key: %u\n", cmd.command, cmd.key);
        }

        sleep_ms(500);
    }

    return 0;
}
