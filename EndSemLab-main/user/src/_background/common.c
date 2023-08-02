#include <stdint.h>
#include "include/log.h"

//=====================================
//      FUNCTIONS
//=====================================

void packet_drop(const uint8_t drop_code) {
    log_drop(drop_code);
}