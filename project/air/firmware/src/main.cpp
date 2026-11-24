#include "core/spsc_queue.h"
#include "pico/stdlib.h"
#include "protocol/packet.h"

SpscQueue<CommandPacket, 16> g_cmd_queue;

int main() {
    return 0;
}