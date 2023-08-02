#include "include/common.h"
#include "include/packet.h"
#include "include/router_api.h"
#include <stdint.h>

uint32_t last_timestamp = 0;

/**
 * This routine is called when the router receives a packet (from the network or the application).
 * It deserialises the packet and takes actions based on its fields.
 * It drops packets that are considered invalid.
 * `buf` - The byte buffer which holds the packet.
 * `size` - The size of the buffer.
 * `link` - The router link the packet was received on.
 */
void route(uint8_t *buf, const uint8_t size, const uint8_t link) {
    packet_t pkt;
    if(packet_deserialise(&pkt, buf, size) != 0) {
        packet_drop(PACKET_DROP_CHECKSUM_ERROR);
        return;
    }

    if(pkt.type == PACKET_TYPE_DATA) {
        // If application destination, send to application.
        if(pkt.dest == APPLICATION_ADDR) {
            send_buffer_to_app(buf, pkt.length);
            return;
        }

        // Dest is another subnet, has to be routed.
        // If TTL is less than or equal to 1, drop it.
        if(pkt.ttl <= 1) {
            packet_drop(PACKET_DROP_TTL_ZERO);
            return;
        }

        pkt.ttl -= 1;
        if(packet_serialise(&pkt, buf, pkt.length) != 0) return;

        // Addr[7:2] (subnet) used to index.
        const uint8_t dest_subnet = pkt.dest >> 2;
        const dv_entry_t *entry = dv_get_entry(dest_subnet);
        
        if(entry) {
            if(send_buffer_to_link(entry->next_hop_link, buf, pkt.length) != 0) return;
        }
        else {
            packet_drop(PACKET_DROP_NO_ROUTING_ENTRY);
            return;
        }
    }
    else {
        cmd_payload_t *cmd = &pkt.payload_as.cmd;

        // Drop if timestamp is lesser than or equal to last timestamp.
        if(cmd->timestamp <= last_timestamp) {
            packet_drop(PACKET_DROP_OUTDATED_COMMAND);
            return;
        }
        else {
            last_timestamp = cmd->timestamp;
        }

        // Update table if required.
        int did_table_change = 0;
        for(int i = 0; i < cmd->entry_count; i++) {
            const cmd_entry_t *entry = &cmd->entries[i];
            const dv_entry_t *dv = dv_get_entry(entry->dest_subnet);
            const uint8_t new_cost = entry->cost + router_get_link_weight(link);
            if(!dv || dv->cost > new_cost) {
                dv_set_entry(entry->dest_subnet, new_cost, link);
                did_table_change = 1;
            }
        }

        // Broadcast local table if it was updated.
        // Assume that table entry count never exceeds max capacity of a command packet.
        if(did_table_change) {
            uint8_t entry_count = 0;
            const dv_entry_t *entry;
            for(uint8_t dvi = 0; dvi < (1 << 6); dvi++) {
                if(!(entry = dv_get_entry(dvi))) continue;

                cmd->entries[entry_count].dest_subnet = dvi;
                cmd->entries[entry_count].cost = entry->cost;
                entry_count += 1;
            }
            cmd->entry_count = entry_count;

            pkt.length = HEADER_SIZE + COMMAND_HEADER_SIZE + COMMAND_ENTRY_SIZE * entry_count;
            pkt.src = pkt.dest;
            
            for(uint8_t i = 0; i < ROUTER_LINK_COUNT; i++) {
                int neighbour_subnet = router_get_neighbour_subnet(i);
                if(neighbour_subnet < 0) return;
                pkt.dest = neighbour_subnet << 2;

                if(packet_serialise(&pkt, buf, pkt.length) != 0) return;
                if(send_buffer_to_link(i, buf, pkt.length) != 0) return;
            }
        }
    }
}