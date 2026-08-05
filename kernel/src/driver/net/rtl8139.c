#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/net/net_if.h"
#include "error.h"
#include "cpu/mmio.h"
#include "cpu/io.h"
#include "mm.h"
#include "irq.h"
#include "delay.h"
#include "net/ethernet.h"
#include "barrier.h"
#include <string.h>

extern void arch_irq_route_irq(uint8_t irqno, uint8_t cpuid);

// ===== Operational registers (BAR1, 256 bytes) =====================
// ACCESS WIDTH IS NOT A STYLE CHOICE. The width in each comment is the width
// that must be used. Reading a 16-bit register with a 32-bit access often
// works under QEMU and does not on hardware. Three groups are called out
// explicitly by the datasheet:
//
//   IDR0-5, MAR0-7 : WRITE only by 4-byte access; read may be byte/word/dword.
//   TSD0-3         : WRITE only by double-word access. [D 5.2]

#define RTL8139_IDR0     0x00   // 8-bit x6   MAC address, autoloaded from EEPROM
//#define RTL_MAR0     0x08u   // 8-bit x8   multicast filter - left at reset
#define RTL8139_TSD0         0x10   // 32-bit     Transmit Status of Descriptor 0..3, +4 each
#define RTL8139_TSAD0        0x20   // 32-bit     Transmit Start Address 0..3, +4 each
#define RTL8139_RBSTART  0x30   // 32-bit     Rx buffer physical address, dword aligned [C+ 6]
//#define RTL_ERBCR    0x34u   // 16-bit     Early Rx Byte Count - unused, see AD-18
//#define RTL_ERSR     0x36u   // 8-bit      Early Rx Status - unused, see AD-18
#define RTL8139_CR       0x37   // 8-bit      Command Register
#define RTL8139_CAPR     0x38u   // 16-bit     Current Address of Packet Read - see AD-20
//#define RTL_CBR      0x3Au   // 16-bit RO  Current Buffer Address (chip write pointer)
#define RTL8139_IMR      0x3C   // 16-bit     Interrupt Mask
#define RTL8139_ISR      0x3Eu   // 16-bit     Interrupt Status
#define RTL8139_TCR      0x40   // 32-bit     Transmit Configuration
#define RTL8139_RCR      0x44   // 32-bit     Receive Configuration
//#define RTL_MPC      0x4Cu   // 32-bit     Missed Packet Counter, 24-bit value
//#define RTL_9346CR   0x50u   // 8-bit      EEPROM / config-register lock
//#define RTL_CONFIG1  0x52u   // 8-bit      NEVER WRITTEN - see the note below
//#define RTL_MSR      0x58u   // 8-bit      Media Status
//#define RTL_CONFIG4  0x5Au   // 8-bit      RxFIFOAutoClr lives here
//#define RTL_RERID    0x5Eu   // 8-bit RO   PCI revision, reads 0x10 [D 5]


// ----- CR, Command Register. [D 5.4] -----
#define RTL8139_CR_BUFE  (1u << 0)   // RO: Rx buffer empty. The receive loop condition.
#define RTL8139_CR_TE    (1u << 2)   // Transmitter enable
#define RTL8139_CR_RE    (1u << 3)   // Receiver enable
#define RTL8139_CR_RST   (1u << 4)   // Software reset; self-clears when complete


// ----- RCR, Receive Configuration. [D 5.8] -----
//#define RTL8139_RCR_AAP        (1u << 0)   // accept all (promiscuous)
#define RTL8139_RCR_APM        (1u << 1)   // accept physical match
//#define RTL8139_RCR_AM         (1u << 2)   // accept multicast
#define RTL8139_RCR_AB         (1u << 3)   // accept broadcast
//#define RTL8139_RCR_AR         (1u << 4)   // accept runt
//#define RTL8139_RCR_AER        (1u << 5)   // accept error
#define RTL8139_RCR_WRAP       (1u << 7)   // see AD-6

// ----- ISR / IMR. Identical layout. [D 5.5, D 5.6] -----
#define RTL8139_INT_ROK      (1u << 0)   // Receive OK
#define RTL8139_INT_RER      (1u << 1)   // Receive error
#define RTL8139_INT_TOK      (1u << 2)   // Transmit OK
#define RTL8139_INT_TER      (1u << 3)   // Transmit error
#define RTL8139_INT_RBO      (1u << 4)   // Rx Buffer Overflow (host ring exhausted)
//#define RTL_INT_PUN      (1u << 5)   // Packet underrun / link change
#define RTL8139_INT_FOVW     (1u << 6)   // Rx FIFO Overflow
//#define RTL_INT_LENCHG   (1u << 13)  // Cable length change
//#define RTL_INT_TIMEOUT  (1u << 14)
//#define RTL_INT_SERR     (1u << 15)

