#include "pico/multicore.h"
#include "pico/stdlib.h"
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

SpscQueue<CommandPacket, 16> g_cmd_queue;
volatile bool                g_radio_irq_fired = false;

void radio_dio1_isr() {
    g_radio_irq_fired = true;
}

void core1Main() {
    auto state = radio.begin({kRadioFrequency, kRadioBandwidth, kRadioSpreadingFactor,
                              kRadioCodingRate, kRadioSyncWord, kRadioPower});

    if (state != RADIOLIB_ERR_NONE) {
        printf("Radio init failed: %d\n", state);
        while (true) {
            tight_loop_contents();
        }
    }

    radio.setCRC(kRadioCRCLength);

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
            g_cmd_queue.tryPush(pkt);
        }

        radio.startReceive();
    }
}

int main() {
    stdio_init_all();
    sleep_ms(5000);

    printf("Launching core 1...\n");
    multicore_launch_core1(core1Main);

    while (true) {
        CommandPacket pkt;
        if (g_cmd_queue.tryPop(pkt)) {
            printf("CommandType: %u | Key: %u\n", pkt.command, pkt.key);
        }
        tight_loop_contents();
    }
}
