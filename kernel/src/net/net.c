
#include "log.h"
#include "error.h"
#include "driver/net/net_if.h"
#include "mm.h"
#include "unicode.h"
#include "net/ethernet.h"
#include "net/ip.h"
#include <string.h>






extern void net_if_init(void);

void net_init(void) {
    net_if_init();
}


void net_poll(void) {
    for (int i=0;i<MAX_IF_DEVS;i++) {
        net_if_device_t* ifdev = if_devices[i];
        if (ifdev>0 && ifdev->ifstate==IF_STATE_STARTED) {
            ifdev->poll(ifdev);
        }
    }
}




// QEMU SLIRP defaults: guest 10.0.2.15, gateway 10.0.2.2, DNS 10.0.2.3.
// Configurable with -netdev user,net=...,host=... - check with -netdev help
// before assuming these.
static const uint8_t TEST_SRC_IP[4] = { 10u, 0u, 2u, 15u };
static const uint8_t TEST_DST_IP[4] = { 10u, 0u, 2u,  2u };

//static const eth_mac_t ETH_BROADCAST = { { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu } };
static const eth_mac_t ETH_ZERO_MAC  = { { 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u } };




// Builds an ARP request into `out`, which must hold at least ETH_FRAME_MIN
// bytes. Returns the padded on-wire length; the chip appends the 4-byte FCS.
static uint16_t eth_test_build_arp(uint8_t *out,const uint8_t *smac) {
    //const uint8_t *mac = rtl8139_get_mac();

    memset(out, 0, 60); //ETH_FRAME_MIN = 60 (excluding FCS)


    eth_header_t *eh = (eth_header_t*) (void *) out;

    //Broadcast
    for (int i=0;i<6;i++) {
        eh->dst.b[i] = 0xFF;
    }
    memcpy(&eh->src, smac, 6u);
    eh->ethertype = (uint16_t) cpu_to_be16((uint16_t)ETH_TYPE_ARP);


    arp_packet_t *arp = (arp_packet_t *)(void *)(out + ETH_HEADER_LEN);

    arp->htype = (uint16_t) cpu_to_be16((uint16_t)ARP_HTYPE_ETHERNET);
    arp->ptype = (uint16_t) cpu_to_be16((uint16_t)ETH_TYPE_IPV4);
    arp->hlen  = 6u;
    arp->plen  = 4u;
    arp->oper  = (uint16_t) cpu_to_be16((uint16_t)ARP_OPER_REQUEST);
    //eth_copy(&arp->sha, mac, 6u);
    memcpy(&arp->sha, smac, 6u);
    memcpy(arp->spa, TEST_SRC_IP, 4);
    //arp->tha = ETH_ZERO_MAC;
    memcpy(&arp->tha, &ETH_ZERO_MAC, 6);
    memcpy(arp->tpa, TEST_DST_IP, 4);

    // 14 + 28 = 42 bytes of content. The minimum on-wire frame is 64 bytes
    // including FCS, so pad to 60 and let the chip add the FCS (AD-7).
    return (uint16_t)ETH_FRAME_MIN;

    //return 0;
}

static void eth_test_hexdump(const char *tag, const uint8_t *data, uint16_t len) {
    log_msg("%s (%u bytes):\n", tag, len);
    for (uint16_t i = 0; i < len; i += 16u) {
        log_msg("  %04x: ", i);
        for (uint16_t j = 0; j < 16u && (uint16_t)(i + j) < len; j++) {
            log_msg("%02x ", data[i + j]);
        }
        log_msg("\n");
    }
}

void net_test_arp(void) {
    static uint8_t fout[60];
    //static uint8_t fin[1518];

    log_msg("Test send an arp message on ifdev[0]\n");
    net_if_device_t* ifdev = if_devices[0];
    net_if_eth_device_t* ethifdev = (net_if_eth_device_t*) ifdev;
    if (ifdev==0 || ifdev->ifstate!=IF_STATE_STARTED) {
        PANIC("ifdev[0] not started");
    }


    uint16_t out_length = eth_test_build_arp(fout,&ethifdev->mac[0]);
    log_msg("eth_test_build_arp returns 0x%04x\n", out_length);
    eth_test_hexdump("T1 ARP request", fout, out_length);


    int rc = ifdev->send(ifdev,fout, out_length);
    if (rc != 0) {
        log_msg("T2: FAIL - rtl8139_send returned %d\n", rc);
        return;
    }
    log_msg("T2: sent. Open eth.pcap: Wireshark should read it as "
            "\"ARP Who has 10.0.2.2? Tell 10.0.2.15\" with no warnings. "
            "Check this BEFORE debugging receive - a malformed frame explains "
            "the absent reply.\n");
}