#define RTL8139_INT_RESERVED 0x1F80u


// ----- TSD, Transmit Status of Descriptor. [D 5.2, PG 1] -----
//
// OWN reads backwards from what the name suggests: the DRIVER writes 0 to hand
// the descriptor over, and the CHIP sets it to 1 when its DMA is done. Reset
// value is 1, and OWN is the only TSD bit the software reset does not clear.
// A descriptor is free again when OWN and TOK are BOTH set [PG 1.5].
//
// Writing SIZE also clears every read-only status bit in the register
// [D 5.2], so they are never acknowledged separately - and TOK from the
// previous transmit must be read before the next one is started.
#define RTL_TSD_SIZE_MASK  0x1FFFu
#define RTL_TSD_OWN        (1u << 13)
#define RTL_TSD_TUN        (1u << 14)
#define RTL_TSD_TOK        (1u << 15)
#define RTL_TSD_ERTXTH_SHIFT 16u       // bits 21:16
#define RTL_TSD_OWC        (1u << 29)
#define RTL_TSD_TABT       (1u << 30)
#define RTL_TSD_CRS        (1u << 31)

// Early Tx threshold: 000000 = 8 bytes, 000001..111111 count in 32-byte units,
// and the threshold must stay under 2K. [D 5.2]
//
// [derived] 32 * 32 = 1024 bytes. Zero would start transmitting after 8 bytes
// were in the FIFO, which maximises the chance of TUN if the PCI bus hesitates.
// Our frames are at most 1792 bytes and already sit complete in memory, so a low
// threshold buys no latency. 1024 gives the DMA a long head start and stays well
// under the 2K ceiling.
#define RTL_ERTXTH_1024   32u


// ----- TCR, Transmit Configuration. [D 5.7] -----
#define RTL_TCR_CLRABT     (1u << 0)   // retransmit after abort; only valid in abort state

#define RTL8139_TX_SLOTS      4
#define RTL8139_TX_SLOT_SIZE  2048


// ===== Receive buffer geometry =====================================
//
// RBLEN = 10 gives 32 KiB + 16 bytes, and WRAP = 1 requires "at least 1.5K
// bytes" of slack past the end [D 5.8, PG 2.4.2]:
//
//   32768 usable + 16 mandatory + 1536 wrap slack = 34320 bytes
//
// which fits an order-4 (65536-byte) allocation with room to spare. The block
// is naturally 64 KiB aligned, which covers RBSTART's double-word requirement
// [C+ 6] with absurd margin - hence no alignment arithmetic anywhere (AD-5).
#define RTL8139_RX_USABLE   32768u
//#define RTL8139_RX_PAD         16u
//#define RTL8139_RX_WRAP_SLACK 1536u
//#define RTL8139_RX_ORDER        4u
//#define RTL8139_RX_ALLOC   (4096u << RTL8139_RX_ORDER)     // 65536

// One mask does both jobs on the read pointer: round down to the next
// double-word boundary, and wrap modulo the usable size. See AD-20.
#define RTL8139_RX_PTR_MASK ((RTL8139_RX_USABLE - 1u) & ~3u)

// The chip adds 16 internally to whatever is written to CAPR, so the driver
// always writes offset-16 and reads back CAPR+16. [PG 2.5] writes it plainly:
//     outport( IOBase + CAPR, RxReadPtrOffset - 0x10);
// and [C+ 6] gives CAPR's reset value as 0FFF0h, i.e. 0 - 16. An empty buffer
// with the reader at offset 0 is CAPR = 0xFFF0, never 0.
#define RTL8139_CAPR_BIAS  16u


const struct pci_device_id rtl8139_pci_idents[] = {
    PCI_DEVICE(0x10EC, 0x8139,"Realtek RTL-8100/8101L/8139 PCI Fast Ethernet Adapter"),
    PCI_DEVICE_END,
};


typedef struct rtl8139_device {

    net_if_eth_device_t  net_if_eth;   //must be first, this way a pointer to usb_hcd is also pointer to usb_hcd...
    adr_t mmio_regbase;
    adr_t io_regbase;

    void* rxbufframe; //64k area holding all rx buffers
    void* txbufframe; //8k area holding all tx buffers

    //uint8_t  mac[6];
    uint32_t  tx_next;          // round-robin descriptor index
    uint32_t  rx_offset;        //offset to receive buffer

} rtl8139_device_t;

