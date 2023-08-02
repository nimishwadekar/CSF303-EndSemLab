#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdio.h>

//=====================================
//      FUNCTIONS
//=====================================

void log_begin(FILE *file);
void log_end(void);

void log_test_number(const uint8_t test);
void log_send_to_link(const uint8_t *buf, const uint8_t size, const uint8_t link);
void log_send_to_app(const uint8_t *buf, const uint8_t size);
void log_send_to_router(const uint8_t *buf, const uint8_t size);
void log_dv_set(const uint8_t dest_subnet, const uint8_t cost, const uint8_t next_hop_link);
void log_drop(const uint8_t drop_code);

#endif