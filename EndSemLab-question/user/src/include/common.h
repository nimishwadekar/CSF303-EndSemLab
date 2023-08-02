#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>

//=====================================
//      MACROS
//=====================================

// Listed below are the different drop error codes for which packets must be dropped.

// Packet with erroneous checksum must be dropped.
#define PACKET_DROP_CHECKSUM_ERROR 100

// Packet whose TTL value decreases to zero before sending it to another subnet must be dropped.
#define PACKET_DROP_TTL_ZERO 101

// Packet whose destination subnet has no entry in the routing table must be dropped.
#define PACKET_DROP_NO_ROUTING_ENTRY 102

// Command packet whose timestamp is earlier than the timestamp of the last command packet received must be dropped.
#define PACKET_DROP_OUTDATED_COMMAND 103

// Application must drop a packet if it would exceed the maximum packet size after performing its operation on it.
#define PACKET_DROP_TOO_LARGE 104

// Use this drop code for any other reason for dropping other than the ones mentioned above, if needed.
#define PACKET_DROP_GENERAL 99

/**
 * Prints to screen without buffering. This print macro is recommended over `printf()` for debugging.
 * Arguments - The same arguments as would be passed to `printf()`.
 */
#define print(...) do { fprintf(stderr, __VA_ARGS__); } while(0)

//=====================================
//      FUNCTIONS
//=====================================

/**
 * This function is called when a packet needs to be dropped. Make sure to return from the caller routine after this function has been called.
 * `drop_code` - The reason why the packet was dropped. Use one of the `PACKET_DROP_*` constants.
 */
void packet_drop(const uint8_t drop_code);

#endif