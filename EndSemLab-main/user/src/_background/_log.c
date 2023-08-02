#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "include/log.h"
#include "../include/common.h"

//=====================================
//      MACROS
//=====================================

#define log_write(buf, size) do { fwrite(buf, 1, size, log_file); } while(0)
#define log_flush() do { fflush(log_file); } while(0)

//=====================================
//      ENUMERATIONS
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

FILE *log_file;

//=====================================
//      FUNCTIONS
//=====================================

void log_begin(FILE *file) {
    static const uint8_t MAGIC_BEGIN[4] = { 0xCA, 0xFE, 0xDE, 0xAD };

    log_file = file;
    log_write(MAGIC_BEGIN, sizeof(MAGIC_BEGIN));
    log_flush();
}

void log_end(void) {
    static const uint8_t MAGIC_END[4] = { 0xB0, 0xBA, 0xB0, 0xBA };
    log_write(MAGIC_END, sizeof(MAGIC_END));
    log_flush();
    fclose(log_file);
}

void log_test_number(const uint8_t test) {
    static const uint8_t ID = LOG_TEST_NUMBER;

    log_write(&ID, 1);
    log_write(&test, 1);
    log_flush();
}

void log_send_to_link(const uint8_t *buf, const uint8_t size, const uint8_t link) {
    static const uint8_t ID = LOG_SEND_TO_LINK;

    log_write(&ID, 1);
    log_write(&link, 1);
    log_write(&size, 1);
    log_write(buf, size);
    log_flush();
}

void log_send_to_app(const uint8_t *buf, const uint8_t size) {
    static const uint8_t ID = LOG_SEND_TO_APP;

    log_write(&ID, 1);
    log_write(&size, 1);
    log_write(buf, size);
    log_flush();
}

void log_send_to_router(const uint8_t *buf, const uint8_t size) {
    static const uint8_t ID = LOG_SEND_TO_ROUTER;

    log_write(&ID, 1);
    log_write(&size, 1);
    log_write(buf, size);
    log_flush();
}

void log_dv_set(const uint8_t dest_subnet, const uint8_t cost, const uint8_t next_hop_link) {
    static const uint8_t ID = LOG_DV_SET;

    log_write(&ID, 1);
    log_write(&dest_subnet, 1);
    log_write(&cost, 1);
    log_write(&next_hop_link, 1);
    log_flush();
}

void log_drop(const uint8_t drop_code) {
    static const uint8_t ID = LOG_PACKET_DROP;

    log_write(&ID, 1);
    log_write(&drop_code, 1);
    log_flush();
}