#include "types.h"
#include "error.h"
#include "net/ethernet.h"

int ethernet_input(const uint8_t* frame, size_t length) {

    eth_header_t * ethhdr = (eth_header_t *)frame;
    log_msg("Got ethernet header @ %#X", ethhdr);

    PANIC("TODO ethernet_input");
    return 0;
}