// ===== Receive packet header =======================================
//
// The chip prefixes every received frame with four bytes, and the frame is
// followed by its FCS. [C+ 9.1.2]: "All received packets stored in Rx buffer,
// including Rx header and 4-byte CRC, are double-word alignment."
//
// rx_size INCLUDES THE 4 FCS BYTES. [PG 2.5] states it in the sample code:
//     PktLength = pPacketHeader->PacketLength;   //this length include CRC
//     CopyPacket(pIncomePacket, PktLength - 4);  //don't copy 4 bytes CRC
// Copy rx_size bytes and four bytes of CRC ride along into every frame.
typedef struct __attribute__((packed)) {
    uint16_t status;
    uint16_t size;      // frame length INCLUDING FCS
} rtl8139_rx_header_t;
_Static_assert(sizeof(rtl8139_rx_header_t) == 4, "Rx header is 4 bytes");

// Receive status bits, in the packet header. [D 5.1, PG 2.1]
#define RTL8139_RX_ROK    (1u << 0)
#define RTL8139_RX_FAE    (1u << 1)
#define RTL8139_RX_CRC    (1u << 2)
#define RTL8139_RX_LONG   (1u << 3)   // packet exceeds 4 KB
#define RTL8139_RX_RUNT   (1u << 4)   // shorter than 64 bytes including CRC
#define RTL8139_RX_ISE    (1u << 5)   // invalid symbol, 100BASE-TX only
#define RTL8139_RX_BAR    (1u << 13)  // broadcast
#define RTL8139_RX_PAM    (1u << 14)  // physical address matched
#define RTL8139_RX_MAR    (1u << 15)  // multicast

// [PG 2.5] PacketOK(): a frame is dropped if any of these is set.
#define RTL8139_RX_BAD    (RTL8139_RX_FAE | RTL8139_RX_CRC | RTL8139_RX_LONG | RTL8139_RX_RUNT)


// ===== CAPR - the only place the -16 bias lives ====================
//
// AD-20. The chip adds 16 internally to whatever is written here, so the driver
// writes offset-16 and reads back CAPR+16. [PG 2.5]:
//     outport( IOBase + CAPR, RxReadPtrOffset - 0x10);
// and [C+ 6] gives the reset value as 0FFF0h = 0 - 16.
//
// Two functions, one bias, and no other code in this file touches RTL_CAPR.

//static inline uint32_t rtl8139_capr_get(rtl8139_device_t* priv) {
//    uint32_t raw = rtl_read16(RTL_CAPR);
//    return (raw + RTL8139_CAPR_BIAS) & (RTL8139_RX_USABLE - 1u);
//}

static inline void rtl8139_capr_set(rtl8139_device_t* priv,uint32_t offset) {
    adr_t mmioregbase = priv->mmio_regbase;
    mmio_write16(mmioregbase+RTL8139_CAPR, (uint16_t)(offset - RTL8139_CAPR_BIAS));
}


int rtl8139_send(net_if_device_t *ifdev,const void *frame, uint16_t length) {
    rtl8139_device_t* priv = (rtl8139_device_t*) ifdev;
    adr_t mmioregbase = priv->mmio_regbase;
    //net_if_eth_device_t* ethifdev = (net_if_eth_device_t*) ifdev;

    if (ifdev==0 || ifdev->ifstate!=IF_STATE_STARTED) {
        log_msg("ifdev not started\n");
        return ERROR;
    }
    if (frame==0) {
        log_msg("frame is null\n");
        return ERROR;
    }
    if(length>1792) {
        // The chip's limit, not the slot size and not the Ethernet MTU. [D 5.2]:
        // "If the packet length is more than 1792 byte (0700h), the Tx queue will
        // be invalid".
        log_msg("frame is to big\n");
        PANIC("TODO ERROR HANDLING");
    }

    // Pad in software. The chip only pads automatically in C+ mode, gated on
    // C+CR<PAD> [C+ 9.2], and we never enter C+ mode (AD-16). A 42-byte ARP
    // request hits this every time.
    uint16_t paddedlen = (length < ETH_FRAME_MIN) ? (uint16_t)ETH_FRAME_MIN : length;

    uint32_t txslot = priv->tx_next;
    uint8_t *txbuf  = priv->txbufframe + (txslot * RTL8139_TX_SLOT_SIZE);
    memcpy(txbuf, frame, length);
    if (paddedlen > length) {
        memset(txbuf + length, 0,(uint32_t)(paddedlen - length));
    }

    // TSAD before TSD. The write to TSD is the trigger; if TSAD is not set by
    // then the chip DMAs from the previous address.
    uint32_t tsadreg  = txslot * 4;
    mmio_write32(mmioregbase + RTL8139_TSAD0 + tsadreg, V2P(txbuf) );

    cpu_barrier();

    // Writing SIZE clears OWN, and clearing OWN is what starts the DMA [D 5.2, PG 1].
    // The same write also clears every read-only status bit in the register,
    // so TOK below always refers to this transmit.
    uint32_t tsd = ((uint32_t)RTL_ERTXTH_1024 << RTL_TSD_ERTXTH_SHIFT)
                 | ((uint32_t)paddedlen & RTL_TSD_SIZE_MASK);
    mmio_write32(mmioregbase + RTL8139_TSD0 + tsadreg, tsd);


    // A descriptor is done when OWN and TOK are BOTH set -
    // OWN when the DMA into the FIFO finished,
    // TOK when the frame reached the wire.
    // [PG 1.5] CheckTSDStatus() calls that state TSDSTATUS_BOTH.
    const uint32_t donebits = RTL_TSD_OWN | RTL_TSD_TOK;

    uint32_t statusreg = mmio_read32(mmioregbase + RTL8139_TSD0 + tsadreg);
    uint32_t timeout = 10;
    while (timeout != 0) {
        if ((statusreg & donebits) == donebits) {
            priv->tx_next = (txslot + 1) % RTL8139_TX_SLOTS;
            ifdev->stats.tx_ok++;
            return OK;
        }

        if ((statusreg & RTL_TSD_TABT) != 0u) {
            // [D 5.7] CLRABT is the only documented way out of the transmit
            // abort state, and it RETRANSMITS the last descriptor rather than
            // discarding it. We still report the error upward; the caller
            // should not resend, or the frame goes out twice.
            mmio_write32(mmioregbase + RTL8139_TCR, mmio_read32(mmioregbase + RTL8139_TCR) | RTL_TCR_CLRABT);
            priv->tx_next = (txslot + 1) % RTL8139_TX_SLOTS;
            ifdev->stats.tx_err++;
            return ERROR;
        }

        mdelay(2);
        timeout--;
    }
    ifdev->stats.tx_timeout++;
    //PANIC("TODO send");
    return ERROR; //timeout
}


