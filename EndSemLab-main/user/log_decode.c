#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/include/common.h"
#include "src/include/packet.h"

//=====================================
//      FORMAT
//=====================================

// LOG_SEND_TO_LINK: link (1 byte), size (1 byte), buf (`size` bytes)
// LOG_SEND_TO_APP: size (1 byte), buf (`size` bytes)
// LOG_SEND_TO_ROUTER: size (1 byte), buf (`size` bytes)
// LOG_DV_SET: dest subnet (1 byte), cost (1 byte), next hop link (1 byte)
// LOG_TEST_NUMBER: test number (1 byte)
// LOG_DROP: drop code (1 byte)

//=====================================
//      MACROS
//=====================================

#define expect(assertion, what_failed) do { if(!(assertion)) { perror(what_failed); exit(1); } } while(0)

//=====================================
//      STRUCTURES
//=====================================

typedef enum log_type {
    LOG_SEND_TO_LINK = 1,
    LOG_SEND_TO_APP,
    LOG_SEND_TO_ROUTER,
    LOG_DV_SET,
    LOG_TEST_NUMBER,
    LOG_PACKET_DROP,
} log_type_t;

//=====================================
//      DATA
//=====================================

static const uint8_t MAGIC_BEGIN[4] = { 0xCA, 0xFE, 0xDE, 0xAD };
static const uint8_t MAGIC_END[4] = { 0xB0, 0xBA, 0xB0, 0xBA };

//=====================================
//      FUNCTIONS
//=====================================

uint8_t read_byte(uint8_t **ptr) {
    uint8_t byte = **ptr;
    *ptr += 1;
    return byte;
}

uint8_t *read_bytes(uint8_t **ptr, const uint8_t size) {
    uint8_t *bytes = *ptr;
    *ptr += size;
    return bytes;
}

void print_log(FILE *log) {
    fseek(log, 0, SEEK_END);
    long size = ftell(log);
    fseek(log, 0, SEEK_SET);

    uint8_t *buf = malloc(size);
    expect(buf != NULL, "malloc");
    expect(fread(buf, 1, size, log) == size, "read");

    packet_t pkt;

    for(uint8_t *ptr = buf, *end = buf + size; ptr < end; ) {
        printf("\n");

        // check if begin
        if(ptr <= end - sizeof(MAGIC_BEGIN)) {
            if(memcmp(ptr, MAGIC_BEGIN, sizeof(MAGIC_BEGIN)) == 0) {
                printf("********** BEGIN **********\n");
                ptr += sizeof(MAGIC_BEGIN);
                continue;
            }
        }

        // check if end
        if(ptr <= end - sizeof(MAGIC_END)) {
            if(memcmp(ptr, MAGIC_END, sizeof(MAGIC_END)) == 0) {
                printf("********** END **********\n");
                ptr += sizeof(MAGIC_END);
                continue;
            }
        }

        // We assume there are no partial log entries.
        const log_type_t type = read_byte(&ptr);
        switch(type) {
            case LOG_SEND_TO_LINK:
            {
                printf("* SEND_TO_LINK *\n");
                const uint8_t link = read_byte(&ptr);
                const uint8_t size = read_byte(&ptr);
                const uint8_t *data = read_bytes(&ptr, size);

                printf("link: %u\nsize: %u\n", link, size);

                if(packet_deserialise(&pkt, data, size) == 0) {
                    packet_print(&pkt);
                }
                else {
                    printf("Invalid packet: ");
                    for(uint8_t j = 0; j < size; j++) printf("%u ", data[j]);
                    printf("\n");
                }
            }
            break;

            case LOG_SEND_TO_APP:
            {
                printf("* SEND_TO_APP *\n");
                const uint8_t size = read_byte(&ptr);
                const uint8_t *data = read_bytes(&ptr, size);

                printf("size: %u\n", size);

                if(packet_deserialise(&pkt, data, size) == 0) {
                    packet_print(&pkt);
                }
                else {
                    printf("Invalid packet: ");
                    for(uint8_t j = 0; j < size; j++) printf("%u ", data[j]);
                    printf("\n");
                }
            }
            break;

            case LOG_SEND_TO_ROUTER:
            {
                printf("* SEND_TO_ROUTER *\n");
                const uint8_t size = read_byte(&ptr);
                const uint8_t *data = read_bytes(&ptr, size);

                printf("size: %u\n", size);

                if(packet_deserialise(&pkt, data, size) == 0) {
                    packet_print(&pkt);
                }
                else {
                    printf("Invalid packet: ");
                    for(uint8_t j = 0; j < size; j++) printf("%u ", data[j]);
                    printf("\n");
                }
            }
            break;

            case LOG_DV_SET:
            {
                const uint8_t dest_subnet = read_byte(&ptr);
                const uint8_t cost = read_byte(&ptr);
                const uint8_t next_hop_link = read_byte(&ptr);
                printf("* DV_SET *\nDest Subnet: %u\nCost: %u\nNext Hop Link: %u\n", dest_subnet, cost, next_hop_link);
            }
            break;

            case LOG_TEST_NUMBER:
            {
                const uint8_t test = read_byte(&ptr);
                if(test != 0) printf("***** TEST %u *****\n", test);
                else printf("***** TEST *****\n");
            }
            break;

            case LOG_PACKET_DROP:
            {
                const uint8_t drop_code = read_byte(&ptr);
                printf("PACKET_DROP: ");
                switch(drop_code) {
                    case PACKET_DROP_CHECKSUM_ERROR: printf("Checksum Error"); break;
                    case PACKET_DROP_NO_ROUTING_ENTRY: printf("No Routing Entry"); break;
                    case PACKET_DROP_OUTDATED_COMMAND: printf("Outdated Command"); break;
                    case PACKET_DROP_TTL_ZERO: printf("TTL Zero"); break;
                    case PACKET_DROP_TOO_LARGE: printf("Too Large"); break;
                    case PACKET_DROP_GENERAL: printf("General"); break;
                    default: printf("[!] Invalid drop code"); break;
                }
                printf("\n");
            }
            break;

            default:
            printf("[!!!] INVALID LOG ENTRY\n");
            break;
        }
    }

    free(buf);
}

int main() {
    FILE *log = fopen("log/router_log", "rb");
    printf("======== ROUTER LOG ========\n");
    if(log) print_log(log);
    printf("\n============================\n\n");
    if(log) fclose(log);

    log = fopen("log/app_log", "rb");
    printf("========= APP LOG ==========\n");
    if(log) print_log(log);
    printf("\n============================\n\n");
    if(log) fclose(log);
}