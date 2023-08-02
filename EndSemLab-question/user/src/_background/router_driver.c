#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include "../include/common.h"
#include "../include/packet.h"
#include "../include/router_api.h"
#include "include/log.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define warn(...) do { print("%s[!]%s ", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET); print(__VA_ARGS__); } while(0)
#define error(...) do { print("%s[!!!]%s ", ANSI_COLOR_RED, ANSI_COLOR_RESET); print(__VA_ARGS__); } while(0)
#define no_error(...) do { print("%s[---]%s ", ANSI_COLOR_GREEN, ANSI_COLOR_RESET); print(__VA_ARGS__); } while(0)
#define expect(assertion, what_failed) do { if(!(assertion)) { print("%s[!!!]%s ", ANSI_COLOR_RED, ANSI_COLOR_RESET); perror(what_failed); exit(1); } } while(0)

//=====================================
//      CONSTANTS
//=====================================

#define ERROR_VALUE 0xFF

#define NETSIM_LINK_COUNT 4
#define TOTAL_LINK_COUNT (NETSIM_LINK_COUNT + 1)

#define APP_LINK (TOTAL_LINK_COUNT - 1)
#define NETSIM_LINK_BEGIN 0
#define NETSIM_LINK_END (TOTAL_LINK_COUNT - 2)

#define BASE_LINK_PORT 10000
#define APP_PORT 5000
#define APP_INITIAL_BYTE 5
#define ERROR_PORT 22222

#define SUBNET_MASK_BITS 6
#define SUBNET_ADDRESS_MAX (1 << SUBNET_MASK_BITS)
#define SUBNET(addr) ((addr & 0xFC) >> 2)

#define MY_ADDR 12
#define APP_ADDR 14

//=====================================
//      STRUCTURES
//=====================================

typedef struct dv_entry_wrapper {
    uint8_t is_valid;
    uint8_t dest_subnet;
    dv_entry_t entry;
} dv_entry_wrapper_t ;

typedef struct router_data {
    uint8_t link_weights[NETSIM_LINK_COUNT];

    // Addr[7:2] (subnet) used to index.
    uint8_t dv_entry_count;
    dv_entry_wrapper_t dv[SUBNET_ADDRESS_MAX];
} router_data_t;

//=====================================
//      DATA
//=====================================

static int error_socket;
// Abstract behind API, throw error when offset is wrong.
static int link_sockets[TOTAL_LINK_COUNT];
static int links_yet_inactive = TOTAL_LINK_COUNT;

// Initialise with specific values.
static router_data_t router;

static const uint8_t NEIGHBOUR_SUBNETS[NETSIM_LINK_COUNT] = { 2, 5, 18, 45 };

static uint8_t current_test_id = 0;

//=====================================
//      ROUTER FUNCTIONS
//=====================================

void router_init(void) {
    memset(&router, 0, sizeof(router));
    
    // Set link weights.
    const uint8_t LINK_WEIGHTS[NETSIM_LINK_COUNT] = { 2, 3, 2, 11 };
    for(int i = 0; i < NETSIM_LINK_COUNT; i++) {
        router.link_weights[i] = LINK_WEIGHTS[i];
    }

    // Fill initial routing table.
    router.dv_entry_count = 5;
    const uint8_t INITIAL_DEST_SUBNETS[5] = { 2, 3, 5, 18, 45 };
    const uint8_t INITIAL_COSTS[5] = { 2, 0, 3, 2, 11 };
    const uint8_t INITIAL_NEXT_HOP_LINKS[5] = { 0, ERROR_VALUE, 1, 2, 3 };
    for(int i = 0; i < 5; i++) {
        dv_entry_wrapper_t entry = {
            1,
            INITIAL_DEST_SUBNETS[i],
            {
                INITIAL_COSTS[i],
                INITIAL_NEXT_HOP_LINKS[i]
            }
        };
        router.dv[INITIAL_DEST_SUBNETS[i]] = entry;
    }
}

int router_get_link_weight(const uint8_t link) {
    if(link >= NETSIM_LINK_COUNT) {
        warn("`router_get_link_weight()`: Argument `link` is out of bounds\n");
        return -1;
    }

    return router.link_weights[link];
}