// Throw away whatever is unread and line the software read pointer back up
// with the chip's write pointer.
//
// Used on Rx buffer overflow, and when a packet header is unusable - the length
// field cannot be trusted there, and advancing by a bad length would desync the
// read pointer permanently, which is far worse than dropping a few frames.
static void rtl8139_rx_resync(rtl8139_device_t* priv) {
    PANIC("TODO rtl8139_rx_resync()");
    /*
    uint32_t cbr = rtl_read16(RTL_CBR);
    g_dev.rx_offset = cbr & RTL8139_RX_PTR_MASK;
    rtl_capr_set(g_dev.rx_offset);
    */
}


static void rtl8139_rx_drain(rtl8139_device_t* priv) {
    //PANIC("TODO rtl8139_rx_drain()");
    net_if_device_t *ifdev = (net_if_device_t*) priv;
    adr_t mmioregbase = priv->mmio_regbase;
    // [D 5.4] CR<BUFE>: "Rx Buffer Empty. There is no packet stored in the Rx
    // buffer ring." The chip's own opinion, and the only one that counts -
    // comparing against CBR is not the loop condition. [PG 2.5] uses the same
    // test.
    while ((mmio_read8(mmioregbase+RTL8139_CR) & RTL8139_CR_BUFE) == 0u) {
        //BUFE=0 => has rx buffers

        uint32_t off = priv->rx_offset=0;
        const uint8_t *p = priv->rxbufframe + off;

        // The chip wrote this memory by DMA. x86 keeps it coherent, so no cache
        // maintenance is needed - only a barrier.h so the compiler does not hoist
        // these loads above the volatile CR read that told us data is there.
        cpu_barrier();
        rtl8139_rx_header_t hdr;
        memcpy(&hdr, p, (uint32_t)sizeof(hdr));

        if ((hdr.status & RTL8139_RX_ROK) == 0 || (hdr.status & RTL8139_RX_BAD) != 0) {
            // RCR has AR and AER clear, so the hardware should have filtered
            // these out already. Reaching here means something is off; do not
            // trust hdr.size.
            ifdev->stats.rx_err++;
            rtl8139_rx_resync(priv);
            return;
        }

        // hdr.size INCLUDES the 4-byte FCS. [PG 2.5]:
        //     PktLength = pPacketHeader->PacketLength; //this length include CRC
        //     CopyPacket(pIncomePacket, PktLength - 4); //don't copy 4 bytes CRC
        if (hdr.size < (uint16_t)(ETH_FRAME_MIN + ETH_FCS_LEN) ||
            hdr.size > (uint16_t)ETH_FRAME_MAX_FCS) {
            ifdev->stats.rx_too_big++;
            rtl8139_rx_resync(priv);
            return;
            }

        uint16_t length = (uint16_t)(hdr.size - ETH_FCS_LEN);
        // With WRAP = 1 an overflowing frame is written contiguously past the
        // end of the 32 KiB region, into the slack the allocation reserves
        // (AD-6). So this is always one contiguous copy - no reassembly, which
        // is the whole reason WRAP is set. [PG 2.5]'s sample has a memcpy here
        // precisely because it assumes WRAP = 0.
        //int rc = eth_ring_push(g_dev.rx_ring, p + sizeof(hdr), length);
        int rc = ethernet_input(p + sizeof(hdr), length);
        if (rc == OK) {
            ifdev->stats.rx_ok++;
        } else {
            ifdev->stats.rx_dropped++;
        }

        // Advance past the header, the frame and its FCS, then round up to the
        // next double-word boundary - "each received packet is placed at next
        // available double-word alignment address after the last received
        // packet" [C+ 9.1.2]. Identical to [PG 2.5]:
        //     RxReadPtrOffset = (RxReadPtrOffset + PktLength + 4 + 3) & MASK;
        //
        // The single mask both rounds down to a dword and wraps modulo the
        // usable size. That wrap is exactly right even for a frame that spilled
        // into the slack: if it overflowed by X bytes, the chip puts the next
        // frame at offset X, and (off + consumed) mod 32768 is X. See AD-6.
        priv->rx_offset = (off + hdr.size + 4 + 3) & RTL8139_RX_PTR_MASK;
        rtl8139_capr_set(priv,priv->rx_offset);

    }

}



