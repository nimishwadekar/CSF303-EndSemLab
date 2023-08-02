#include "include/common.h"
#include "include/packet.h"
#include "include/router_api.h"
#include <stdint.h>

// You may use this variable to keep track of the last received timestamp.
static uint32_t last_timestamp = 0;

/**
 * This routine is called when the router receives a packet (from the network or the application).
 * It deserialises the packet and takes actions based on its fields.
 * It drops packets that are considered invalid.
 * `buf` - The byte buffer which holds the packet.
 * `size` - The size of the buffer.
 * `link` - The router link the packet was received on.
 */
void route(uint8_t *buf, const uint8_t size, const uint8_t link) {
    // Deserialise buf into a packet_t.
    // Drop the packet if required.

    // If data packet, serialise and route it correctly to the proper destination (to application or another subnet).
    // Otherwise drop if something is wrong.

    // If command packet, update the DV routing table if required.
    // Keep track of the latest received timestamp.
    // If the table was updated, wrap the routing table into a command packet and send to all links.
    // Assume that the current table entry count never exceeds max capacity of a command packet.
    // Otherwise drop if something is wrong.
}