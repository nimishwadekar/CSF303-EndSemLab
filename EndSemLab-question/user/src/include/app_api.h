#ifndef APP_API_H
#define APP_API_H

#include <stdint.h>

//=====================================
//      FUNCTIONS
//=====================================

/**
 * NOTE: THESE FUNCTIONS SHOULD ONLY BE CALLED IN THE APPLICATION.
 */

/**
 * Sends a packet to the router.
 * `buf` - The serialised packet to send.
 * `size` - The size of the buffer.
 * Return Value - 0 if the packet was sent properly, else -1.
 */
int send_buffer_to_router(const uint8_t *buf, const uint8_t size);

#endif