// ===== Chip variant, from TCR<HWVERID> [D 5.7] =====================
//
// Bits 30:26 (HWVERID_A) together with 23:22 (HWVERID_B) identify the part.
// Logging it costs one register read and tells you which of the two datasheets
// applies where they disagree.
#define RTL_TCR_HWVERID_A_SHIFT 26    // bits 30:26, with 23:22 - chip variant
#define RTL_TCR_HWVERID_B_SHIFT 22
const char *rtl8139_chip_name(uint32_t tcr_hwver){
    uint32_t a = (tcr_hwver >> RTL_TCR_HWVERID_A_SHIFT) & 0x1Fu;
    uint32_t b = (tcr_hwver >> RTL_TCR_HWVERID_B_SHIFT) & 0x03u;

    if (a == 0x18u && b == 0u) { return "RTL8139";        }  // 11000, 00
    if (a == 0x1Cu && b == 0u) { return "RTL8139A";       }  // 11100, 00
    if (a == 0x1Du && b == 0u) { return "RTL8139C";       }  // 11101, 00
    if (a == 0x1Eu && b == 0u) { return "RTL8139B/8130";  }  // 11110, 00
    if (a == 0x1Eu && b == 1u) { return "RTL8100";        }  // 11110, 01
    if (a == 0x1Du && b == 1u) { return "RTL8100B/8139D"; }  // 11101, 01
    if (a == 0x1Du && b == 2u) { return "RTL8139C+";      }  // 11101, 10
    if (a == 0x1Du && b == 3u) { return "RTL8101";        }  // 11101, 11
    return "unknown RTL81xx";
}


int rtl8139_reset(net_if_device_t *ifdev) {
    rtl8139_device_t* priv = (rtl8139_device_t*) ifdev;
    adr_t mmioregbase = priv->mmio_regbase;

    mmio_write8(mmioregbase + RTL8139_CR, RTL8139_CR_RST);
    int timeout = 50; //100ms
    while ( mmio_read8(mmioregbase + RTL8139_CR) & RTL8139_CR_RST ) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for rtl8139 reset\n");
            return ERROR;
        }
        mdelay(2);
    }
    log_msg("rtl8139 reset OK!\n");
    mdelay(10);
    return 0;
}

int rtl8139_setup_buffers(net_if_device_t *ifdev) {
    rtl8139_device_t* priv = (rtl8139_device_t*) ifdev;
    //adr_t mmioregbase = priv->mmio_regbase;

    log_msg("RTL8139 setup rx buffers\n");

    page_t *rxbufpage = page_alloc_pages(GFP_DMA32,4);   //64k i mem < 4gb
    priv->rxbufframe = (uint64_t*)PAGE2VIRT(rxbufpage);
    memset(priv->rxbufframe, 0, PAGESIZE << 4);
    log_msg("Setup RxBufFrame at %#lx\n",priv->rxbufframe);
    priv->rx_offset=0;

    page_t *txbufpage = page_alloc_pages(GFP_DMA32,1);   //8k (4*2048bytes)
    priv->txbufframe = (uint64_t*)PAGE2VIRT(txbufpage);
    memset(priv->txbufframe, 0, PAGESIZE << 1);
    log_msg("Setup TxBufFrame at %#lx\n",priv->txbufframe);
    priv->tx_next = 0;

    return 0;
}

