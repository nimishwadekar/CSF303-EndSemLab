#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/common.h"
#include "../include/packet.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

//=====================================
//      DATA
//=====================================

static const uint8_t TEST_BUF_1[] = { 19, 65, 17, 70, 141, 20, 133, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static const uint8_t TEST_BUF_1_INVALID_CHECKSUM[] = { 19, 65, 17, 70, 141, 20, 65, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

static const packet_t TEST_PKT_1 = { 19, 65, 17, 6, 1, PACKET_TYPE_DATA, 3348,
    { .data = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } } 
};

static const uint8_t TEST_BUF_2[] = { 103, 7, 18, 15, 65, 11, 195, 0, 3, 13, 187, 160, 16, 45, 1, 100, 78, 3 };
static const packet_t TEST_PKT_2 = { 103, 7, 18, 15, 0, PACKET_TYPE_COMMAND, 267,
    { .cmd = { 3, 900000, {
        { 16, 45 },
        { 1, 100 },
        { 78, 3 }
    } } } 
};

static int current_test_case, net_assertion;

//=====================================
//      FUNCTIONS
//=====================================

static int packet_eq(const packet_t *a, const packet_t *b) {
    int eq =
        a->src == b->src &&
        a->dest == b->dest &&
        a->length == b->length &&
        a->ttl == b->ttl &&
        a->flag_ack == b->flag_ack &&
        a->type == b->type &&
        a->seq_no == b->seq_no
    ;
    if(!eq) return 0;

    if(a->type == PACKET_TYPE_DATA) {
        return memcmp(a->payload_as.data, b->payload_as.data, a->length - HEADER_SIZE) == 0;
    } else {
        const cmd_payload_t *ac = &a->payload_as.cmd;
        const cmd_payload_t *bc = &b->payload_as.cmd;

        eq = ac->entry_count == bc->entry_count && ac->timestamp == bc->timestamp;
        if(!eq) return 0;

        for(int i = 0; i < ac->entry_count; i++) {
            const cmd_entry_t *ace = &ac->entries[i];
            const cmd_entry_t *bce = &bc->entries[i];
            
            if(ace->cost != bce->cost || ace->dest_subnet != bce->dest_subnet) return 0;
        }

        return 1;
    }
}

static inline void test_case(const int assertion, const char *msg) {
    print(
        "[*] Packet Parsing Test %d [%s]: %s\n",
        current_test_case,
        msg,
        assertion ? ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET : ANSI_COLOR_RED "FAILED" ANSI_COLOR_RESET
    );
    net_assertion = net_assertion && assertion;
    current_test_case += 1;
}

// Returns 1 if all tests pass, else 0.
int test_packet_parsing(void) {
    uint8_t buf[MAX_PACKET_SIZE];
    packet_t pkt;
    current_test_case = 1;
    net_assertion = 1;
    int retval;

    test_case(packet_serialise(&TEST_PKT_1, buf, 4) == -1, "insufficient buffer serialise");
    test_case(packet_deserialise(&pkt, TEST_BUF_1, 4) == -1, "insufficient buffer deserialise");
    test_case(packet_deserialise(&pkt, TEST_BUF_1_INVALID_CHECKSUM, sizeof(TEST_BUF_1_INVALID_CHECKSUM)) == -1, "invalid checksum deserialise");

    retval = packet_serialise(&TEST_PKT_1, buf, sizeof(buf));
    test_case(retval == 0 && memcmp(buf, TEST_BUF_1, TEST_PKT_1.length) == 0, "data packet serialise");

    retval = packet_deserialise(&pkt, TEST_BUF_1, sizeof(TEST_BUF_1));
    test_case(retval == 0 && packet_eq(&pkt, &TEST_PKT_1), "data packet deserialise");

    retval = packet_serialise(&TEST_PKT_2, buf, sizeof(buf));
    test_case(retval == 0 && memcmp(buf, TEST_BUF_2, TEST_PKT_2.length) == 0, "cmd packet serialise");

    retval = packet_deserialise(&pkt, TEST_BUF_2, sizeof(TEST_BUF_2));
    test_case(retval == 0 && packet_eq(&pkt, &TEST_PKT_2), "cmd packet deserialise");

    return net_assertion;
}