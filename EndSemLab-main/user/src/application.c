#include <stdint.h>
#include <string.h>
#include "include/app_api.h"
#include "include/common.h"
#include "include/packet.h"

/**
 * This routine is called when the application receives a packet from the router.
 * It deserialises the packet and performs its operation on it before serialising it and sending it back.
 * It drops packets that are considered invalid.
 * `buf` - The byte buffer which holds the packet.
 * `size` - The size of the buffer.
 */
void application(uint8_t *buf, const uint8_t size) {
    packet_t pkt = {};
    if(packet_deserialise(&pkt, buf, size) < 0) {
        packet_drop(PACKET_DROP_CHECKSUM_ERROR);
        return;
    }

    if(pkt.type != PACKET_TYPE_DATA) {
        // only data packets.
        print("[!] Invalid packet type: %d", pkt.type);
        return;
    }

    if(pkt.length > MAX_PACKET_SIZE - 6) {
        // too big.
        packet_drop(PACKET_DROP_TOO_LARGE);
        return;
    }

    uint8_t *payload = pkt.payload_as.data;
    memmove(payload + 6, payload, size);
    memcpy(payload, "Hello ", 6);
    pkt.length += 6;

    pkt.seq_no += 1;
    pkt.ttl = 15;
    pkt.flag_ack = 1;

    uint8_t tmp = pkt.src;
    pkt.src = pkt.dest;
    pkt.dest = tmp;

    if(packet_serialise(&pkt, buf, pkt.length) != 0) return;
    if(send_buffer_to_router(buf, pkt.length) != 0) return;
}