int rtl8139_setup(net_if_device_t *ifdev) {
    rtl8139_device_t* priv = (rtl8139_device_t*) ifdev;
    net_if_eth_device_t* ethifdev = (net_if_eth_device_t*) ifdev;
    adr_t mmioregbase = priv->mmio_regbase;

    // 1.Software Reset:
    // Write 0x10 to the Command Register (0x37) and wait for the reset bit to clear.
    rtl8139_reset(ifdev);

    //clear stats
    memset(&ifdev->stats,0x0,sizeof(if_stats_t));

    //get HW version from TCR
    uint32_t tcr_hwver = mmio_read32(mmioregbase + RTL8139_TCR);
    log_msg("RTL8139 chip name = %s\n", rtl8139_chip_name(tcr_hwver));

    //get MAC address
    for (int i = 0; i < 6; i++) {
        ethifdev->mac[i] = mmio_read8(mmioregbase + RTL8139_IDR0 + i);
    }
    log_msg("rtl8139: %s MAC:%02x:%02x:%02x:%02x:%02x:%02x\n",
                rtl8139_chip_name(tcr_hwver),
                ethifdev->mac[0], ethifdev->mac[1], ethifdev->mac[2],
                ethifdev->mac[3], ethifdev->mac[4], ethifdev->mac[5]);

    //Setup buffers
    rtl8139_setup_buffers(ifdev);

    // 2.Set up Buffer Pointer:
    // Write the physical memory address of your network ring buffer to the RBSTART register (0x30).
    mmio_write32(mmioregbase + RTL8139_RBSTART,V2P(priv->rxbufframe));

    // 3. Configure RCR:
    // Write your desired packet filtering and DMA configurations to the RCR register (0x44).
    //
    // RCR. AD-6 for RBLEN/WRAP, AD-18 for RXFTH/ERTH/MXDMA.
    //   APM  - accept frames addressed to us
    //   AM   - multicast NOT set
    //   AB   - accept broadcast; ARP requests are broadcast, so this is not
    //          optional for the test in eth_test.c
    //   WRAP - let the chip run an overflowing frame past the end of the buffer
    //          instead of splitting it, so there is no reassembly path at all
    //   AAP  - NOT set. Promiscuous mode is the first knob to turn when an
    //          expected frame does not appear, but it is not normal operation.
    //   AR, AER - NOT set. Broken frames must not reach the ring.

    uint32_t rmxdma = (uint32_t) 4 << 8; //256 bytes
    uint32_t rblen = (uint32_t) 2 << 11; // Rx buffer length. [D 5.8]  00=8K+16 01=16K+16 10=32K+16 11=64K+16
    uint32_t rxfth = (uint32_t) 4 << 13; //4=256 bytes

    uint32_t rcr = RTL8139_RCR_APM | RTL8139_RCR_AB | RTL8139_RCR_WRAP
                 | rmxdma | rblen | rxfth;
    mmio_write32(mmioregbase + RTL8139_RCR, rcr);

    //set CAPR (current rx buf?) ; skip this for now, should be 0xFFF0 after reset.
    uint16_t capr = mmio_read16(mmioregbase + RTL8139_CAPR);
    log_msg("CAPR = %#x  (should be 0xFFF0)\n",capr);

    //4. Enable TX/RX Engines:
    // Write 0x0C (sets both RE and TE high) to the Command Register (0x37).
    mmio_write8(mmioregbase + RTL8139_CR, RTL8139_CR_RE | RTL8139_CR_TE);

    // 5. Configure TCR:
    // Now that TE is high, safely write your configuration to the TCR register (0x40).
    uint32_t tmxdma = (uint32_t) 4 << 8; //256 bytes
    uint32_t tcr = tmxdma;
    mmio_write32(mmioregbase + RTL8139_TCR, tcr);


    log_msg("RTL8139 setup overflow handling\n");
    // MPC counts packets discarded after an Rx FIFO overflow [C+ 6.7].
    //  Any write resets it [D 5], so this zeroes the counter for a clean baseline.
    //  Already done by reset()
    // rtl_write32(RTL_MPC, 0u);

    // Let the hardware clear Rx FIFO overflows by itself (AD-13). Documented in
    // both datasheets [D 5.14, C+ 6.15], and it is what keeps the software
    // recovery path small.
    // skip for now????
    //rtl_config_set_rxfifo_autoclr();

    //Clear ISR
    uint16_t pending = mmio_read16(mmioregbase + RTL8139_ISR);
    if (pending != 0u) {
        mmio_write16(mmioregbase + RTL8139_ISR, (uint16_t)(pending & (uint16_t)(~RTL8139_INT_RESERVED & 0xFFFF)));
    }

    //6. Interrupt Mask: Unmask your desired network interrupts by configuring the IMR register (0x3C).
    uint16_t imr_enabled =  (RTL8139_INT_ROK | RTL8139_INT_RER | RTL8139_INT_TOK | RTL8139_INT_TER | RTL8139_INT_RBO | RTL8139_INT_FOVW);
    mmio_write16(mmioregbase + RTL8139_IMR, imr_enabled);

    ifdev->ifstate = IF_STATE_SETUP;

    //PANIC("SETUP TODO");
    return 0;
}
int rtl8139_start(net_if_device_t *ifdev) {
    ifdev->ifstate = IF_STATE_STARTED;
    log_msg("RTL 8139 STARTED\n");
    return 0;
}
int rtl8139_stop(net_if_device_t *ifdev) {
    PANIC("TODO rtl8139_stop");
    return 0;
}
int rtl8139_release(net_if_device_t *ifdev) {
    PANIC("TODO rtl8139_release");
    return 0;
}
int rtl8139_poll(net_if_device_t *ifdev) {
    //PANIC("TODO");
    return 0;
}


