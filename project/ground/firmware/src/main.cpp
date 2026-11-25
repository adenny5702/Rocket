#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <atomic>
#include <cstring>

#include "protocol/packet.h"
#include "spsc_queue.h"

#include "RadioLib.h"
#include "hal/RPiPico/PicoHal.h"

#define kPinRadioMiso 12
#define kPinRadioMosi 11
#define kPinRadioSck  10
#define kPinRadioCs   9
#define kPinRadioIrq  5
#define kPinRadioRst  13
#define kPinRadioGpio 4

#define kRadioFrequency       RADIOLIB_UNIT_MEGA(915)
#define kRadioBandwidth       RADIOLIB_UNIT_KILO(125)
#define kRadioSpreadingFactor 9
#define kRadioCodingRate      7
#define kRadioSyncWord        111
#define kRadioPower           22
#define kRadioCRCLength       2

PicoHal hal(spi1, kPinRadioMiso, kPinRadioMosi, kPinRadioSck);
Module  mod(&hal, kPinRadioCs, kPinRadioIrq, kPinRadioRst, kPinRadioGpio);
SX1262  radio(&mod);

SpscQueue<CommandPacket, 16> g_tx_queue;
std::atomic<bool>            g_tx_done_irq{false};

void radio_dio1_isr() {
    g_tx_done_irq.store(true, std::memory_order_release);
}

void core1Main() {
    auto state = radio.begin({kRadioFrequency, kRadioBandwidth, kRadioSpreadingFactor,
                              kRadioCodingRate, kRadioSyncWord, kRadioPower});

    if (state != RADIOLIB_ERR_NONE) {
        printf("Ground Radio init failed: %d\n", state);
        while (true) {
            tight_loop_contents();
        }
    }

    radio.setCRC(kRadioCRCLength);
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
        if (g_tx_queue.tryPop(pkt)) {
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
    sleep_ms(3000);

    printf("Launching Ground Core 1...\n");
    multicore_launch_core1(core1Main);

    uint8_t key_val = 0;

    while (true) {
        CommandPacket cmd{};
        cmd.command = CommandType::EnterTesting;
        cmd.key     = key_val++;

        if (g_tx_queue.tryPush(cmd)) {
            printf("Queued send -> CommandType: %u | Key: %u\n", cmd.command, cmd.key);
        }

        sleep_ms(500);
    }

    return 0;
}
