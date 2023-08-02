#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common.h"

//=====================================
//      MACROS
//=====================================

// This is the maximum packet size the protocol supports.
#define MAX_PACKET_SIZE 255

#define HEADER_SIZE 8
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)

#define PACKET_TYPE_COMMAND 4
#define PACKET_TYPE_DATA 8

#define COMMAND_HEADER_SIZE 4
#define COMMAND_ENTRY_SIZE 2
#define MAX_COMMAND_ENTRIES ((MAX_PACKET_SIZE - (HEADER_SIZE + COMMAND_HEADER_SIZE)) / COMMAND_ENTRY_SIZE)

//=====================================
//      STRUCTURES
//=====================================
//
//  (All unused fields are always zero.)
//  (All offsets and sizes are in bytes.)
//  (All multi-byte fields are big-endian.)
//  
//  Structure of a packet is: 8 bytes of header, followed by payload.
//
//  Header format:
//  Offset      Size        Description
//  0           1           Source Address
//  1           1           Destination Address
//  2           1           Total length of packet (including header)
//  3           1           Flags + TTL:
//                          Lower 4 bits (0-3) - TTL (4 bits)
//                          Bit 6 - ACK flag
//                          Bits 4, 5, 7 - unused (zero)
//  4           1           Upper 4 bits (4-7) - Type (command packet = 4, data packet = 8)
//                          Lower 4 bits (0-3) - Highest 4 bits (out of 12) of Seq. No.
//  5           1           Lowest 8 bits (out of 12) of Seq. No.
//  6           1           8-bit 1's complement checksum of full packet (including header)
//  7           1           Unused
//
//  Data packet payload is just a raw sequence of bytes.
//
//  Command packet payload format:
//  (All of the following offsets are from the start of the payload)
//
//  Command Header:
//  Offset      Size        Description
//  0           1           No. of DV Table entries in this packet
//  1           1           Upper 4 bits (4-7) - unused
//                          Lower 4 bits (0-3) - Highest 4 bits (out of 20) of timestamp (number of seconds since midnight)
//  2           2           Lower 16 bits (out of 20) of timestamp
//
//  Command Entry:
//  5           1           Destination Subnet
//  6           1           Cost
//  Entry format continues for the rest of the packet. [Entry 2 is at offsets (7, 8), entry 3 at (9, 10), etc.]


typedef struct cmd_entry {
    uint8_t dest_subnet;
    uint8_t cost;
} cmd_entry_t;

typedef struct cmd_payload {
    uint8_t entry_count;
    uint32_t timestamp;
    cmd_entry_t entries[MAX_COMMAND_ENTRIES];
} cmd_payload_t;

typedef struct packet {
    uint8_t src;
    uint8_t dest;
    uint8_t length;

    uint8_t ttl;
    uint8_t flag_ack;
    uint8_t type;

    uint16_t seq_no;

    // This is a union field (since each packet can either have a data payload or a cmd payload.
    // If `pkt` is the name of a packet_t variable,
    // you can access the payload as a data payload with `pkt.payload_as.data`,
    // and as a cmd payload with `pkt.payload_as.cmd`.
    // Make sure to use the `type` field to know which payload to access it as, and not access both payloads on the same packet.
    // Make sure you only access as many elements in the arrays as the corresponding fields in the header say there are.
    union {
        uint8_t data[MAX_PAYLOAD_SIZE];
        cmd_payload_t cmd;
    } payload_as;
} packet_t;

//=====================================
//      FUNCTIONS
//=====================================

/**
 * Deserialises a packet from a byte buffer into a packet struct.
 * `pkt` - The packet struct the data will be filled into.
 * `buf` - The byte buffer to extract the data from.
 * `size` - The size of the buffer.
 * Return Value - 0 if the packet was deserialised correctly with no errors (like invalid checksum). Otherwise -1.
 */
int packet_deserialise(packet_t *pkt, const uint8_t *buf, const ssize_t size);

/**
 * Serialises a packet struct into a byte buffer.
 * `pkt` - The packet struct to serialise.
 * `buf` - The byte buffer to fill.
 * `size` - The size of the buffer.
 * Return Value - 0 if the packet was serialised correctly with no errors. Otherwise -1.
 */
int packet_serialise(const packet_t *pkt, uint8_t *buf, const ssize_t size);

/**
 * Prints a packet struct to standard output.
 * `pkt` - The packet struct to print.
 */
void packet_print(const packet_t *pkt);

#endif