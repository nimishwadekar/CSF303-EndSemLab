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

static uint8_t TEST_BUF_1[] = { 19, 65, 17, 70, 141, 20, 133, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static const uint8_t TEST_BUF_1_CHECKSUMS[] = { 133, 134, 135 };
static const uint8_t TEST_BUF_1_INVALID_CHECKSUM[] = { 19, 65, 17, 70, 141, 20, 65, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

static const packet_t TEST_PKT_1 = { 19, 65, 17, 6, 1, PACKET_TYPE_DATA, 3348,
    { .data = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } } 
};

static uint8_t TEST_BUF_2[] = { 103, 7, 18, 15, 65, 11, 195, 0, 3, 13, 187, 160, 16, 45, 1, 100, 78, 3 };
static const uint8_t TEST_BUF_2_CHECKSUMS[] = { 195, 198, 199, 197 };
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

// Does not verify checksum.
static void test_deser(packet_t *pkt, const uint8_t *buf) {
    pkt->src = buf[0];
    pkt->dest = buf[1];
    pkt->length = buf[2];

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
}

static int score_packet(const packet_t *expected, const packet_t *actual) {
    int score = 0, is_type_correct = 0;
    score +=
        (expected->src == actual->src) * 1 + 
        (expected->dest == actual->dest) * 1 +
        (expected->length == actual->length) * 1 +
        (expected->ttl == actual->ttl) * 2 +
        (expected->flag_ack == actual->flag_ack) * 2 +
        (is_type_correct = (expected->type == actual->type)) * 2 +
        (expected->seq_no == actual->seq_no) * 3
    ;

    
    print("\n\n****************\n*SRC: Expected--%x. Got--%x\n*DST: Expected--%x. Got--%x\n*LEN: Expected--%x. Got--%x\n",expected->src,actual->src,expected->dest,actual->dest,expected->length,actual->length);
    print("*TTL: Expected--%x. Got--%x\n*ACK: Expected--%x. Got--%x\n*TYP: Expected--%x. Got--%x\n*SEQ: Expected--%x. Got--%x\n****************\n\n",expected->ttl,actual->ttl,expected->flag_ack,actual->flag_ack,expected->type,actual->type,expected->seq_no,actual->seq_no);

    if(is_type_correct) {
        if(expected->type == PACKET_TYPE_DATA) {
            if(memcmp(expected->payload_as.data, actual->payload_as.data, expected->length) == 0) {
                score += 2;
            }
            print("*DATA: " );
            for(int ii=0;ii<expected->length;ii++)
            print("%d::%d\t",expected->payload_as.data[ii],actual->payload_as.data[ii]);

        }
        else if(expected->type == PACKET_TYPE_COMMAND) {
            const cmd_payload_t *expc = &expected->payload_as.cmd;
            const cmd_payload_t *actc = &actual->payload_as.cmd;
            score +=
                (expc->entry_count == actc->entry_count) * 1 +
                (expc->timestamp == actc->timestamp) * 3;
            
            int are_all_entries_correct = 1;
            for(int i = 0; i < expc->entry_count; i++) {
                const cmd_entry_t *expce = &expc->entries[i];
                const cmd_entry_t *actce = &actc->entries[i];
                if(expce->cost != actce->cost || expce->dest_subnet != actce->dest_subnet) {
                    are_all_entries_correct = 0;
                }
            }
            score += are_all_entries_correct * 2;
        }
        else {
            print("FATAL: Invalid test case\n");
            return -1;
        }
    }

    return score;
}

static int test_case_error(const int test_num, const char *const msg, const int assertion, const int total_score) {
    int score = assertion ? total_score: 0;
    print("[*] Test %d [%s]: %d / %d points\n", test_num, msg, score, total_score);
    return score;
}

static int test_case_deser(const int test_num, const char *const msg, const int total_score) {
    uint8_t *buf, bufsiz, checksums_count;
    const uint8_t *checksums;
    const packet_t *test_pkt;
    switch(test_num) {
        case 1:
            buf = TEST_BUF_1;
            bufsiz = sizeof(TEST_BUF_1);
            checksums = TEST_BUF_1_CHECKSUMS;
            checksums_count = sizeof(TEST_BUF_1_CHECKSUMS);
            test_pkt = &TEST_PKT_1;
            break;
        
        case 2:
            buf = TEST_BUF_2;
            bufsiz = sizeof(TEST_BUF_2);
            checksums = TEST_BUF_2_CHECKSUMS;
            checksums_count = sizeof(TEST_BUF_2_CHECKSUMS);
            test_pkt = &TEST_PKT_2;
            break;

        default:
            print("Invalid test_num\n");
            return -1;
    }

    int score = 0;
    packet_t pkt = {};
    for(int i = 0; i < checksums_count; i++) {
        buf[6] = checksums[i];
        if(packet_deserialise(&pkt, buf, bufsiz) == 0) {
            score += 2;
            break;
        }
    }

    score += score_packet(test_pkt, &pkt);

    print("[*] Test %d [%s]: %d / %d points\n", test_num, msg, score, total_score);

    return score;
}

static int test_case_ser(const int test_num, const char *const msg, const int total_score) {
    int checksums_count;
    const uint8_t *checksums;
    const packet_t *test_pkt;
    uint8_t buf[256];

    switch(test_num) {
        case 1:
            checksums = TEST_BUF_1_CHECKSUMS;
            checksums_count = sizeof(TEST_BUF_1_CHECKSUMS);
            test_pkt = &TEST_PKT_1;
            break;
        
        case 2:
            checksums = TEST_BUF_2_CHECKSUMS;
            checksums_count = sizeof(TEST_BUF_2_CHECKSUMS);
            test_pkt = &TEST_PKT_2;
            break;

        default:
            print("Invalid test_num\n");
            return -1;
    }

    int score = 0;
    if(packet_serialise(test_pkt, buf, sizeof(buf)) == 0) {
        // Verify checksum using all methods.
        for(int i = 0; i < checksums_count; i++) {
            if(buf[6] == checksums[i]) {
                score += 2;
                break;
            }
        }

        // Deserialise buf and score.
        packet_t pkt = {};
        test_deser(&pkt, buf);
        score += score_packet(test_pkt, &pkt);
    }

    print("[*] Test %d [%s]: %d / %d points\n", test_num, msg, score, total_score);

    return score;
}

// Returns 1 if all tests pass, else 0.
int test_packet_parsing(const int test_num) {
    int score = 0;
    uint8_t buf[MAX_PACKET_SIZE];
    packet_t pkt = {};

    // print("ERROR TESTS:\n");
    // score += test_case_error(1, "insufficient size", packet_serialise(&TEST_PKT_1, buf, 4) == -1, 2);
    // score += test_case_error(2, "insufficient size", packet_deserialise(&pkt, TEST_BUF_1, 4) == -1, 2);
    // score += test_case_error(3, "invalid checksum", packet_deserialise(&pkt, TEST_BUF_1_INVALID_CHECKSUM, sizeof(TEST_BUF_1_INVALID_CHECKSUM)) == -1, 4);
    // print("\n");

    // print("DESERIALISE TESTS:\n");
    // score += test_case_deser(1, "data", 16);
    // score += test_case_deser(2, "command", 20);
    // print("\n");

    // print("SERIALISE TESTS:\n");
    // score += test_case_ser(1, "data", 16);
    // score += test_case_ser(2, "command", 20);
    // print("\n");

    // print("Total score: %d / %d\n", score, 80);
    // print("Scaled-down score: %d / %d\n", (score + 1) / 2, 40);

    switch (test_num)
    {
    case 1:
    // Error test 1
        print("ERROR TEST 1:\n");
        score += test_case_error(1, "insufficient size", packet_serialise(&TEST_PKT_1, buf, 4) == -1, 2);
        break;
    case 2:
        print("ERROR TEST 2:\n");
        score += test_case_error(2, "insufficient size", packet_deserialise(&pkt, TEST_BUF_1, 4) == -1, 2);
        break;
    case 3:
        print("ERROR TEST 3:\n");
        score += test_case_error(3, "invalid checksum", packet_deserialise(&pkt, TEST_BUF_1_INVALID_CHECKSUM, sizeof(TEST_BUF_1_INVALID_CHECKSUM)) == -1, 4);
        break;
    case 4:
        // Deserialise test 1
        print("DESERIALISE TEST 1:\n");
        score += test_case_deser(1, "data", 16);
        break;
    case 5:
        print("DESERIALISE TEST 2:\n");
        score += test_case_deser(2, "command", 20);
        break;
    case 6:
        // Serialise test 1
        print("SERIALISE TEST 1:\n");
        score += test_case_ser(1, "data", 16);
        break;
    case 7:
        print("SERIALISE TEST 2:\n");
        score += test_case_ser(2, "command", 20);
        break;
    
    default:
        break;
    }

    return 0;
}