int router_get_neighbour_subnet(const uint8_t link) {
    if(link >= NETSIM_LINK_COUNT) {
        warn("`router_get_neighbour_subnet()`: Argument `link` is out of bounds\n");
        return -1;
    }

    return NEIGHBOUR_SUBNETS[link];
}

const dv_entry_t *dv_get_entry(const uint8_t dest_subnet) {
    if(dest_subnet >= SUBNET_ADDRESS_MAX) {
        warn("`dv_get_entry()`: Argument `dest_subnet` is out of bounds\n");
        return NULL;
    }

    dv_entry_wrapper_t *wrapper = &router.dv[dest_subnet];
    return wrapper->is_valid ? &wrapper->entry : NULL;
}

int dv_set_entry(const uint8_t dest_subnet, const uint8_t cost, const uint8_t next_hop_link) {
    if(dest_subnet >= SUBNET_ADDRESS_MAX) {
        warn("`dv_set_entry()`: Argument `dest_subnet` is out of bounds\n");
        return -1;
    }

    dv_entry_wrapper_t *wrapper = &router.dv[dest_subnet];
    if(!wrapper->is_valid) router.dv_entry_count += 1;

    wrapper->is_valid = 1;
    wrapper->dest_subnet = dest_subnet;
    wrapper->entry.cost = cost;
    wrapper->entry.next_hop_link = next_hop_link;

    // log dv entry
    log_dv_set(dest_subnet, cost, next_hop_link);

    return 0;
}

void dv_print(void) {
    for(int i = 0; i < SUBNET_ADDRESS_MAX; i++) {
        dv_entry_wrapper_t *wrapper = &router.dv[i];
        if(wrapper->is_valid) {
            print("dest %u : [cost %u, next_hop_link %u]\n", wrapper->dest_subnet, wrapper->entry.cost, wrapper->entry.next_hop_link);
        }
    }
}

//=====================================
//      OTHER API FUNCTIONS
//=====================================

int send_buffer_to_link(const uint8_t link, const uint8_t *buf, const uint8_t size) {
    if(link >= NETSIM_LINK_COUNT) return -1;
    if(send(link_sockets[link], buf, size, 0) != size) return -1;
    
    // log
    log_send_to_link(buf, size, link);

    return 0;
}

int send_buffer_to_app(const uint8_t *buf, const uint8_t size) {
    if(send(link_sockets[APP_LINK], buf, size, 0) != size) return -1;

    // log
    log_send_to_app(buf, size);

    return 0;
}

//=====================================
//      FUNCTIONS
//=====================================

void print_results(void) {
    static char buf[4096] = {};
    memset(buf, 0, sizeof(buf));
    expect(recv(error_socket, buf, sizeof(buf), 0) >= 0, "error message recv");
    char *msg = strtok(buf, "\n");
    print("\n==== RESULTS ====\n");
    while(msg) {
        char *result = strrchr(msg, ':');
        *result = 0;
        result += 2;
        char *why = strchr(result, ' ');
        *why = 0;
        why += 1;
        char *colour = ANSI_COLOR_RESET;

        if(*result == 'P') colour = ANSI_COLOR_GREEN;
        else if(*result == 'F') colour = ANSI_COLOR_RED;

        print("[*] %s: %s%s%s %s\n", msg, colour, result, ANSI_COLOR_RESET, why);
        msg = strtok(NULL, "\n");
    }
}

void *link_handler(void *_link) {
    while(links_yet_inactive > 0);

    const uint8_t link = (const uint8_t) (long) _link;
    int sock = link_sockets[link];

    print("[*] Link %d established\n", link);

    int64_t exit_code = 0;

    uint8_t buf[MAX_PACKET_SIZE];
    while(1) {
        memset(buf, 0, MAX_PACKET_SIZE);
        ssize_t bytes_read = recv(sock, buf, MAX_PACKET_SIZE, 0);
        expect(bytes_read > 0, "link packet recv");
        expect(bytes_read <= UINT8_MAX, "packet too big");
        uint8_t size = (uint8_t) bytes_read;

        int flag_err = (buf[3] & (1 << 4)) != 0;
        int flag_end = (buf[3] & (1 << 5)) != 0;

        if(flag_err) {
            exit_code = 1;
            break;
        }
        else if(flag_end) {
            exit_code = 0;
            break;
        }

        if(link != APP_LINK) {
            current_test_id += 1;
            log_test_number(current_test_id);
        }

        void route(uint8_t *buf, const uint8_t size, const uint8_t link);

        route(buf, size, link);
    }

    print("[*] Link %d closing down\n", link);
    pthread_exit((void *) exit_code);
}

