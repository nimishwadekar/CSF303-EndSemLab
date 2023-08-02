#include <stdint.h>
#include <string.h>
#include "include/common.h"
#include "include/packet.h"

int is_checksum_valid(const uint8_t *buf, const ssize_t size) {
    uint16_t sum = 0;
    for(ssize_t i = 0; i < size; i++) {
        sum += buf[i];
    }
    return ((sum & 0xFF) + ((sum >> 8) & 0xFF)) == 0xFF;
}

uint8_t compute_checksum(const uint8_t *pkt_buf, const int length) {
    uint16_t sum = 0;
    for(int i = 0; i < length; i++) {
        sum += pkt_buf[i];
    }
    return ~((sum & 0xFF) + ((sum >> 8) & 0xFF));
}

int packet_deserialise(packet_t *pkt, const uint8_t *buf, const ssize_t size) {
    if(!is_checksum_valid(buf, size)) { return -1; }
    if(size < HEADER_SIZE) { return -1; }

    pkt->src = buf[0];
    pkt->dest = buf[1];
    pkt->length = buf[2];

    if(size < pkt->length) { return -1; }

    pkt->ttl = buf[3] & 0x0F;
    pkt->flag_ack = (buf[3] & (1 << 6)) != 0;
    pkt->type = (buf[4] & 0xF0) >> 4;
    pkt->seq_no = (((uint16_t) buf[4] & 0x0F) << 8) | (uint16_t) buf[5];

    if(pkt->type == PACKET_TYPE_DATA) {
        memcpy(pkt->payload_as.data, buf + HEADER_SIZE, pkt->length - HEADER_SIZE);
    }
    else if(pkt->type == PACKET_TYPE_COMMAND) {
        cmd_payload_t *payload = &pkt->payload_as.cmd;
        buf += HEADER_SIZE;

        payload->entry_count = buf[0];
        payload->timestamp = ((uint32_t) (buf[1] & 0x0F) << 16) | ((uint32_t) buf[2] << 8) | ((uint32_t) buf[3]);
        buf += COMMAND_HEADER_SIZE;

        for(int i = 0; i < payload->entry_count; i++, buf += COMMAND_ENTRY_SIZE) {
            cmd_entry_t *entry = &payload->entries[i];
            entry->dest_subnet = buf[0];
            entry->cost = buf[1];
        }
    }
    else {
        return -1;
    }

    return 0;
}

int packet_serialise(const packet_t *pkt, uint8_t *buf, const ssize_t size) {
    if(size < pkt->length) return -1;

    buf[0] = pkt->src;
    buf[1] = pkt->dest;
    buf[2] = pkt->length;

    buf[3] = pkt->ttl | (pkt->flag_ack << 6);
    buf[4] = (pkt->type << 4) | ((pkt->seq_no & 0x0F00) >> 8);
    buf[5] = pkt->seq_no & 0x00FF;
    buf[6] = 0;
    buf[7] = 0;

    uint8_t *pkt_buf = buf;

    if(pkt->type == PACKET_TYPE_DATA) {
        memcpy(buf + HEADER_SIZE, pkt->payload_as.data, pkt->length - HEADER_SIZE);
    }
    else if(pkt->type == PACKET_TYPE_COMMAND) {
        const cmd_payload_t *payload = &pkt->payload_as.cmd;
        buf += HEADER_SIZE;

        buf[0] = payload->entry_count;
        buf[1] = (payload->timestamp & 0x000F0000) >> 16;
        buf[2] = (payload->timestamp & 0x0000FF00) >> 8;
        buf[3] = payload->timestamp & 0x000000FF;

        buf += COMMAND_HEADER_SIZE;
        for(int i = 0; i < payload->entry_count; i++, buf += COMMAND_ENTRY_SIZE) {
            const cmd_entry_t *entry = &payload->entries[i];
            buf[0] = entry->dest_subnet;
            buf[1] = entry->cost;
        }
    }
    else {
        return -1;
    }

    pkt_buf[6] = compute_checksum(pkt_buf, pkt->length);

    return 0;
}

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