irq_status_t rtl8139_irqhandler(void *ctx){
    device_t *dev = (device_t *) ctx;
    rtl8139_device_t* priv = dev->priv;
    //net_if_eth_device_t* ethifdev = (net_if_eth_device_t*) priv;
    net_if_device_t *ifdev = (net_if_device_t*) priv;
    log_msg("IRQ ON dev %s\n",dev->name);

    adr_t mmioregbase = priv->mmio_regbase;


    if (ifdev->ifstate != IF_STATE_STARTED) {
        return IRQ_NOT_KNOWN;
    }

    uint16_t isrstatus = mmio_read16(mmioregbase+RTL8139_ISR);
    if (isrstatus == 0u) {
        // Not ours. INTA# is open drain [C+ 8.1.2], so the line is shared and
        // being called for someone else's interrupt is normal, not an error.
        ifdev->stats.irq_spurious++;
        return IRQ_NOT_MINE;
    }

    uint16_t israck = isrstatus;

    if ((isrstatus & (RTL8139_INT_RBO | RTL8139_INT_FOVW)) != 0) {
        ifdev->stats.rx_overflow++;

        // [PG 2.3.1] Rx buffer overflow: "The Rx DMA(FIFO to buffer) is
        // stopped, The CAPR must be updated first to dismiss the
        // ISR(RxBufferOverflow) event.. The correct actions to process RxBufOvw
        // is: (a). Update CAPR. (b). write a '1' to ISR(ROK). The Rx DMA
        // resumes after step (b)."
        //
        // So CAPR moves BEFORE the acknowledge below, and ROK must be in the
        // acknowledge even when ROK was not the bit that fired.
        rtl8139_rx_resync(priv);
        //PANIC("TODO rtl8139_rx_resync()");

        // [PG 2.3.2] Rx FIFO overflow: "Clear ISR(RxFIFOOvw) doesn't dismiss
        // RxFIFOOvw event. To dismiss RxFIFOOvw event, the ISR(RxBufOvw) must
        // be written with a '1'."
        //
        // Both quirks are satisfied by forcing RBO and ROK into the acknowledge.
        // Neither is guessable from the register tables, and getting either
        // wrong wedges reception permanently.
        israck = (uint16_t)(israck | RTL8139_INT_RBO | RTL8139_INT_ROK);
    }

    // Acknowledge before processing. A frame that arrives mid-handler then
    // raises a fresh interrupt instead of being lost silently; acknowledging
    // last would drop it. Reserved bits 12:7 are masked out [D 5.6] - writing
    // ones there is exactly what QEMU's -d guest_errors reports.
    //
    // This is write-1-to-clear, per [C+ 6.7] and [PG]. [D 5.6] claims otherwise
    // and is wrong; see the note in rtl8139.h and ARCHITECTURE.md correction C2.
    mmio_write16(mmioregbase+RTL8139_ISR, (uint16_t)(israck & (uint16_t)(~RTL8139_INT_RESERVED & 0xFFFFu)));

    if ((isrstatus & RTL8139_INT_RER) != 0) {
        ifdev->stats.rx_err++;
    }
    if ((isrstatus & RTL8139_INT_TER) != 0) {
        ifdev->stats.tx_err++;
    }

    // [PG 2.3.4] suggested handling: clear the overflow/ROK group, then
    //     if (ROK) { while (BufEmpty == 0) { read one packet then update CAPR } }
    if ((isrstatus & RTL8139_INT_ROK) != 0) {
        //rtl_rx_drain();
        rtl8139_rx_drain(priv);
    }

    // TOK needs nothing here: rtl8139_send() blocks on TSD itself (AD-7).
    return IRQ_HANDLED;

}

