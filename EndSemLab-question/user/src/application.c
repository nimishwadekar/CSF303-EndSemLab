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
    // Deserialise buf into a packet_t.

    // Drop the packet if required.

    // Append "Hello " to the start of the data payload.

    // Modify header fields as required.

    // Serialise and send to router.
}