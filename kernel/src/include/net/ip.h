
#ifndef _NET_IP_H
#define _NET_IP_H

#include "types.h"

// ===== ARP over Ethernet for IPv4, RFC 826 =========================
typedef struct __attribute__((packed)) {
    uint16_t  htype;      // 1 = Ethernet
    uint16_t  ptype;      // 0x0800 = IPv4
    uint8_t   hlen;       // 6
    uint8_t   plen;       // 4
    uint16_t  oper;       // 1 = request, 2 = reply
    eth_mac_t sha;        // sender hardware address
    uint8_t   spa[4];     // sender protocol address
    eth_mac_t tha;        // target hardware address, zero in a request
    uint8_t   tpa[4];     // target protocol address
} arp_packet_t;
_Static_assert(sizeof(arp_packet_t) == 28, "ARP packet must be 28 bytes");

#define ARP_HTYPE_ETHERNET  1
#define ARP_OPER_REQUEST    1
#define ARP_OPER_REPLY      2

#endif