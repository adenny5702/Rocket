#include <cstring>

#include "config/radio.hpp"
#include "pins.hpp"
#include "protocol/packet.hpp"

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "RadioLib.h"
#include "hal/RPiPico/PicoHal.h"

#define kPinRadioMiso 12
#define kPinRadioMosi 11
#define kPinRadioSck  10
#define kPinRadioCs   9
#define kPinRadioIrq  5
#define kPinRadioRst  13
#define kPinRadioGpio 4

static queue_t g_cmd_queue;
volatile bool  g_radio_irq_fired = false;

void radio_dio1_isr() {
    g_radio_irq_fired = true;
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
        printf("Radio init failed: %d\n", state);
        while (true) {
            tight_loop_contents();
        }
    }

    radio.setCRC(cfg::radio::CrcLength);

    radio.setDio1Action(radio_dio1_isr);
    radio.startReceive();

    uint8_t buffer[256];

    while (true) {
        if (!g_radio_irq_fired) {
            tight_loop_contents();
            continue;
        }
        g_radio_irq_fired = false;

        size_t  len      = radio.getPacketLength();
        int16_t rx_state = radio.readData(buffer, len);

        if (rx_state == RADIOLIB_ERR_NONE && len == sizeof(CommandPacket)) {
            CommandPacket pkt;
            std::memcpy(&pkt, buffer, sizeof(CommandPacket));
            queue_try_add(&g_cmd_queue, &pkt);
        }

        radio.startReceive();
    }
}

int main() {
    stdio_init_all();
    queue_init(&g_cmd_queue, sizeof(CommandPacket), 16);

    sleep_ms(5000);

    printf("Launching core 1...\n");
    multicore_launch_core1(core1Main);

    while (true) {
        CommandPacket pkt;
        if (queue_try_remove(&g_cmd_queue, &pkt)) {
            printf("CommandType: %u | Key: %u\n", pkt.command, pkt.key);
        }
        tight_loop_contents();
    }
}
