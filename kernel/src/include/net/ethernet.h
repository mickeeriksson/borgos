
#ifndef _NET_ETHERNET_H
#define _NET_ETHERNET_H

#include "types.h"

#define ETH_HEADER_LEN       14
#define ETH_PAYLOAD_MAX    1500u
#define ETH_FRAME_MIN        60                                // padded, no FCS
#define ETH_FRAME_MAX      (ETH_HEADER_LEN + ETH_PAYLOAD_MAX)   // 1514
#define ETH_FCS_LEN           4u
#define ETH_FRAME_MAX_FCS  (ETH_FRAME_MAX + ETH_FCS_LEN)        // 1518

typedef struct {
    uint8_t b[6];
} eth_mac_t;

// Ethernet II header exactly as it appears on the wire.
// ethertype is big endian on wire; use eth_be16() on every access.
typedef struct __attribute__((packed)) {
    eth_mac_t dst;
    eth_mac_t src;
    uint16_t  ethertype;
} eth_header_t;
_Static_assert(sizeof(eth_header_t) == ETH_HEADER_LEN,"eth_header_t must be exactly 14 bytes on the wire");

#define ETH_TYPE_IPV4  0x0800
#define ETH_TYPE_ARP   0x0806


extern int ethernet_input(const uint8_t* frame, size_t length);


#endif