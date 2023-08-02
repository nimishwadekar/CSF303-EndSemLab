#include <stdint.h>
#include <string.h>
#include "include/common.h"
#include "include/packet.h"

int packet_deserialise(packet_t *pkt, const uint8_t *buf, const ssize_t size) {
    // Deserialise (the binary serialised) buf into pkt.

    return 0;
}

int packet_serialise(const packet_t *pkt, uint8_t *buf, const ssize_t size) {
    // (Binary) Serialise pkt into buf.

    return 0;
}

//=================================================================================================

void packet_print(const packet_t *pkt) {
    print(
        "PACKET:\n \
        src: %u\n \
        dest: %u\n \
        length: %u\n \
        ttl: %u\n \
        flag_ack: %u\n \
        type: %u\n \
        seq_no: %u\n",
        pkt->src, pkt->dest, pkt->length, pkt->ttl, pkt->flag_ack, pkt->type, pkt->seq_no
    );

    if(pkt->type == PACKET_TYPE_DATA) {
        print("data: ");
        for(int i = 0; i < pkt->length - HEADER_SIZE; i++) {
            print("%u ", pkt->payload_as.data[i]);
        }
        print("\n");
    }
    else if(pkt->type == PACKET_TYPE_COMMAND) {
        const cmd_payload_t *payload = &pkt->payload_as.cmd;
        print("command:\n \
        entry_count: %u\n \
        timestamp: %u\n \
        entries:\n",
        payload->entry_count, payload->timestamp);
        for(int i = 0; i < payload->entry_count; i++) {
            print("\
            dest_subnet: %u, cost: %u\n", payload->entries[i].dest_subnet, payload->entries[i].cost);
        }
        print("\n");
    }
}