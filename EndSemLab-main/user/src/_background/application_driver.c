#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include "../include/packet.h"
#include "../include/app_api.h"
#include "include/log.h"

#define expect(assertion, what_failed) do { if(!(assertion)) { print("[!] "); perror(what_failed); exit(1); } } while(0)

//=====================================
//      CONSTANTS
//=====================================

#define APP_PORT 5000
#define APP_INITIAL_BYTE 5

//=====================================
//      DATA
//=====================================

int router_sock = 0;

//=====================================
//      API FUNCTIONS
//=====================================

int send_buffer_to_router(const uint8_t *buf, const uint8_t size) {
    if(send(router_sock, buf, size, 0) != size) return -1;

    // log
    log_send_to_router(buf, size);

    return 0;
}

//=====================================
//      FUNCTIONS
//=====================================

int application_loop(void) {
    print("[*] Application initialised\n");
    uint8_t buf[MAX_PACKET_SIZE];
    int exit_code = 0;

    while(1) {
        memset(buf, 0, MAX_PACKET_SIZE);
        ssize_t bytes_read = recv(router_sock, buf, MAX_PACKET_SIZE, 0);
        expect(bytes_read >= 0, "link packet recv");
        expect(bytes_read <= UINT8_MAX, "packet too big");
        uint8_t size = (uint8_t) bytes_read;

        if(size == 0) {
            print("[!] Connection with router terminated\n");
            exit(1);
        }

        int flag_err = (buf[3] & (1 << 4)) != 0;
        int flag_end = (buf[3] & (1 << 5)) != 0;

        if(flag_err) {
            expect(send(router_sock, buf, size, 0) == size, "ERR packet send");
            exit_code = 1;
            break;
        }
        else if(flag_end) {
            expect(send(router_sock, buf, size, 0) == size, "END packet send");
            exit_code = 0;
            break;
        }

        log_test_number(0);

        void application(uint8_t *buf, const uint8_t size);

        application(buf, size);
    }

    print("[*] Link with router closing down\n");
    return exit_code;
}

int main() {
    const char *router_ip = "127.0.0.1";

    FILE *log_file = fopen("log/app_log", "ab");
    if(!log_file) {
        perror("log file open");
        return 1;
    }
    log_begin(log_file);

    print("[*] Waiting for router connection...\n");

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(router_ip);
    addr.sin_port = htons(APP_PORT);

    // Accept connection from router.
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    expect(listen_sock >= 0, "router socket");
    expect(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) >= 0, "REUSEADDR option");
    expect(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) >= 0, "REUSEPORT option");

    expect(bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) >= 0, "bind");
    expect(listen(listen_sock, 0) >= 0, "listen");
    socklen_t addr_len = 0;
    router_sock = accept(listen_sock, (struct sockaddr *) &addr, &addr_len);
    expect(router_sock >= 0, "accept");

    // Receive initial byte.
    uint8_t byte = 0;
    expect(recv(router_sock, &byte, sizeof(byte), 0) > 0, "router initial byte recv");
    if(byte != APP_INITIAL_BYTE) {
        print("[!] Invalid router initial byte");
        exit(1);
    }

    print("[*] Link established with router\n");
    int status = application_loop();

    // Close all sockets.
    close(router_sock);
    close(listen_sock);

    log_end();

    print("[*] Application ended\n");

    return status;
}