int main(const int argc, const char *argv[]) {
    const char *app_ip = "127.0.0.1";

    /* if(argc != 2) {
        error("Expecting exactly 1 argument specifying the network simulation's IP address\n");
        return 1;
    } */
    
    int test_num = argv[1][0] - '0';

    const char *netsim_ip = NULL;

    // Test Packet parsing
    int test_packet_parsing(int);
    test_packet_parsing(test_num);
    return 0;

    //========================= END =======================================================

    FILE *log_file = fopen("log/router_log", "ab");
    if(!log_file) {
        perror("log file open");
        return 1;
    }
    log_begin(log_file);

    router_init();

    pthread_t threads[TOTAL_LINK_COUNT];
    pthread_attr_t attr;
    expect(pthread_attr_init(&attr) == 0, "thread attribute init");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(app_ip);
    addr.sin_port = htons(APP_PORT);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    expect(sock >= 0, "app socket");
    expect(connect(sock, (struct sockaddr *) &addr, sizeof(addr)) >= 0, "app port connect");
    uint8_t byte = APP_INITIAL_BYTE;
    expect(send(sock, &byte, sizeof(uint8_t), 0) == sizeof(uint8_t), "app initial byte send");
    print("[*] Link established with application\n");
    link_sockets[APP_LINK] = sock;
    expect(pthread_create(&threads[APP_LINK], &attr, link_handler, (void *) (long) APP_LINK) == 0, "thread create");
    links_yet_inactive -= 1;

    addr.sin_addr.s_addr = inet_addr(netsim_ip);

    error_socket = socket(AF_INET, SOCK_STREAM, 0);
    expect(error_socket >= 0, "error socket");
    addr.sin_port = htons(ERROR_PORT);
    expect(connect(error_socket, (struct sockaddr *) &addr, sizeof(addr)) >= 0, "error port connect");

    for(int i = NETSIM_LINK_BEGIN; i <= NETSIM_LINK_END; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        expect(sock >= 0, "link socket");

        addr.sin_port = htons(BASE_LINK_PORT + i);
        expect(connect(sock, (struct sockaddr *) &addr, sizeof(addr)) >= 0, "link port connect");

        uint8_t byte = i;
        expect(send(sock, &byte, sizeof(uint8_t), 0) == sizeof(uint8_t), "link ID byte send");

        link_sockets[i] = sock;
        expect(pthread_create(&threads[i], &attr, link_handler, (void *) (long) i) == 0, "thread create");
        links_yet_inactive -= 1;
    }

    long has_error_occured = 0;
    for(int i = NETSIM_LINK_BEGIN; i <= NETSIM_LINK_END; i++) {
        void *retval;
        pthread_join(threads[i], &retval);
        has_error_occured |= (long) retval;
    }

    // Send termination packet to app.
    packet_t pkt = {};
    pkt.length = HEADER_SIZE;
    pkt.type = PACKET_TYPE_DATA;

    uint8_t buf[MAX_PACKET_SIZE];
    if(packet_serialise(&pkt, buf, sizeof(buf)) != 0) {
        print("[!] ERR/END packet serialisation failed");
        exit(1);
    }

    if(has_error_occured) {
        buf[3] = buf[3] | (1 << 4); // Set ERR
        buf[6] -= (1 << 4); // Update checksum
    }
    else {
        buf[3] = buf[3] | (1 << 5); // Set END
        buf[6] -= (1 << 5); // Update checksum
    }

    expect(send(link_sockets[APP_LINK], buf, pkt.length, 0) == pkt.length, "ERR/END packet app send");
    pthread_join(threads[APP_LINK], NULL);

    print_results();
    print("\n");
    if(has_error_occured) {
        error("Routing is incorrect\n");
    }
    else {
        no_error("All routing tests passed\n");
    }
    print("\n");

    // Close all sockets.
    close(error_socket);
    for(int i = 0; i < TOTAL_LINK_COUNT; i++) {
        close(link_sockets[i]);
    }

    log_end();

    print("[*] Router ended\n");

    return 0;
}