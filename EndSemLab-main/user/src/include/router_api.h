#ifndef ROUTER_H
#define ROUTER_H

#include <stdint.h>

//=====================================
//      MACROS
//=====================================

#define ROUTER_ADDR 12
#define APPLICATION_ADDR 14
#define ROUTER_LINK_COUNT 4

// This will be the value of the `next_hop_link` field of the `dv_entry_t` struct
// for the entry with the router's subnet as the destination subnet.
#define NO_NEXT_HOP_LINK 0xFF

//=====================================
//      STRUCTURES
//=====================================

/**
 * A struct representing an entry in the routing table.
 */
typedef struct dv_entry {
    // The cost to the destination.
    uint8_t cost;

    // The link which connects this router to the next hop towards the destination.
    // Possible values are from 0 to `ROUTER_LINK_COUNT - 1`, both inclusive,
    // or `NO_NEXT_HOP_LINK` as stated in the MACROS section above.
    uint8_t next_hop_link;
} dv_entry_t;

//=====================================
//      FUNCTIONS
//=====================================

/**
 * NOTE: THESE FUNCTIONS SHOULD ONLY BE CALLED IN THE ROUTER.
 */

/**
 * Sends a packet over a router link to another subnet (For sending to the application, use `send_buffer_to_app()`).
 * `link` - The link to send the packet over. Possible values are from 0 to `ROUTER_LINK_COUNT - 1`, both inclusive.
 * `buf` - The serialised packet to send.
 * `size` - The size of the buffer.
 * Return Value - 0 if the packet was sent properly, else -1.
 */
int send_buffer_to_link(const uint8_t link, const uint8_t *buf, const uint8_t size);

/**
 * Sends a packet to the application.
 * `buf` - The serialised packet to send.
 * `size` - The size of the buffer.
 * Return Value - 0 if the packet was sent properly, else -1.
 */
int send_buffer_to_app(const uint8_t *buf, const uint8_t size);

/**
 * Gets the weight (cost) of a link of the router.
 * `link` - The link. Possible values are from 0 to `ROUTER_LINK_COUNT - 1`, both inclusive.
 * Return Value - The weight of the link (fits in 8 bits) if link was valid, else -1.
 */
int router_get_link_weight(const uint8_t link);

/**
 * Gets the subnet value (6 bits) of the subnet connected to by a link.
 * `link` - The link. Possible values are from 0 to `ROUTER_LINK_COUNT - 1`, both inclusive.
 * Return Value - The 6-bit subnet at the other end of the link if link was valid, else -1.
 */
int router_get_neighbour_subnet(const uint8_t link);

/**
 * Gets the entry from the router distance vector table.
 * `dest_subnet` - The destination subnet (6 bits) which the entry is for.
 * Return Value - A pointer to the entry in the table if it exists, otherwise NULL.
 * NOTE: Do not modify the entry using this pointer. Use `dv_set_entry()` instead.
 */
const dv_entry_t *dv_get_entry(const uint8_t dest_subnet);

/**
 * Sets an entry in the router distance vector table.
 * `dest_subnet` - The destination subnet (6 bits) which the entry is for.
 * `cost` - The cost to the destination.
 * `next_hop_link` - The link which connects this router to the next hop towards the destination. Possible values are from 0 to `ROUTER_LINK_COUNT - 1`, both inclusive.
 * Return Value - 0 if the entry was set properly, else -1.
 */
int dv_set_entry(const uint8_t dest_subnet, const uint8_t cost, const uint8_t next_hop_link);

#endif