int rtl8139_pci_probe(device_t *dev) {
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("rtl8139_pci_probe\n");
    rtl8139_device_t* priv = kmalloc(sizeof(rtl8139_device_t),0);
    dev->priv = priv;

    dev->name = "RealTek8139";

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "rtl8139 PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }



    // BAR0 is a 64-bit memory BAR on every xHCI implementation. Probe its
    // size the standard PCI way: write all ones, read back, restore.
    if (pcidev->bar[1].bartype != BARTYPE_MEM) {
        PANIC("RTL8139 driver ONLY SUPPORTS MEMORY BAR");
    }


    log_msg("BAR 1 (MEMORY)   = 0x%lx \n",pcidev->bar[1].addr);
    adr_t reg_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    reg_base = pci_iomap(pcidev, &pcidev->bar[1], 0);
    if (!reg_base) {
        pci_release_region(pcidev, &pcidev->bar[1]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map reg_base via pci_iomap");
        return -ENOMEM;
    }
    priv->io_regbase=0;  //disable
    priv->mmio_regbase=reg_base;
    //adr_t mmioregbase = priv->mmio_regbase;


    uint8_t revisionID = pci_config_read8(pcidev,0x08); //0x08, Revision ID
    log_msg("revisionID= %#x\n",revisionID);
    //if(revisionID!=0x00){
    //    PANIC("STRANGE revisionID FOR RTL8139");
    //    return ERROR;
    //}

    //Enable I/O + bumastering + intx disable
    uint16_t command = pci_config_read16(pcidev,PCICONFIG_COMMAND);
    log_msg("command (default) = %#x\n",command);
    command = command | PCICONFIG_CMD_MEM_SPACE | PCICONFIG_CMD_BUS_MASTER | PCICONFIG_CMD_INTX_DISABLE;  // bit0=I/O Space, bit1=Memory Space, bit2=Bus Master, bit10 = intx disable
    log_msg("command to write = %#x\n",command);
    pci_config_write16(pcidev,PCICONFIG_COMMAND,command);

    //test MSI support
    uint32_t capptr_msi = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSI);
    uint32_t capptr_msix = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSIX);
    log_msg("Found MSI Capability PTR = 0x%x\n", capptr_msi);
    log_msg("Found MSIX Capability PTR = 0x%x\n", capptr_msix);

    if (capptr_msi==0 && capptr_msix==0) {
        log_msg("This RTL8139 Adapter has no MSI or MSIX support, this is expected!\n");

        uint8_t irq_line = pci_config_read8(pcidev, PCICONFIG_IRQLINE);
        log_msg("irq_line= %d (%#x)\n",irq_line,irq_line);


        //route irq
        log_msg("Route IRQ to  = irq line %d\n",irq_line);
        irq_add_irq_handler(irq_line,rtl8139_irqhandler,dev);

        //Enable int x
        log_msg("Enable Int x in PCI Command Reg\n",irq_line);
        uint16_t command = pci_config_read16(pcidev,PCICONFIG_COMMAND);
        log_msg("command = %#x\n",command);
        log_msg("mask %#x\n",~PCICONFIG_CMD_INTX_DISABLE);
        command = command & ~PCICONFIG_CMD_INTX_DISABLE;  // clear bit10 = intx disable
        log_msg("command to write = %#x\n",command);
        pci_config_write16(pcidev,PCICONFIG_COMMAND,command);

        arch_irq_route_irq(irq_line,0); //always route to id=0;

    }

    uint32_t pmptr= pci_config_get_capabillity_ptr(pcidev,0x01);
    log_msg("pmptr = %#x\n",pmptr);
    log_msg("Bring device out of low power mode!!!\n");

    priv->net_if_eth.net_if_dev.name = "RTL8139";
    priv->net_if_eth.net_if_dev.setup=rtl8139_setup;
    priv->net_if_eth.net_if_dev.start=rtl8139_start;
    priv->net_if_eth.net_if_dev.stop=rtl8139_stop;
    priv->net_if_eth.net_if_dev.release=rtl8139_release;
    priv->net_if_eth.net_if_dev.poll=rtl8139_poll;

    priv->net_if_eth.net_if_dev.send=rtl8139_send;

    dev->initialized = 1;
    return 0;
}

int rtl8139_pci_attach(device_t *dev) {
    rtl8139_device_t* priv = dev->priv;
    if (dev->initialized) {
        log_msg("rtl8139_pci_attach\n");
        priv->net_if_eth.net_if_dev.ifstate = IF_STATE_INIT;
        net_if_attach_if(&priv->net_if_eth.net_if_dev);
    }else {
        log_msg("Device is not initialized, skip attach\n");
    }

    return 0;
}

struct pcidriver rtl8139_pci_driver = {
    .driver.name = "Realtek RTL-8139 PCI Fast Ethernet Adapter",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = rtl8139_pci_probe,
    .driver.attach = rtl8139_pci_attach,
    .idents = (void *) &rtl8139_pci_idents,
};

REGISTER_DRIVER(rtl8139driver,"rtl8139",&rtl8139_pci_driver);

