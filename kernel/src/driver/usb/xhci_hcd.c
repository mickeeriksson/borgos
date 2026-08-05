#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/usb/usb.h"
#include "driver/usb/usb_std.h"
#include "error.h"
#include "cpu/mmio.h"
#include "cpu/io.h"
#include "cpu/cpu_barrier.h"
#include "mm.h"
#include "bits.h"
#include "irq.h"
#include "delay.h"
#include <string.h>


// Compile-time bounds. All driver state is static
// so these cap what the driver will use even if the controller offers more.
#define XHCI_MAX_SLOTS_SUPPORTED    32
#define XHCI_MAX_PROTOCOLS          4
#define XHCI_MAX_ROOT_HUBS          XHCI_MAX_PROTOCOLS
#define XHCI_MAX_PORTS_SUPPORTED    64

// Root hub addresses are synthetic. USB device addresses are 7 bits (1-127),
// so anything at or above 0x80 cannot collide with an address the controller
// assigns to a real device. These values never travel on the bus.
#define XHCI_HUB_ADDR_BASE          0xF0


// ===== Capability registers ========================================
// xHCI 1.2 table 5-9, section 5.3. Offsets from the MMIO base.
#define XHCI_CAP_CAPLENGTH          0x00   // 8-bit
#define XHCI_CAP_HCIVERSION         0x02   // 16-bit
#define XHCI_CAP_HCSPARAMS1         0x04
#define XHCI_CAP_HCSPARAMS2         0x08
#define XHCI_CAP_HCSPARAMS3         0x0C
#define XHCI_CAP_HCCPARAMS1         0x10
#define XHCI_CAP_DBOFF              0x14
#define XHCI_CAP_RTSOFF             0x18
#define XHCI_CAP_HCCPARAMS2         0x1C

// HCSPARAMS1 - xHCI 1.2 table 5-10.
#define XHCI_HCSP1_MAXSLOTS(x)      ((uint32_t)((x) & 0xFFu))
#define XHCI_HCSP1_MAXINTRS(x)      ((uint32_t)(((x) >> 8) & 0x7FFu))
#define XHCI_HCSP1_MAXPORTS(x)      ((uint32_t)(((x) >> 24) & 0xFFu))

// HCSPARAMS2 - xHCI 1.2 table 5-11. The scratchpad buffer count is split
// across two fields: high order 5 bits at 25:21, low order 5 bits at 31:27.
#define XHCI_HCSP2_ERST_MAX(x)      ((uint32_t)(((x) >> 4) & 0x0Fu))
#define XHCI_HCSP2_SPB_HI(x)        ((uint32_t)(((x) >> 21) & 0x1Fu))
#define XHCI_HCSP2_SPB_LO(x)        ((uint32_t)(((x) >> 27) & 0x1Fu))
#define XHCI_HCSP2_MAX_SCRATCHPAD(x)  ((XHCI_HCSP2_SPB_HI(x) << 5) | XHCI_HCSP2_SPB_LO(x))

// HCCPARAMS1 - xHCI 1.2 table 5-13. xECP is a dword offset from the MMIO
// base, not a byte offset (section 5.3.6, worked example).
#define XHCI_HCCP1_AC64             (1u << 0)   // 64-bit addressing capable
#define XHCI_HCCP1_CSZ              (1u << 2)   // 64-byte contexts if set
#define XHCI_HCCP1_PPC              (1u << 3)   // port power control present
//#define XHCI_HCCP1_XECP(x)          ((uint32_t)(((x) >> 16) & 0xFFFFu))

// ===== Operational registers =======================================
// xHCI 1.2 table 5-18. Offsets from the operational base, which is
// mmio_base + CAPLENGTH.
#define XHCI_OP_USBCMD              0x00
#define XHCI_OP_USBSTS              0x04
#define XHCI_OP_PAGESIZE            0x08
#define XHCI_OP_DNCTRL              0x14
#define XHCI_OP_CRCR                0x18   // 64-bit
#define XHCI_OP_DCBAAP              0x30   // 64-bit
#define XHCI_OP_CONFIG              0x38
#define XHCI_OP_PORT_BASE           0x400  // port register sets start here

// Port register set - xHCI 1.2 table 5-19. 16 bytes per port, port numbers
// are 1-based.
#define XHCI_PORT_SET_SIZE          0x10u
#define XHCI_PORT_PORTSC            0x00u
#define XHCI_PORT_PORTPMSC          0x04u
#define XHCI_PORT_PORTLI            0x08u
#define XHCI_PORT_PORTHLPMC         0x0Cu


// USBCMD - xHCI 1.2 table 5-20.
#define XHCI_CMD_RS                 (1 << 0)   // Run/Stop
#define XHCI_CMD_HCRST              (1 << 1)   // Host Controller Reset
#define XHCI_CMD_INTE               (1 << 2)   // Interrupter Enable
#define XHCI_CMD_HSEE               (1 << 3)   // Host System Error Enable

// USBSTS - xHCI 1.2 table 5-21.
#define XHCI_STS_HCH                (1 << 0)   // HCHalted, RO
#define XHCI_STS_HSE                (1 << 2)   // Host System Error, RW1C
#define XHCI_STS_EINT               (1 << 3)   // Event Interrupt, RW1C
#define XHCI_STS_PCD                (1 << 4)   // Port Change Detect, RW1C
#define XHCI_STS_CNR                (1 << 11)  // Controller Not Ready, RO
#define XHCI_STS_HCE                (1 << 12)  // Host Controller Error, RO

// Every RW1C bit in USBSTS. Used to build a write mask that does not clear
// status bits accidentally - writing a 1 to any of these clears it.
#define XHCI_STS_RW1C_MASK          (XHCI_STS_HSE | XHCI_STS_EINT | XHCI_STS_PCD)

// CRCR - xHCI 1.2 table 5-24.
#define XHCI_CRCR_RCS               (1 << 0) // Ring Cycle State
//#define XHCI_CRCR_CS                (1ull << 1) // Command Stop, RW1S
//#define XHCI_CRCR_CA                (1ull << 2) // Command Abort, RW1S
//#define XHCI_CRCR_CRR               (1ull << 3) // Command Ring Running, RO
//#define XHCI_CRCR_PTR_MASK          0xFFFFFFFFFFFFFFC0ull

// CONFIG - xHCI 1.2 table 5-26.
#define XHCI_CONFIG_MAXSLOTSEN_MASK 0xFFu

// PORTSC - xHCI 1.2 table 5-27.
#define XHCI_PORTSC_CCS             (1 << 0)   // Current Connect Status, ROS
#define XHCI_PORTSC_PED             (1 << 1)   // Port Enabled, RW1CS
#define XHCI_PORTSC_OCA             (1 << 3)   // Over-current Active, RO
#define XHCI_PORTSC_PR              (1 << 4)   // Port Reset, RW1S
#define XHCI_PORTSC_PLS_SHIFT       5           // Port Link State, RWS
#define XHCI_PORTSC_PLS_MASK        (0x0Fu << 5)
#define XHCI_PORTSC_PP              (1 << 9)   // Port Power, RWS
#define XHCI_PORTSC_SPEED_SHIFT     10          // Port Speed, ROS
#define XHCI_PORTSC_SPEED_MASK      (0x0Fu << 10)
#define XHCI_PORTSC_PIC_MASK        (0x03u << 14)
#define XHCI_PORTSC_LWS             (1 << 16)  // Link State Write Strobe
#define XHCI_PORTSC_CSC             (1 << 17)  // Connect Status Change
#define XHCI_PORTSC_PEC             (1 << 18)  // Port Enabled Change
#define XHCI_PORTSC_WRC             (1 << 19)  // Warm Port Reset Change
#define XHCI_PORTSC_OCC             (1 << 20)  // Over-current Change
#define XHCI_PORTSC_PRC             (1 << 21)  // Port Reset Change
#define XHCI_PORTSC_PLC             (1 << 22)  // Port Link State Change
#define XHCI_PORTSC_CEC             (1 << 23)  // Port Config Error Change
#define XHCI_PORTSC_CAS             (1 << 24)  // Cold Attach Status, RO
#define XHCI_PORTSC_WCE             (1 << 25)
#define XHCI_PORTSC_WDE             (1 << 26)
#define XHCI_PORTSC_WOE             (1 << 27)
#define XHCI_PORTSC_DR              (1 << 30)  // Device Removable, RO
#define XHCI_PORTSC_WPR             (1 << 31)  // Warm Port Reset, RW1S

// All the write-1-to-clear change bits in one mask.
#define XHCI_PORTSC_CHANGE_MASK     (XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | \
                                     XHCI_PORTSC_WRC | XHCI_PORTSC_OCC | \
                                     XHCI_PORTSC_PRC | XHCI_PORTSC_PLC | \
                                     XHCI_PORTSC_CEC)

// THE PORTSC TRAP. Writing a 1 to any of these bits performs an action:
// PED disables the port, PR and WPR start a reset, and each change bit
// clears itself. A naive read-modify-write of PORTSC therefore disables the
// port and destroys pending change notifications as a side effect. Every
// write to PORTSC in this driver goes through xhci_portsc_write(), which
// masks these out unless the caller explicitly asks for them.
#define XHCI_PORTSC_RW1_MASK        (XHCI_PORTSC_PED | XHCI_PORTSC_PR |  XHCI_PORTSC_WPR | XHCI_PORTSC_CHANGE_MASK)


// Port Speed field values - xHCI 1.2 table 7-13, "Default USB Speed ID
// Mapping". Valid only when the port's Supported Protocol capability
// declares PSIC == 0, which is the common case and the only one handled here.
#define XHCI_SPEED_FULL             1u
#define XHCI_SPEED_LOW              2u
#define XHCI_SPEED_HIGH             3u
#define XHCI_SPEED_SUPER            4u
#define XHCI_SPEED_SUPER_PLUS       5u   // and 6, 7 for Gen1x2 / Gen2x2

// ===== Runtime registers ===========================================
// xHCI 1.2 table 5-35 and 5-37. Offsets from the runtime base, which is
// mmio_base + RTSOFF.

//#define XHCI_RT_MFINDEX             0x00u
#define XHCI_RT_IR0                 0x20   // interrupter register sets
//#define XHCI_RT_IR_SIZE             0x20u   // 32 bytes per interrupter

#define XHCI_IR_IMAN                0x00u
//#define XHCI_IR_IMOD                0x04u
#define XHCI_IR_ERSTSZ              0x08
#define XHCI_IR_ERSTBA              0x10   // 64-bit
#define XHCI_IR_ERDP                0x18   // 64-bit

// IMAN - xHCI 1.2 table 5-38.
#define XHCI_IMAN_IP                (1 << 0)   // Interrupt Pending, RW1C
#define XHCI_IMAN_IE                (1 << 1)   // Interrupt Enable, RW

// ERDP - xHCI 1.2 table 5-42.
#define XHCI_ERDP_DESI_MASK         0x07ull     // Dequeue ERST Segment Index
#define XHCI_ERDP_EHB               (1ull << 3) // Event Handler Busy, RW1C
#define XHCI_ERDP_PTR_MASK          0xFFFFFFFFFFFFFFF0ull

// ===== Doorbell registers ==========================================
// xHCI 1.2 section 5.6, table 5-43. One 32-bit register per slot at
// mmio_base + DBOFF; index 0 is the command ring doorbell.

#define XHCI_DB_TARGET_MASK         0xFF
#define XHCI_DB_STREAM_SHIFT        16
#define XHCI_DB_CMD_TARGET          0      // command doorbell target
#define XHCI_DB_EP0_TARGET          1      // control endpoint = DCI 1


#define XHCI_EXT_CAP_LEGACY         1
#define XHCI_EXT_CAP_PROTOCOL       2

// USB Legacy Support - xHCI 1.2 section 7.1, tables 7-4 and 7-5.
#define XHCI_LEGSUP                 0x00   // USBLEGSUP, at xECP + 0
#define XHCI_LEGCTLSTS              0x04   // USBLEGCTLSTS, at xECP + 4

#define XHCI_LEGSUP_BIOS_OWNED      (1 << 16)
#define XHCI_LEGSUP_OS_OWNED        (1 << 24)

// USBLEGCTLSTS: the SMI enable bits (0, 4, 13, 14, 15) and the RW1C SMI
// status bits (29, 30, 31). Bits 16 and 20 are read-only shadows of USBSTS
// and are cleared through USBSTS, not here.
#define XHCI_LEGCTL_SMI_ENABLES     0x0000E011
#define XHCI_LEGCTL_SMI_RW1C        0xE0000000

#define USB_TRSTRCY  10             // reset recovery time, defined as 10 ms in USB2.0


// ===== Event Ring Segment Table ====================================
// xHCI 1.2 section 6.5, figure 6-40. Ring Segment Size is a 16-bit field,
// valid values 16 to 4096.

typedef struct {
    uint32_t base_lo;               // bits 5:0 are RsvdZ - 64-byte aligned
    uint32_t base_hi;
    uint32_t size;                  // bits 15:0 = TRB count
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

// ===== Device and Input Contexts ===================================
// xHCI 1.2 section 6.2. IMPORTANT: a context entry is 32 bytes, or 64 bytes
// when HCCPARAMS1.CSZ is set. The structs below describe the 32-byte form;
// all indexing into a context array must use the runtime context size held in
// g_xhci.context_size, never sizeof() on these types.

// Slot Context - xHCI 1.2 section 6.2.2.
typedef struct {
    uint32_t dw0;   // 19:0 Route String, 26 Hub, 31:27 Context Entries
    uint32_t dw1;   // 15:0 Max Exit Latency, 23:16 Root Hub Port Number,
    // 31:24 Number of Ports
    uint32_t dw2;   // 7:0 Parent Hub Slot ID, 15:8 Parent Port Number,
    // 17:16 TT Think Time
    uint32_t dw3;   // 7:0 USB Device Address, 31:27 Slot State
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_context_t;

#define XHCI_SLOT_ROUTE_MASK        0x000FFFFFu
#define XHCI_SLOT_HUB               (1u << 26)
#define XHCI_SLOT_SET_CTX_ENTRIES(n) (((uint32_t)(n) & 0x1Fu) << 27)
#define XHCI_SLOT_SET_ROOT_PORT(p)  (((uint32_t)(p) & 0xFFu) << 16)
#define XHCI_SLOT_SET_NUM_PORTS(n)  (((uint32_t)(n) & 0xFFu) << 24)
#define XHCI_SLOT_GET_ADDRESS(dw3)  ((dw3) & 0xFFu)

// Endpoint Context - xHCI 1.2 section 6.2.3.
typedef struct {
    uint32_t dw0;   // 2:0 EP State, 9:8 Mult, 15 LSA, 23:16 Interval
    uint32_t dw1;   // 2:1 CErr, 5:3 EP Type, 15:8 Max Burst Size,
    // 31:16 Max Packet Size
    uint32_t deq_lo; // bit 0 = Dequeue Cycle State (DCS)
    uint32_t deq_hi;
    uint32_t dw4;   // 15:0 Average TRB Length, 31:16 Max ESIT Payload Lo
    uint32_t reserved[3];
} __attribute__((packed)) xhci_endpoint_context_t;

#define XHCI_EP_SET_INTERVAL(i)     (((uint32_t)(i) & 0xFFu) << 16)
#define XHCI_EP_SET_CERR(c)         (((uint32_t)(c) & 0x03u) << 1)
#define XHCI_EP_SET_TYPE(t)         (((uint32_t)(t) & 0x07u) << 3)
#define XHCI_EP_SET_MAX_BURST(b)    (((uint32_t)(b) & 0xFFu) << 8)
#define XHCI_EP_SET_MAX_PACKET(m)   (((uint32_t)(m) & 0xFFFFu) << 16)
#define XHCI_EP_DCS                 (1u << 0)

// Endpoint Type values - xHCI 1.2 section 6.2.3, EP Type field.
#define XHCI_EP_TYPE_NOT_VALID      0u
#define XHCI_EP_TYPE_ISOCH_OUT      1u
#define XHCI_EP_TYPE_BULK_OUT       2u
#define XHCI_EP_TYPE_INTERRUPT_OUT  3u
#define XHCI_EP_TYPE_CONTROL        4u
#define XHCI_EP_TYPE_ISOCH_IN       5u
#define XHCI_EP_TYPE_BULK_IN        6u
#define XHCI_EP_TYPE_INTERRUPT_IN   7u

// Input Control Context - xHCI 1.2 section 6.2.5.1, figure 6-6. Drop flags
// are D2..D31 (bits 1:0 are reserved), add flags are A0..A31.
typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[5];
    uint32_t config_value;  // 7:0 Configuration Value, 15:8 Interface Number,
    // 23:16 Alternate Setting
} __attribute__((packed)) xhci_input_control_context_t;

// ===== TRBs ========================================================
// xHCI 1.2 section 6.4. A TRB is 16 bytes: two parameter dwords, one status dword, one control dword.
// The cycle bit is bit 0 of control and the TRB type is bits 15:10 (table 6-91).
typedef struct {
    uint32_t parameter_lo;
    uint32_t parameter_hi;
    uint32_t status;
    uint32_t control;

    // internal data
    //usb_xfer_request_t req;
} __attribute__((packed)) xhci_trb_t;

#define XHCI_TRB_SIZE               16
#define XHCI_TRB_CYCLE              (1 << 0)
//#define XHCI_TRB_TYPE_SHIFT         10
//#define XHCI_TRB_TYPE_MASK          (0x3Fu << 10)
#define XHCI_TRB_TYPE(ctrl)         (((ctrl) >> 10) & 0x3Fu)
#define XHCI_TRB_SET_TYPE(t)        (((uint32_t)(t) & 0x3Fu) << 10)


// TRB type codes - xHCI 1.2 table 6-91.
#define XHCI_TRB_NORMAL             1
#define XHCI_TRB_SETUP_STAGE        2
#define XHCI_TRB_DATA_STAGE         3
#define XHCI_TRB_STATUS_STAGE       4
//#define XHCI_TRB_ISOCH              5
#define XHCI_TRB_LINK               6
//#define XHCI_TRB_EVENT_DATA         7
//#define XHCI_TRB_NO_OP              8
#define XHCI_TRB_ENABLE_SLOT        9
//#define XHCI_TRB_DISABLE_SLOT       10
#define XHCI_TRB_ADDRESS_DEVICE     11
#define XHCI_TRB_CONFIG_ENDPOINT    12
//#define XHCI_TRB_EVALUATE_CONTEXT   13
//#define XHCI_TRB_RESET_ENDPOINT     14
//#define XHCI_TRB_STOP_ENDPOINT      15
//#define XHCI_TRB_SET_TR_DEQUEUE     16
//#define XHCI_TRB_RESET_DEVICE       17
#define XHCI_TRB_NO_OP_CMD          23
#define XHCI_TRB_TRANSFER_EVENT     32
#define XHCI_TRB_CMD_COMPLETION     33
#define XHCI_TRB_PORT_STATUS_CHANGE 34
//#define XHCI_TRB_BANDWIDTH_REQUEST  35
//#define XHCI_TRB_DOORBELL_EVENT     36
#define XHCI_TRB_HOST_CONTROLLER    37
//#define XHCI_TRB_DEVICE_NOTIFY      38
//#define XHCI_TRB_MFINDEX_WRAP       39

// Link TRB control bits - xHCI 1.2 figure 6-38.
#define XHCI_TRB_TC                 (1 << 1)   // Toggle Cycle

// Transfer TRB control bits - xHCI 1.2 figures 6-9, 6-10, 6-11.
#define XHCI_TRB_ENT                (1u << 1)   // Evaluate Next TRB
#define XHCI_TRB_ISP                (1u << 2)   // Interrupt on Short Packet
#define XHCI_TRB_NS                 (1u << 3)   // No Snoop
#define XHCI_TRB_CH                 (1u << 4)   // Chain
#define XHCI_TRB_IOC                (1u << 5)   // Interrupt On Completion
#define XHCI_TRB_IDT                (1u << 6)   // Immediate Data
#define XHCI_TRB_DIR_IN             (1u << 16)  // Data/Status Stage direction

// Setup Stage Transfer Type (TRT) - xHCI 1.2 table 6-26, bits 17:16.
#define XHCI_TRT_NO_DATA            0u
#define XHCI_TRT_OUT_DATA           2u
#define XHCI_TRT_IN_DATA            3u
#define XHCI_TRB_SET_TRT(t)         (((uint32_t)(t) & 0x03u) << 16)

// Address Device - xHCI 1.2 figure 6-25. BSR = Block Set Address Request.
#define XHCI_TRB_BSR                (1u << 9)
#define XHCI_TRB_SLOT_ID_SHIFT      24
#define XHCI_TRB_SET_SLOT(id)       (((uint32_t)(id) & 0xFFu) << 24)
#define XHCI_TRB_GET_SLOT(ctrl)     (((ctrl) >> 24) & 0xFFu)

// Enable Slot - xHCI 1.2 figure 6-23, Slot Type at bits 20:16.
#define XHCI_TRB_SET_SLOT_TYPE(t)   (((uint32_t)(t) & 0x1Fu) << 16)

// Event TRB field extraction - xHCI 1.2 figures 6-14, 6-15, 6-16.
#define XHCI_EVT_COMP_CODE(status)  (((status) >> 24) & 0xFFu)
#define XHCI_EVT_XFER_LEN(status)   ((status) & 0xFFFFFFu)
#define XHCI_EVT_PORT_ID(param_lo)  (((param_lo) >> 24) & 0xFFu)
#define XHCI_EVT_EP_ID(ctrl)        (((ctrl) >> 16) & 0x1Fu)


// Completion codes - xHCI 1.2 table 6-90.
#define XHCI_CC_INVALID             0u
#define XHCI_CC_SUCCESS             1u
#define XHCI_CC_DATA_BUFFER_ERROR   2u
#define XHCI_CC_BABBLE_DETECTED     3u
#define XHCI_CC_USB_TRANSACTION     4u
#define XHCI_CC_TRB_ERROR           5u
#define XHCI_CC_STALL_ERROR         6u
#define XHCI_CC_RESOURCE_ERROR      7u
#define XHCI_CC_BANDWIDTH_ERROR     8u
#define XHCI_CC_NO_SLOTS_AVAILABLE  9u
#define XHCI_CC_INVALID_STREAM      10u
#define XHCI_CC_SLOT_NOT_ENABLED    11u
#define XHCI_CC_EP_NOT_ENABLED      12u
#define XHCI_CC_SHORT_PACKET        13u
#define XHCI_CC_RING_UNDERRUN       14u
#define XHCI_CC_RING_OVERRUN        15u
#define XHCI_CC_PARAMETER_ERROR     17u
#define XHCI_CC_CONTEXT_STATE_ERROR 19u
#define XHCI_CC_CMD_RING_STOPPED    24u
#define XHCI_CC_COMMAND_ABORTED     25u
#define XHCI_CC_STOPPED             26u


// Ring geometry. A 4 KiB page holds exactly 256 TRBs of 16 bytes, and the
// last one is always a Link TRB, so a segment carries 255 usable TRBs. The
// page size drives the ring size rather than the other way round
// (ARCHITECTURE.md AD-11).
#define XHCI_RING_TRBS              (PAGESIZE/XHCI_TRB_SIZE)    //256
#define XHCI_RING_USABLE_TRBS       (XHCI_RING_TRBS - 1)

#define XHCI_MAX_SBP_PAGES      64

const struct pci_device_id xhcihcd_pci_idents[] = {
    PCI_DEVICE(0x1B36, 0x000D,"QEMU XHCI Host Controller"),
    PCI_DEVICE(0x1033, 0x0194,"NEC uPD720200 USB 3.0 Host Controller"),
    //PCI_DEVICE(0x8086, 0xA71E,"Raptor Lake-P Thunderbolt 4 USB Controller (XHCI)"),
    PCI_DEVICE(0x8086, 0x51ED,"Raptor Alder Lake PCH USB 3.2 xHCI Host Controller (XHCI)"),
    PCI_DEVICE_END,
};

// ===== Driver-visible protocol description =========================
// One entry per Supported Protocol capability, grouped by major revision.
// This is what decides how many virtual root hubs exist and which ports each
// one owns
typedef struct xhci_protocol{
    uint8_t  major;             // 2 or 3
    uint8_t  minor;
    uint8_t  slot_type;         // Protocol Slot Type for Enable Slot
    uint32_t port_offset;       // first global PORTSC number, 1-based
    uint32_t port_count;
} xhci_protocol_t;

// ROOT HUB
typedef struct {
    int      in_use;
    uint8_t  address;
    uint8_t  major;                 // 2 or 3
    uint8_t  minor;
    uint32_t port_offset;           // first GLOBAL PORTSC number
    uint32_t port_count;

    // Status change bitmap delivered on the interrupt IN endpoint. Bit 0 is
    // the hub itself, bits 1..port_count are the ports, in hub-local
    // numbering. Written from interrupt context by xhci_hub_port_changed().
    volatile uint8_t change_bitmap[(XHCI_MAX_PORTS_SUPPORTED / 8u) + 2u];
} xhci_vhub_t;

// One transfer ring per non-control endpoint, indexed by Device Context
// Index (xHCI 1.2 section 4.5.1). DCI 0 is the slot and DCI 1 is EP0, which
// keeps its own fields in xhci_device_t, so eps[0] and eps[1] are unused.
typedef struct {
    xhci_trb_t *ring_virt;
    uint32_t enqueue;
    uint8_t cycle;
    uint8_t configured;     // Configure Endpoint has completed for this DCI
    uint8_t halted;         // completed with an error; see xhcihcd_handle_transfer_event()
    // The single outstanding request. Set by the submitter, cleared by the
    // event handler before the completion callback runs, so the callback may
    // resubmit the same request.
    struct usb_xfer_request *volatile req;
} xhci_endpoint_t;


#define XHCI_MAX_DCI 31

typedef struct {
    int          in_use;
    uint8_t      slot_id;
    uint8_t      address;            // USB address assigned by the xHC
    usb_speed_t  speed;
    uint32_t     root_port;          // global PORTSC number

    //page_t     *dev_ctx_pages;
    void*        dev_ctx_virt;
    //uint64_t    dev_ctx_phys;

    //page_t     *in_ctx_pages;
    void*        in_ctx_virt;
    //uint64_t    in_ctx_phys;

    //page_t     *ep0_ring_pages;
    xhci_trb_t*  ep0_ring_virt;
    //uint64_t    ep0_ring_phys;
    uint32_t     ep0_enqueue;
    uint8_t      ep0_cycle;

    xhci_endpoint_t* eps[XHCI_MAX_DCI + 1];
    //xhci_endpoint_t *eps;

    // Completion state for the one outstanding transfer this draft allows
    // per device. Written by xhci_irq(), read by the waiting caller.
    volatile int      xfer_pending;
    volatile uint32_t xfer_code;
    volatile uint32_t xfer_residual;
} xhci_device_t;


typedef struct xhci_hcd_device {
    usb_hcd_t  usb_hcd;   //must be first, this way a pointer to usb_hcd is also pointer to usb_hcd...
    adr_t mmio_regbase;
    adr_t io_regbase;
    //uint32_t *stackframe;  //Frame List: 1024 x 32-bit pointers, 4096-byte aligned.
    //adr_t queueframe;       //adress to block of allocated qh's
    //uhci_qh_t* queue[QSLOTS];   // the different queues.
    //uhci_qh_t* curTransfer;   //change this to a linked list...

    uint32_t xECPoff;   // xECP offsett from mmio_regbase

    adr_t reg_op_mmiobase; //operational registers
    adr_t reg_rt_mmiobase; //runtime registers
    adr_t reg_db_mmiobase; //doorbell registers


    // Cached capability values, read once in xhci_start()
    //uint16_t hci_version;           // BCD, e.g. 0x0100 for xHCI 1.0
    //uint32_t max_slots;
    //uint32_t max_intrs;
    //uint32_t max_ports;
    //uint32_t slots_enabled;
    uint32_t context_size;          // 32 or 64, from HCCPARAMS1.CSZ
    //uint32_t page_size;             // from the PAGESIZE register
    //uint32_t xecp;                  // byte offset of first extended capability
    //int      ac64;

    // Supported Protocol capabilities, grouped by major revision (AD-8)
    xhci_protocol_t protocols[XHCI_MAX_PROTOCOLS];
    uint32_t protocol_count;


    uint64_t* dcbapp_virt;      //Device Context Base Adress
    //void* dcbapp;

    //COMMND RING
    adr_t    cr_virt;          //Command Ring
    uint32_t cmd_enqueue;      //Current ring write position
    uint8_t  cmd_cycle;        //Current cycle bit

    //EVENT RING
    adr_t    er_virt;          //Event Ring
    uint32_t event_dequeue;
    uint8_t  event_cycle;        //Current cycle bit

    //EVENT RING SEGMENT TABLES
    adr_t    erst_virt;          //Command Ring

    //SCRATCHPADS
    adr_t    scratchpadarr_virt;          //Command Ring
    void*    spb_pages[XHCI_MAX_SBP_PAGES];  //virt adress till varje allokerad scratchpad buffer


    // Outstanding command. Only one at a time in this draft - the command
    // ring is serialised by xhci_command_execute().
    volatile int      cmd_pending;
    volatile uint64_t cmd_trb_phys;
    volatile uint32_t cmd_code;
    volatile uint32_t cmd_slot_id;


    //HUBS
    xhci_vhub_t hubs[XHCI_MAX_ROOT_HUBS];
    uint8_t hubcount;       // nr of roothubs

    // Devices
    xhci_device_t* devices[XHCI_MAX_SLOTS_SUPPORTED + 1]; // Devices, indexed by slot ID (1-based; index 0 unused)
    uint8_t address_to_slot[256];

} xhci_hcd_device_t;



xhci_device_t* getDeviceForSlot(xhci_hcd_device_t* xhcidev,uint32_t slot_id) {
    xhci_device_t* dev = xhcidev->devices[slot_id];
    //xhci_device_t* dev = NULL;

    if (dev == NULL) {
        PANIC("DeviceForSlot is NULL");
    }
    return dev;
}

extern USBRESULT xhcihcd_command_execute(usb_hcd_t *hcd,xhci_trb_t *trb, uint32_t *out_slot_id,uint32_t timeout) ;
extern void xhci_selftest_interrupt(usb_hcd_t *hcd);


// ===== Extended capabilities =======================================
// xHCI 1.2 section 7, table 7-2. The next pointer inside each capability is
// a dword offset relative to that capability's own address.
/*
#define XHCI_EXT_CAP_ID(x)          ((uint32_t)((x) & 0xFFu))
#define XHCI_EXT_CAP_NEXT(x)        ((uint32_t)(((x) >> 8) & 0xFFu))

#define XHCI_EXT_CAP_LEGACY         1u
#define XHCI_EXT_CAP_PROTOCOL       2u

// USB Legacy Support - xHCI 1.2 section 7.1, tables 7-4 and 7-5.
#define XHCI_LEGSUP                 0x00u   // USBLEGSUP, at xECP + 0
#define XHCI_LEGCTLSTS              0x04u   // USBLEGCTLSTS, at xECP + 4

#define XHCI_LEGSUP_BIOS_OWNED      (1u << 16)
#define XHCI_LEGSUP_OS_OWNED        (1u << 24)
*/

// ===== Extended capabilities =======================================
// xECP in HCCPARAMS1 and the Next pointer inside each capability are both
// counted in DWORDS, not bytes (xHCI 1.2 section 5.3.6). Getting that wrong
// lands in the middle of some other register and looks like a hardware fault.
#define XHCI_EXT_CAP_ID(x)          ((uint32_t)((x) & 0xFF))
#define XHCI_EXT_CAP_NEXT(x)        ((uint32_t)(((x) >> 8) & 0xFF))
uint32_t xhci_ext_cap_find(xhci_hcd_device_t* xhcidev,uint32_t id) {
    adr_t mmioregbase = xhcidev->mmio_regbase;
    uint32_t offset = xhcidev->xECPoff;
    for (uint32_t guard = 0; offset != 0 && guard < 256u; guard++) {
        uint32_t entry = mmio_read32(mmioregbase+offset);
        log_msg("XHCI EXT CAP entry= %#x\n",entry);
        if (XHCI_EXT_CAP_ID(entry) == id) {
            return offset;
        }
        uint32_t next = XHCI_EXT_CAP_NEXT(entry);
        if (next == 0) {
            return 0;
        }
        offset += next * 4u;
    }
    return 0;
}

// ===== BIOS handoff (AD-7) =========================================
//
// On real hardware firmware often owns the controller so it can offer USB
// keyboards in BIOS setup, and it traps the controller's interrupts as SMIs.
// Resetting the controller out from under System Management Mode hangs the
// machine, so ownership is transferred through the protocol below first.
// QEMU's qemu-xhci exposes no legacy capability at all, in which case this
// is a no-op and NOT an error.

USBRESULT xhci_disable_legacy_support(xhci_hcd_device_t* xhcidev) {
    adr_t mmioregbase = xhcidev->mmio_regbase;
    uint32_t capoff = xhci_ext_cap_find(xhcidev,XHCI_EXT_CAP_LEGACY);  // XHCI_EXT_CAP_LEGACY_SUPPORT_ID has ID =1
    if (!capoff) {
        log_msg("This XHCI device does not support legacy support, skip disable!");
        return USB_OK;
    }

    uint32_t legsup = mmio_read32(mmioregbase + capoff + XHCI_LEGSUP);
    log_msg("legsup = %#x\n",legsup);

    if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
        mmio_write32(mmioregbase + capoff + XHCI_LEGSUP, legsup | XHCI_LEGSUP_OS_OWNED);

        int timeout = 50; //100ms
        while (mmio_read32(mmioregbase + capoff + XHCI_LEGSUP) & XHCI_LEGSUP_BIOS_OWNED ) {
            if (--timeout==0) {
                log_msg("Timeout while waiting Bios to let go of legacy support\n");
                break;
            }
            mdelay(2);
        }
        legsup = mmio_read32(mmioregbase + capoff + XHCI_LEGSUP);
        if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
            // Firmware never let go. Take it anyway: refusing here would
            // leave USB unusable on a machine with broken firmware, which is
            // a worse outcome than a forced handoff. This is what Linux does.
            legsup &= ~(uint32_t)XHCI_LEGSUP_BIOS_OWNED;
            legsup |= XHCI_LEGSUP_OS_OWNED;
            mmio_write32(mmioregbase + capoff + XHCI_LEGSUP, legsup);
            log_msg("Legacy support handoff forced!");
        }
    }else {
        log_msg("XHCI LEGSUP is already disabled\n");
    }

    // Disable every SMI source and clear the latched SMI status bits. Without
    // this, SMM can still be woken by the controller we now own. The status
    // bits are RW1C, so writing them back as ones is what clears them.
    uint32_t ctl = mmio_read32(mmioregbase + capoff + XHCI_LEGCTLSTS);
    ctl &= ~(uint32_t)XHCI_LEGCTL_SMI_ENABLES;
    ctl |= XHCI_LEGCTL_SMI_RW1C;
    mmio_write32(mmioregbase + capoff + XHCI_LEGCTLSTS, ctl);


    //PANIC("TODO Implement xhci_disable_legacy_support");
    /*
    int forced = 0;
    uint32_t legsup = xhci_read32(cap + XHCI_LEGSUP);

    if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
        xhci_write32(cap + XHCI_LEGSUP, legsup | XHCI_LEGSUP_OS_OWNED);

        uint32_t spins = XHCI_SPIN_HANDOFF;
        while (spins--) {
            legsup = xhci_read32(cap + XHCI_LEGSUP);
            if (!(legsup & XHCI_LEGSUP_BIOS_OWNED)) {
                break;
            }
            xhci_relax();
        }

        if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
            // Firmware never let go. Take it anyway: refusing here would
            // leave USB unusable on a machine with broken firmware, which is
            // a worse outcome than a forced handoff. This is what Linux does.
            legsup &= ~(uint32_t)XHCI_LEGSUP_BIOS_OWNED;
            legsup |= XHCI_LEGSUP_OS_OWNED;
            xhci_write32(cap + XHCI_LEGSUP, legsup);
            forced = 1;
        }
    }

    // Disable every SMI source and clear the latched SMI status bits. Without
    // this, SMM can still be woken by the controller we now own. The status
    // bits are RW1C, so writing them back as ones is what clears them.
    uint32_t ctl = xhci_read32(cap + XHCI_LEGCTLSTS);
    ctl &= ~(uint32_t)XHCI_LEGCTL_SMI_ENABLES;
    ctl |= XHCI_LEGCTL_SMI_RW1C;
    xhci_write32(cap + XHCI_LEGCTLSTS, ctl);

    return forced ? XHCI_ERR_BIOS_HANDOFF : XHCI_OK;
    */
    return 0;
}


// Which protocol owns a global port number, or NULL.
static const xhci_protocol_t *xhci_protocol_for_port(usb_hcd_t *hcd,uint32_t port) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    for (uint32_t i = 0; i < xhcidev->protocol_count; i++) {
        const xhci_protocol_t *p = &xhcidev->protocols[i];
        if (port >= p->port_offset && port < p->port_offset + p->port_count) {
            return p;
        }
    }
    return NULL;
}


// ===== Contexts ====================================================
//
// Context entries are 32 or 64 bytes depending on HCCPARAMS1.CSZ, so every
// index into a context array goes through this helper rather than through
// pointer arithmetic on the struct type.

void *xhci_context_at(usb_hcd_t *hcd, void *base, uint32_t index) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    return (void *)((uint8_t *)base + (uint32_t)(index * xhcidev->context_size));
}

// Default control endpoint packet size by speed, xHCI 1.2 section 4.3
// ("Device Slot Assignment" / initial Max Packet Size). For full speed the
// real value is only known after reading the first 8 bytes of the device
// descriptor, so 8 is used and corrected later with Evaluate Context.
static uint32_t xhci_default_max_packet(usb_speed_t speed) {
    switch (speed) {
        case USB_SPEED_LOW:         return 8;
        case USB_SPEED_FULL:        return 8;
        case USB_SPEED_HIGH:        return 64;
        case USB_SPEED_SUPER:
        case USB_SPEED_SUPER_PLUS:  return 512;
        default:                    return 8;
    }
}

// ===== Ring helpers ================================================
//
// A ring segment is one page: 256 TRBs, of which the last is a Link TRB
// pointing back at the start with the Toggle Cycle bit set. That leaves 255
// usable slots (xHCI 1.2 section 4.9.2).

static void xhcihcd_ring_init(xhci_trb_t *ring) {
    memset(ring,0,XHCI_RING_TRBS * XHCI_TRB_SIZE);

    xhci_trb_t *link = &ring[XHCI_RING_USABLE_TRBS];
    link->parameter_lo = (uint32_t)(V2P(ring) & 0xFFFFFFFF);
    link->parameter_hi = (uint32_t)(V2P(ring) >> 32);
    link->status = 0;
    link->control = XHCI_TRB_SET_TYPE(XHCI_TRB_LINK) | XHCI_TRB_TC;
    return;
}


// ===== Device lifecycle ============================================

USBRESULT xhci_device_alloc_structures(usb_hcd_t *hcd,xhci_device_t *dev) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;

    size_t ctx_size = xhcidev->context_size*32;
    log_msg("ctx_size=%d\n",ctx_size);
    dev->dev_ctx_virt = (void*) page_alloc_pages_virt(ZONENORMAL, 0);
    if (!dev->dev_ctx_virt) {
        //return XHCI_ERR_NO_MEMORY;
        return USB_ERROR;
    }
    // The output device context must be zero before Address Device
    // (xHCI 1.2 section 4.3.3), and every RsvdZ field in the input context
    // must be zero too. Do not rely on the page allocator for either.
    memset(dev->dev_ctx_virt, 0, PAGESIZE);

    // Input context = one Input Control Context followed by the slot context
    // and up to 31 endpoint contexts, so 33 entries.
    dev->in_ctx_virt = (void*) page_alloc_pages_virt(ZONENORMAL, 0);
    if (!dev->in_ctx_virt) {
        //return XHCI_ERR_NO_MEMORY;
        return USB_ERROR;
    }
    memset(dev->in_ctx_virt, 0, PAGESIZE);

    log_msg("ep0_ring_size=%d\n",XHCI_RING_TRBS * XHCI_TRB_SIZE);
    if (XHCI_RING_TRBS * XHCI_TRB_SIZE > PAGESIZE) {
        PANIC("ep0 ring > 4096");
    }

    dev->ep0_ring_virt = (xhci_trb_t *) page_alloc_pages_virt(ZONENORMAL, 0);
    if (!dev->ep0_ring_virt) {
        //return XHCI_ERR_NO_MEMORY;
        return USB_ERROR;
    }
    xhcihcd_ring_init(dev->ep0_ring_virt) ;
    dev->ep0_enqueue = 0;
    dev->ep0_cycle = 1;

    return USB_OK;
}


// Enable Slot, build the input context for the default control endpoint,
// then Address Device. This is the path from "a device is connected to a
// root hub port" to "the device has a USB address and endpoint 0 works".
USBRESULT xhcihcd_device_attach_internal(usb_hcd_t *hcd,uint32_t root_port, usb_speed_t speed,uint8_t *out_addr) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    //Port Speed i PORTSC är bara giltig efter att reset är klart. Läser du den innan får du skräp eller ett stale-värde från föregående state. Det är detta värde du använder för att välja MaxPacketSize0-gissning (8/64/64/512) i din Input Context.


    const xhci_protocol_t *proto = xhci_protocol_for_port(hcd,root_port);
    uint8_t slot_type = proto ? proto->slot_type : 0;
    log_msg("Slot type= %#x\n",slot_type);

    xhci_trb_t cmd;
    memset(&cmd,0,sizeof(xhci_trb_t));
    cmd.control = XHCI_TRB_SET_TYPE(XHCI_TRB_ENABLE_SLOT) | XHCI_TRB_SET_SLOT_TYPE(slot_type);


    uint32_t slot_id=0;
    USBRESULT res = xhcihcd_command_execute(hcd,&cmd, &slot_id,1000);
    log_msg("res=%d  slot_id=%d\n",res,slot_id);

    if (slot_id == 0 || slot_id > XHCI_MAX_SLOTS_SUPPORTED) {
        //return XHCI_ERR_NO_SLOTS;
        return USB_ERROR;
    }

    if (res != USB_OK) {
        return USB_ERROR;
    }

    if (xhcidev->devices[slot_id] != NULL) {
        PANIC("xhcidev->devices[slot_id] already has a device");
    }

    xhci_device_t *dev = kmalloc(sizeof(xhci_device_t),0);
    memset(dev,0,sizeof(xhci_device_t));
    xhcidev->devices[slot_id] = dev;

    dev->slot_id = (uint8_t)slot_id;
    dev->speed = speed;
    dev->root_port = root_port;

    res = xhci_device_alloc_structures(hcd,dev);
    if (res != USB_OK) {
        return res;
    }

    // Input Control Context: add the slot context (A0) and the control
    // endpoint (A1). Nothing is dropped.
    xhci_input_control_context_t *icc = (xhci_input_control_context_t *)dev->in_ctx_virt;
    icc->drop_flags = 0;
    icc->add_flags = (1 << 0) | (1 << 1);

    // Slot context is entry 1 of the input context.
    xhci_slot_context_t *slot = (xhci_slot_context_t *) xhci_context_at(hcd,dev->in_ctx_virt, 1);
    // Route string 0: the device hangs directly off a root hub port.
    // Context Entries = 1 means the control endpoint is the last valid one.
    slot->dw0 = XHCI_SLOT_SET_CTX_ENTRIES(1);
    // THE global port number goes here, not a hub-local one (AD-8).
    slot->dw1 = XHCI_SLOT_SET_ROOT_PORT(root_port);
    slot->dw2 = 0;
    slot->dw3 = 0;

    // Control endpoint context is entry 2 (DCI 1 plus the control context).
    xhci_endpoint_context_t *ep0 = (xhci_endpoint_context_t *)xhci_context_at(hcd,dev->in_ctx_virt, 2);
    ep0->dw0 = 0;
    ep0->dw1 = XHCI_EP_SET_TYPE(XHCI_EP_TYPE_CONTROL) | XHCI_EP_SET_CERR(3u) | XHCI_EP_SET_MAX_PACKET(xhci_default_max_packet(speed));
    ep0->deq_lo = (uint32_t)(V2P(dev->ep0_ring_virt) & 0xFFFFFFFFu) | XHCI_EP_DCS;
    ep0->deq_hi = (uint32_t)(V2P(dev->ep0_ring_virt) >> 32);
    // Average TRB Length is a scheduling hint; 8 is the conventional value
    // for a control endpoint (xHCI 1.2 section 6.2.3, Average TRB Length).
    ep0->dw4 = 8;

    // The device context is what the controller writes back into.
    xhcidev->dcbapp_virt[slot_id] = V2P(dev->dev_ctx_virt);
    cpu_barrier();
    memset(&cmd,0,sizeof(xhci_trb_t));
    cmd.parameter_lo = (uint32_t)( V2P(dev->in_ctx_virt) & 0xFFFFFFFFu);
    cmd.parameter_hi = (uint32_t)( V2P(dev->in_ctx_virt) >> 32);
    cmd.control = XHCI_TRB_SET_TYPE(XHCI_TRB_ADDRESS_DEVICE) | XHCI_TRB_SET_SLOT(slot_id);
    // BSR is left clear, so the controller issues SET_ADDRESS on the bus and
    // the device lands in the Addressed state in one step.
    //rc = xhci_command_execute(&cmd, NULL);
    res = xhcihcd_command_execute(hcd,&cmd, NULL,1000);
    if (res != USB_OK) {
        log_msg("Fail to send cmd\n");
        return res;
    }

    // The assigned address is readable from the output slot context.
    // The assigned address is readable from the output slot context.
    xhci_slot_context_t *out_slot = (xhci_slot_context_t *)xhci_context_at(hcd,dev->dev_ctx_virt, 0);
    dev->address = (uint8_t)XHCI_SLOT_GET_ADDRESS(out_slot->dw3);
    dev->in_use = 1;

    xhcidev->address_to_slot[dev->address] = dev->slot_id;
    if (out_addr) {
        *out_addr = dev->address;
    }

    //log_msg("--------------------\n");
    log_msg("attach internally dev->address=%d slot=%d \n",dev->address,dev->slot_id);
    //log_msg("--------------------\n");

    return USB_OK;
}


// ===== Root hub ================================================

USBRESULT xhcihcd_hub_init(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;

    xhcidev->hubcount = 0;

    for (uint32_t i = 0; i < xhcidev->protocol_count; i++) {
        //const xhci_protocol_t *proto = xhci_get_protocol(i);
        const xhci_protocol_t *proto = &xhcidev->protocols[i];

        if (!proto || proto->port_count == 0) {
            log_msg("proto is NULL || port_count == 0");
            continue;
        }

        // Group by major revision: a controller may declare several
        // capability instances with the same major revision for different
        // port ranges, and those belong to the same hub. xhci.c already
        // merges adjacent ranges, so this catches the non-adjacent case.
        xhci_vhub_t *hub = NULL;
        for (uint32_t j = 0; j < xhcidev->hubcount; j++) {
            if (xhcidev->hubs[j].major == proto->major) {
                hub = &xhcidev->hubs[j];
                break;
            }
        }

        if (!hub) {
            if (xhcidev->hubcount >= XHCI_MAX_ROOT_HUBS) {
                // More protocol revisions than this driver has hub slots.
                // Better to report it than to silently drop the ports.
                //return XHCI_ERR_UNSUPPORTED;
                return USB_ERROR;
            }
            hub = &xhcidev->hubs[xhcidev->hubcount];
            hub->in_use = 1;
            hub->address = (uint8_t)(XHCI_HUB_ADDR_BASE + xhcidev->hubcount);
            hub->major = proto->major;
            hub->minor = proto->minor;
            hub->port_offset = proto->port_offset;
            hub->port_count = proto->port_count;
            xhcidev->hubcount++;
        } else {
            // Extend the existing hub to cover this range too.
            uint32_t lo = (proto->port_offset < hub->port_offset)
                          ? proto->port_offset : hub->port_offset;
            uint32_t hi_a = hub->port_offset + hub->port_count;
            uint32_t hi_b = proto->port_offset + proto->port_count;
            uint32_t hi = (hi_a > hi_b) ? hi_a : hi_b;
            hub->port_offset = lo;
            hub->port_count = hi - lo;
        }
    }

    if (xhcidev->hubcount == 0) {
        // At least one Supported Protocol capability is mandatory
        // (xHCI 1.2 section 7.2), so finding none means the extended
        // capability walk went wrong, not that the controller has no ports.
        //return XHCI_ERR_NO_DEVICE;
        return USB_ERROR;
    }

    //list hubs
    for (int i = 0; i < xhcidev->hubcount; i++) {
        xhci_vhub_t* hub = &xhcidev->hubs[i];
        log_msg("FOUND HUB %d\n", i);
        log_msg("  inuse=%d\n", hub->in_use);
        log_msg("  address=%#x\n", hub->address);
        log_msg("  major=%d\n", hub->major);
        log_msg("  minor=%d\n", hub->minor);
        log_msg("  port_offset=%d\n", hub->port_offset);
        log_msg("  port_count=%d\n", hub->port_count);
    }

    return USB_OK;
}

uint32_t xhcihcd_port_read_status(usb_hcd_t *hcd,uint32_t port) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t portbase =  XHCI_OP_PORT_BASE + ((port-1) * XHCI_PORT_SET_SIZE);
    return mmio_read32(reg_op_mmiobase + portbase + XHCI_PORT_PORTSC);

}

// The single place PORTSC is written.
//
// PORTSC mixes ordinary read/write bits with bits where writing a 1 performs
// an action: PED disables the port, PR and WPR start a reset, and every
// change bit is write-1-to-clear. A plain read-modify-write therefore
// disables the port and silently discards pending change notifications.
// This function reads, masks every such bit out, applies the caller's
// ordinary bits in `set_bits`, and then adds back exactly the action bits
// the caller asked for in `rw1_bits`.
int xhcihcd_port_write_status(usb_hcd_t *hcd,uint32_t port, uint32_t set_bits,uint32_t rw1_bits) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t portbase =  XHCI_OP_PORT_BASE + ((port-1) * XHCI_PORT_SET_SIZE);
    uint32_t value = mmio_read32(reg_op_mmiobase + portbase + XHCI_PORT_PORTSC);

    value &= ~(uint32_t)XHCI_PORTSC_RW1_MASK;
    value |= set_bits & ~(uint32_t)XHCI_PORTSC_RW1_MASK;
    value |= rw1_bits & XHCI_PORTSC_RW1_MASK;

    mmio_write32(reg_op_mmiobase + portbase + XHCI_PORT_PORTSC,value);
    return USB_OK;
}


USBRESULT xhcihcd_port_reset(usb_hcd_t *hcd,uint32_t port) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t portbase =  XHCI_OP_PORT_BASE + ((port-1) * XHCI_PORT_SET_SIZE);

    xhcihcd_port_write_status(hcd,port, XHCI_PORTSC_PR,0);

    int timeout = 20;
    while ( (mmio_read32(reg_op_mmiobase + portbase + XHCI_PORT_PORTSC) & XHCI_PORTSC_PR) == 1) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for port reset\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("port reset done\n");
    return USB_OK;
}

usb_speed_t xhcihcd_port_speed(usb_hcd_t *hcd,uint32_t port) {
    uint32_t portsc = xhcihcd_port_read_status(hcd,port);
    uint32_t speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;

    // These are the default speed IDs from xHCI 1.2 table 7-13, valid only
    // when the owning Supported Protocol capability declares PSIC == 0.
    // TODO: a controller that publishes PSI dwords defines its own speed ID
    // mapping and this table does not apply - parse the PSI dwords instead.
    switch (speed) {
        case XHCI_SPEED_FULL:       return USB_SPEED_FULL;
        case XHCI_SPEED_LOW:        return USB_SPEED_LOW;
        case XHCI_SPEED_HIGH:       return USB_SPEED_HIGH;
        case XHCI_SPEED_SUPER:      return USB_SPEED_SUPER;
        case 5u:
        case 6u:
        case 7u:                    return USB_SPEED_SUPER_PLUS;
        default:                    return USB_SPEED_UNKNOWN;
    }
}

USBRESULT xhcihcd_hub_enumerate(usb_hcd_t *hcd,xhci_vhub_t *hub) {
    log_msg("Enumerate HUB %d.%d\n", hub->major, hub->minor);

    for (int i = 0; i < hub->port_count; i++) {
        uint8_t port = hub->port_offset+i;
        uint32_t status = xhcihcd_port_read_status(hcd,port);
        //log_msg(" status port %d = %#x\n", port, status);


        //log_msg("    CCS = %d\n", bits_32_get(status,0,0));
        //log_msg("    CSC = %d\n", bits_32_get(status,17,17));
        //log_msg("    PED = %d\n", bits_32_get(status,1,1));
        //log_msg("    PRC = %d\n", bits_32_get(status,21,21));

        uint32_t ccs = bits_32_get(status,0,0); //CCS Current Connect Status
        if (ccs) {
            //reset port
            xhcihcd_port_reset(hcd,port);
            uint32_t status = xhcihcd_port_read_status(hcd,port);
            //log_msg(" status port %d = %#x\n", port, status);
            //log_msg("    CCS = %d\n", bits_32_get(status,0,0));
            //log_msg("    CSC = %d\n", bits_32_get(status,17,17));
            //log_msg("    PED = %d\n", bits_32_get(status,1,1));
            //log_msg("    PRC = %d\n", bits_32_get(status,21,21));

            //read ccs again after reset
            ccs = bits_32_get(status,0,0); //CCS Current Connect Status
            xhcihcd_port_write_status(hcd,port,  0,XHCI_PORTSC_CHANGE_MASK);
            if (ccs) {
                log_msg("Port device is present, configure and attach it\n");
                //uint8_t portspeed = bits_32_get(status,10,13); //CCS Current Connect Status
                usb_speed_t portspeed = xhcihcd_port_speed(hcd,port);
                log_msg("portspeed = %d\n", portspeed);
                uint8_t out_addr = 0;
                //usb_speed_t speed = USB_SPEED_UNKNOWN;
                USBRESULT res = xhcihcd_device_attach_internal(hcd,port, portspeed,&out_addr);
                log_msg("res = %d\n", res);
                if (res!=USB_OK) {
                    PANIC("Failed to attach device internal");
                }
                log_msg("USB device address=%d is atteched internally\n", out_addr);


                usb_device_t* devx = usb_alloc_device();
                devx->usb_hcd = (struct usb_hcd *) hcd;
                devx->address = out_addr;
                devx->speed = portspeed;
                usb_attach_device(devx);
            }
        }

    }

    return USB_OK;
}



// Write one TRB into a ring at the producer position and advance it, handling the Link TRB and the cycle flip.
// The caller has already filled in everything except the cycle bit,
// which is written last so the controller never sees a half-built TRB (AD-11 ordering rule).
static void xhcihcd_ring_enqueue(xhci_trb_t *ring,uint32_t *enqueue_slot,uint8_t *cyclebit,const xhci_trb_t *trb) {
    xhci_trb_t *slot = &ring[*enqueue_slot];
    slot->parameter_lo = trb->parameter_lo;
    slot->parameter_hi = trb->parameter_hi;
    slot->status = trb->status;

    uint32_t control = trb->control & ~(uint32_t)XHCI_TRB_CYCLE;
    if (*cyclebit) {
        control |= XHCI_TRB_CYCLE;
    }

    cpu_barrier();
    slot->control = control;
    cpu_barrier();

    (*enqueue_slot)++;
    if (*enqueue_slot >= XHCI_RING_USABLE_TRBS) {
        // Hand the Link TRB to the controller with the current cycle, then
        // wrap and flip. The Link TRB's Toggle Cycle bit tells the controller
        // to do the same on its side (xHCI 1.2 section 4.9.2.2).
        //
        // The Chain bit is left clear: every TD this driver builds is a
        // single TRB (control transfers are three separate TDs, section
        // 4.11.2.2), so the Link TRB never sits inside a TD. If multi-TRB
        // TDs are ever added, CH must be copied from the TRB before the link.
        xhci_trb_t *link = &ring[XHCI_RING_USABLE_TRBS];
        uint32_t link_ctrl = XHCI_TRB_SET_TYPE(XHCI_TRB_LINK) | XHCI_TRB_TC;
        if (*cyclebit) {
            link_ctrl |= XHCI_TRB_CYCLE;
        }
        cpu_barrier();
        link->control = link_ctrl;
        cpu_barrier();

        *enqueue_slot = 0;
        *cyclebit = (uint8_t)(*cyclebit ? 0 : 1);
    }
}

static void xhcihcd_doorbell(usb_hcd_t *hcd,uint32_t slot, uint32_t target) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_db_mmiobase = xhcidev->reg_db_mmiobase;

    mmio_write32(reg_db_mmiobase + (slot * 4), target & XHCI_DB_TARGET_MASK);
}

USBRESULT xhcihcd_halt(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    //Make sure Run/stop bit is clear (STOP)
    uint32_t cmd = mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD);
    cmd &= ~(uint32_t)XHCI_CMD_RS;
    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, cmd);

    //wait for xhci to set halted bit
    int timeout = 10;
    while ( !(mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_STS_HCH)) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for xhci halt\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("xhci halted!\n");

    return USB_OK;
}

USBRESULT xhcihcd_reset(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    if (xhcihcd_halt(hcd) != USB_OK) {
        return USB_ERROR;
    }

    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, XHCI_CMD_HCRST);
    //wait for xhci to clear both reset bit && controillerNotReady bit

    // HCRST self-clears when the reset completes, and CNR stays set until the
    // controller is ready to accept register writes again. Both must be
    // waited on; checking only one is a common source of "the first write
    // after reset vanished".

    int timeout = 50; //100ms
    while (
            (mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_CMD_HCRST) &&
            (mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_STS_CNR) ) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for xhci reset\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("xhci reset OK!\n");

    mdelay(USB_TRSTRCY);

    //check defaults in some reg.

    if (mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD) != 0) {
        PANIC("xhci CMD reg should read back as 0 after reset!\n");
    }
    if (mmio_read32(reg_op_mmiobase + XHCI_OP_DNCTRL) != 0) {
        PANIC("xhci DNCTRL reg should read back as 0 after reset!\n");
    }
    if (mmio_read32(reg_op_mmiobase + XHCI_OP_CONFIG) != 0) {
        PANIC("xhci CONFIG reg should read back as 0 after reset!\n");
    }
    if (mmio_read64(reg_op_mmiobase + XHCI_OP_CRCR) != 0) {
        PANIC("xhci CRCR reg should read back as 0 after reset!\n");
    }
    if (mmio_read64(reg_op_mmiobase + XHCI_OP_DCBAAP) != 0) {
        PANIC("xhci DCBAAP reg should read back as 0 after reset!\n");
    }
    return USB_OK;
}

// ===== Supported Protocol parsing (AD-8) ===========================
//
// Group by major revision, not per capability instance: a controller may
// declare several instances with the same major revision covering different
// port ranges. The number of virtual root hubs follows from the number of
// distinct major revisions found.

// Supported Protocol - xHCI 1.2 section 7.2, tables 7-6 to 7-9.
#define XHCI_PROTO_MINOR(x)         ((uint32_t)(((x) >> 16) & 0xFFu))
#define XHCI_PROTO_MAJOR(x)         ((uint32_t)(((x) >> 24) & 0xFFu))
#define XHCI_PROTO_PORT_OFFSET(x)   ((uint32_t)((x) & 0xFFu))
#define XHCI_PROTO_PORT_COUNT(x)    ((uint32_t)(((x) >> 8) & 0xFFu))
#define XHCI_PROTO_PSIC(x)          ((uint32_t)(((x) >> 28) & 0x0Fu))
#define XHCI_PROTO_SLOT_TYPE(x)     ((uint32_t)((x) & 0x1Fu))

#define XHCI_EXT_CAP_PROTOCOL_ID         0x02
void xhcihcd_parse_protocols(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t mmioregbase = xhcidev->mmio_regbase;
    //adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    xhcidev->protocol_count = 0;


    uint32_t offset = xhcidev->xECPoff;
    for (uint32_t guard = 0; offset != 0 && guard < 256u; guard++) {
        //uint32_t dw0 = xhci_read32(offset);

        uint32_t capentry = mmio_read32(mmioregbase+offset);
        log_msg("XHCI EXT CAP entry= %#x\n",capentry);

        if (XHCI_EXT_CAP_ID(capentry) == XHCI_EXT_CAP_PROTOCOL_ID) {
            uint32_t dw0 = capentry;
            uint32_t dw2 = mmio_read32(mmioregbase+offset + 8);
            uint32_t dw3 = mmio_read32(mmioregbase+offset + 12);

            uint8_t  major = (uint8_t)XHCI_PROTO_MAJOR(dw0);
            uint8_t  minor = (uint8_t)XHCI_PROTO_MINOR(dw0);
            uint32_t port_offset = XHCI_PROTO_PORT_OFFSET(dw2);
            uint32_t port_count = XHCI_PROTO_PORT_COUNT(dw2);
            uint8_t  slot_type = (uint8_t)XHCI_PROTO_SLOT_TYPE(dw3);

            log_msg("Protocol %d.%d  offsett=%d count=%d slottyype=%d\n",major,minor,port_offset,port_count,slot_type);


            if (port_count != 0) {
                // Merge into an existing group with the same major revision
                // if the ranges are adjacent; otherwise start a new entry.
                int merged = 0;
                for (uint32_t i = 0; i < xhcidev->protocol_count; i++) {
                    xhci_protocol_t *p = &xhcidev->protocols[i];
                    if (p->major != major) {
                        continue;
                    }
                    if (port_offset == p->port_offset + p->port_count) {
                        p->port_count += port_count;
                        merged = 1;
                        break;
                    }
                    if (port_offset + port_count == p->port_offset) {
                        p->port_offset = port_offset;
                        p->port_count += port_count;
                        merged = 1;
                        break;
                    }
                }
                if (!merged &&
                    xhcidev->protocol_count < XHCI_MAX_PROTOCOLS) {
                    xhci_protocol_t *p =
                        &xhcidev->protocols[xhcidev->protocol_count++];
                    p->major = major;
                    p->minor = minor;
                    p->slot_type = slot_type;
                    p->port_offset = port_offset;
                    p->port_count = port_count;
                }
            }
        }

        uint32_t next = XHCI_EXT_CAP_NEXT(capentry);
        if (next == 0) {
            break;
        }
        offset += next * 4u;
    }

}

int xhcihcd_setup_scratchpad(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    uint32_t hcsp2 = mmio_read32(mmioregbase+XHCI_CAP_HCSPARAMS2);
    uint32_t maxscratchpads = XHCI_HCSP2_MAX_SCRATCHPAD(hcsp2);
    log_msg("Max Scratchpad count= %d\n",maxscratchpads);

    if (maxscratchpads == 0) {
        return USB_OK;
    }

    uint32_t pagesizebits = mmio_read32(reg_op_mmiobase + XHCI_OP_PAGESIZE);
    uint32_t pagesize = (pagesizebits & 0xFFFF)<<12;
    log_msg("  pagesize = %d (%#x)\n",pagesize,pagesize);
    if (pagesize != PAGESIZE) {
        PANIC("XHCI pagesize differs from PAGESIZE");
    }
    uint32_t max_entries_per_page = pagesize / 8;
    if (maxscratchpads>max_entries_per_page) {
        PANIC("XHCI maxscratchpad differs from PAGESIZE/8");
    }
    if (maxscratchpads>XHCI_MAX_SBP_PAGES) {
        PANIC("maxscratchpads greater than XHCI_MAX_SBP_PAGES");
    }

    //allocate scratchpad index array
    xhcidev->scratchpadarr_virt = (adr_t) page_alloc_pages_virt(ZONENORMAL, 0);
    log_msg("scratchpad index array (size=%d) allocated to 0x%lx\n",maxscratchpads,xhcidev->scratchpadarr_virt);


    for (int i = 0; i < XHCI_MAX_SBP_PAGES; i++) {
        xhcidev->spb_pages[i]=0;
    }

    uint64_t* sparr = (uint64_t*) xhcidev->scratchpadarr_virt ;

    // Each buffer is PAGESIZE bytes and PAGESIZE aligned, where PAGESIZE is
    // the controller's page size, not necessarily the host's.
    //unsigned int order = xhci_order_for(g_xhci.page_size);
    for (uint32_t i = 0; i < maxscratchpads; i++) {
        adr_t spvirt = (adr_t) page_alloc_pages_virt(ZONENORMAL, 0);  //fix this to allow scratchpads > PAGESIZE
        memset((void*)spvirt,0,pagesize);

        xhcidev->spb_pages[i]=(void*)spvirt;
        sparr[i] = V2P(spvirt);
    }
    uint64_t* dcbapp = (void*) xhcidev->dcbapp_virt;
    dcbapp[0] = (uint64_t) V2P(xhcidev->scratchpadarr_virt);

    return USB_OK;
}

int xhcihcd_setup(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t reg_rt_mmiobase = xhcidev->reg_rt_mmiobase;

    //disable bios legacy suport.
    xhci_disable_legacy_support(xhcidev);

    if (xhcihcd_halt(hcd) != USB_OK) {
        PANIC("Failed to halt xhci");
    }

    if (xhcihcd_reset(hcd) != USB_OK) {
        PANIC("Failed to reset xhci");
    }

    uint32_t hcsp1 = mmio_read32(mmioregbase+XHCI_CAP_HCSPARAMS1);
    uint32_t max_slots = XHCI_HCSP1_MAXSLOTS(hcsp1);
    uint32_t max_intrs = XHCI_HCSP1_MAXINTRS(hcsp1);
    uint32_t max_ports = XHCI_HCSP1_MAXPORTS(hcsp1);

    if (max_ports > XHCI_MAX_PORTS_SUPPORTED) {
        max_ports = XHCI_MAX_PORTS_SUPPORTED;
    }
    log_msg("XHCI HCSPARAMS1 register:\n");
    log_msg("  max_slots = %d (%#x)\n",max_slots,max_slots);
    log_msg("  max_intrs = %d (%#x)\n",max_intrs,max_intrs);
    log_msg("  max_ports = %d (%#x)\n",max_ports,max_ports);

    uint32_t pagesizebits = mmio_read32(reg_op_mmiobase + XHCI_OP_PAGESIZE);
    uint32_t pagesize = (pagesizebits & 0xFFFF)<<12;
    log_msg("  pagesize = %d (%#x)\n",pagesize,pagesize);
    if (pagesize != PAGESIZE) {
        PANIC("XHCI pagesize differs from PAGESIZE");
    }

    xhcihcd_parse_protocols(hcd);
    //list protocols
    for (int i=0; i<xhcidev->protocol_count; i++) {
        xhci_protocol_t* p = &xhcidev->protocols[i];
        log_msg("Protocol i=%d  %d.%d  offsett=%d count=%d slottype=%d\n",i,p->major,p->minor,p->port_offset,p->port_count,p->slot_type);
    }

    //Set Max SLots
    uint32_t slots_enabled = max_slots;
    if (slots_enabled > XHCI_MAX_SLOTS_SUPPORTED) {
        slots_enabled = XHCI_MAX_SLOTS_SUPPORTED;
    }
    mmio_write32(reg_op_mmiobase + XHCI_OP_CONFIG, slots_enabled & 0xFF); //What about bits 31..16 ??


    //Enable reception of device notification transaction packets. FUNCTION_WAKE
    mmio_write64(reg_op_mmiobase + XHCI_OP_DNCTRL, 1<<1 );


    //alloc DCBAAP
    // Device Context Base Address Array: one 64-bit entry per slot plus
    // entry 0 for the scratchpad array.
    size_t dcbaap_size = (max_slots + 1) * 8;
    if (dcbaap_size > PAGESIZE) {
        PANIC ("DCBAAP > PAGESIZE");
    }
    xhcidev->dcbapp_virt = (void*) page_alloc_pages_virt(ZONENORMAL, 0);
    log_msg("DCBAAP (size=%d) allocated to 0x%lx\n",dcbaap_size,xhcidev->dcbapp_virt);
    memset((void*) xhcidev->dcbapp_virt,0,dcbaap_size);

    xhcihcd_setup_scratchpad(hcd);
    mmio_write64(reg_op_mmiobase + XHCI_OP_DCBAAP,V2P(xhcidev->dcbapp_virt));

    //Setup Command Ring 4k
    log_msg("XHCI_RING_TRBS=%d\n",XHCI_RING_TRBS);
    xhcidev->cr_virt = (adr_t) page_alloc_pages_virt(ZONENORMAL, 0);
    xhcihcd_ring_init((xhci_trb_t*) xhcidev->cr_virt);
    xhcidev->cmd_enqueue = 0;
    xhcidev->cmd_cycle = 1;
    mmio_write64(reg_op_mmiobase + XHCI_OP_CRCR,V2P(xhcidev->cr_virt) | XHCI_CRCR_RCS );


    //Setup Event Ring 4k
    // Event ring and its segment table. One segment, interrupter 0 (AD-4).
    xhcidev->er_virt = (adr_t) page_alloc_pages_virt(ZONENORMAL, 0);
    // The event ring has no Link TRB: the controller wraps using the segment table, and software follows by flipping its own cycle state.
    // Zeroing the whole segment is what makes the initial cycle comparison work.
    memset((void*)xhcidev->er_virt,0, XHCI_RING_TRBS * XHCI_TRB_SIZE);
    xhcidev->event_dequeue = 0;
    xhcidev->event_cycle = 1;

    //Setup Event Ring Segment tables
    xhcidev->erst_virt = (adr_t) page_alloc_pages_virt(ZONENORMAL, 0);
    xhci_erst_entry_t *erst = (xhci_erst_entry_t*) xhcidev->erst_virt;
    erst[0].base_lo = (uint32_t)(V2P(xhcidev->er_virt) & 0xFFFFFFFFu);
    erst[0].base_hi = (uint32_t)(V2P(xhcidev->er_virt) >> 32);
    erst[0].size = XHCI_RING_TRBS;
    erst[0].reserved = 0;

    //Setup Interrupter 0 (to point to segment 0)
    log_msg("mmioregbase = #%lx\n",mmioregbase);
    log_msg("reg_rt_mmiobase = #%lx\n",reg_rt_mmiobase);
    log_msg("XHCI_RT_IR0 = #%lx\n",XHCI_RT_IR0);
    log_msg("XHCI_IR_ERSTSZ = #%lx\n",XHCI_IR_ERSTSZ);
    mmio_write32(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_ERSTSZ, 1);
    // ERDP before ERSTBA: writing ERSTBA is what arms the interrupter,
    // so the dequeue pointer must already be valid when it lands.
    mmio_write64(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_ERDP, V2P(xhcidev->er_virt));
    mmio_write64(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_ERSTBA, V2P(erst));

    // Enable interrupts on Interrupter 0
    uint32_t irman = mmio_read32(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_IMAN);
    log_msg("irman = #%x\n",irman);
    mmio_write32(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_IMAN, irman | XHCI_IMAN_IE);

    //also enbale interruptor in Op Cmd register
    uint32_t cmd = mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD);
    cmd |= XHCI_CMD_INTE | XHCI_CMD_HSEE;
    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, cmd);


    if (xhcihcd_hub_init(hcd) != USB_OK) {
        return USB_ERROR;
    }

    cpu_barrier();

    xhcidev->usb_hcd.hcdstate = HCD_STATE_SETUP;
    return 0;
}

int xhcihcd_start(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    //adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    //Start xhci by setting run bit
    uint32_t cmd = mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD);
    cmd |= XHCI_CMD_RS;
    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, cmd);

    //wait for bit XHCI_STS_HCH (halted) to clear
    int timeout = 50; //100ms
    while ( (mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_STS_HCH)  ) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for start command (HCH halted to be cleared)\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("xhci Start OK!\n");
    xhcidev->usb_hcd.hcdstate = HCD_STATE_STARTED;


    //Enumerate HUBS
    //list hubs
    for (int i = 0; i < xhcidev->hubcount; i++) {
        xhci_vhub_t* hub = &xhcidev->hubs[i];
        USBRESULT res = xhcihcd_hub_enumerate(hcd,hub);
        log_msg("xhci hub enumerate = %d\n",res);

    }

    return USB_OK;
}
int xhcihcd_stop(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}
int xhcihcd_release(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}
int xhcihcd_poll(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}

/*
static void xhcihcd_dump_irq_state(xhci_hcd_device_t *x) {
    adr_t ir0 = x->reg_rt_mmiobase + XHCI_RT_IR0;
    xhci_trb_t *e = &((xhci_trb_t *)x->er_virt)[x->event_dequeue];
    log_msg("USBSTS=%#x IMAN=%#x ERDP=%#lx deq=%d cyc=%d evt.ctrl=%#x\n",
            mmio_read32(x->reg_op_mmiobase + XHCI_OP_USBSTS),
            mmio_read32(ir0 + XHCI_IR_IMAN),
            mmio_read64(ir0 + XHCI_IR_ERDP),
            x->event_dequeue, x->event_cycle, e->control);
}*/
// ===== Commands ====================================================

// Put a command TRB on the command ring, ring doorbell 0 and wait for its
// Command Completion Event.
//
// BOOTSTRAP CONCESSION: the wait below drains the event ring itself instead
// of sleeping until xhci_irq() signals it. That is event polling, which AD-3
// otherwise forbids. It is here because there is no scheduler to sleep on
// yet and because xhci_start() runs before interrupts may even be unmasked
// (ARCHITECTURE.md section 6, "wait-for-completion primitive"). The
// interrupt path remains the production path - both call the same
// xhci_process_events() - so replacing this loop with a sleep/wake pair is a
// local change. Do not build anything else on top of the polling behaviour.
//
// KNOWN RACE, and the reason this must not survive contact with a real
// system: xhci_process_events() is not reentrant, so if the MSI vector is
// unmasked while this loop is spinning, xhci_irq() and this loop can drain
// the event ring at the same time and both advance event_dequeue. Bring-up
// is single threaded with the vector still masked, which is the only reason
// this is safe today. Unmask the vector only after the last synchronous
// caller is gone, or give the ring a lock.
USBRESULT xhcihcd_command_execute(usb_hcd_t *hcd,xhci_trb_t *trb, uint32_t *out_slot_id,uint32_t timeout) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;

    if (hcd==0 || hcd->hcdstate==HCD_STATE_DEAD) {
        PANIC("HCD IS DEAD\n");
        return USB_ERROR;
    }

    uint64_t trb_phys = V2P(xhcidev->cr_virt) + (xhcidev->cmd_enqueue * XHCI_TRB_SIZE);

    xhcidev->cmd_code = XHCI_CC_INVALID;
    xhcidev->cmd_trb_phys=trb_phys;
    xhcidev->cmd_slot_id=0;
    xhcidev->cmd_pending=1;
    cpu_barrier();

    xhcihcd_ring_enqueue((xhci_trb_t *) xhcidev->cr_virt,&xhcidev->cmd_enqueue,&xhcidev->cmd_cycle,trb);
    xhcihcd_doorbell(hcd,0, XHCI_DB_CMD_TARGET);  //slot 0 is always Command Doorbell

    //wait for bit XHCI_STS_HCH (halted) to clear
    int tloops = timeout/10;
    while ( xhcidev->cmd_pending && tloops-- ) {
        //TODO test if hcd is dead.... hcd->dead
        mdelay(10);
    }

    if (xhcidev->cmd_pending) {
        xhcidev->cmd_pending = 0;
        //return XHCI_ERR_TIMEOUT;
        log_msg("xhcihcd_command_execute timeout\n");
        return USB_ERROR;
    }


    if (out_slot_id) {
        *out_slot_id = xhcidev->cmd_slot_id;
    }
    return (xhcidev->cmd_code == XHCI_CC_SUCCESS) ? USB_OK : USB_ERROR;
}

// ===== Event handling ==============================================

void xhcihcd_handle_transfer_event(usb_hcd_t *hcd,const xhci_trb_t *trb) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    uint32_t slot_id = XHCI_TRB_GET_SLOT(trb->control);
    if (slot_id == 0 || slot_id > XHCI_MAX_SLOTS_SUPPORTED) {
        log_msg("xhcihcd_handle_transfer_event strange slot\n");
        return;
    }
    xhci_device_t *dev = getDeviceForSlot(xhcidev,slot_id);
    if (!dev->in_use) {
        log_msg("xhcihcd_handle_transfer_event not inuse \n");
        return;
    }

    // Endpoint ID in the event is the DCI (xHCI 1.2 figure 6-38, 4.5.1).
    uint32_t dci = XHCI_EVT_EP_ID(trb->control);
    uint32_t code = XHCI_EVT_COMP_CODE(trb->status);
    uint32_t residual = XHCI_EVT_XFER_LEN(trb->status);

    if (dci == XHCI_DB_EP0_TARGET) {
        // Default control endpoint: one synchronous transfer at a time,
        // waited on by xhcihcd_control_transfer_hw().
        if (!dev->xfer_pending) {
            return;
        }
        dev->xfer_code = code;
        dev->xfer_residual = residual;
        cpu_barrier();
        dev->xfer_pending = 0;
        return;
    }

    if (dci < 2 || dci > XHCI_MAX_DCI) {
        log_msg("xhcihcd_handle_transfer_event dci < 2 or dci > MAX\n");
        return;
    }

    // Interrupt / bulk endpoint. One outstanding request per endpoint and
    // one TRB per request, so the event cannot belong to anything else.
    // TODO: match trb->parameter (the TRB pointer) against the enqueued TRB
    // once more than one request per endpoint is allowed.
    xhci_endpoint_t *ep = dev->eps[dci];
    if (ep==NULL) {
        PANIC("xhcihcd_handle_transfer_event WITH ep==NULL");
    }
    struct usb_xfer_request *req = ep->req;
    if (!req) {
        log_msg("XHCI transfer event for idle endpoint slot=%d dci=%d cc=%d\n", slot_id, dci, code);
        return;
    }

    // The event reports the residual - what was NOT transferred.
    uint32_t len = req->xferlen;
    req->actual_xferlen = (residual <= len) ? (len - residual) : 0;

    if (code == XHCI_CC_SUCCESS || code == XHCI_CC_SHORT_PACKET) {
        req->status = USB_OK;
    } else {
        // Stall, babble and transaction errors leave the endpoint in the
        // Halted state (xHCI 1.2 section 4.10.2). Ringing its doorbell does
        // nothing until a Reset Endpoint command, which is not implemented,
        // so refuse further submits instead of hanging silently.
        log_msg("XHCI transfer error slot=%d dci=%d cc=%d, endpoint halted\n", slot_id, dci, code);
        ep->halted = 1;
        req->status = USB_ERROR;
    }

    // Release the endpoint BEFORE the callback so the callback may resubmit.
    cpu_barrier();
    ep->req = NULL;
    cpu_barrier();

    //log_msg("*** Transfer Event with datasize = \n", req->actual_xferlen);

    if (req->xfer_cb_func) {
        //log_msg("*** CALL cb at %#x\n", req->xfer_cb_func);
        req->xfer_cb_func(req);
    }
}


void xhcihcd_handle_command_completion(usb_hcd_t *hcd,const xhci_trb_t *trb) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    uint64_t cmd_phys = ((uint64_t)trb->parameter_hi << 32) |  (uint64_t)trb->parameter_lo;

    if (!xhcidev->cmd_pending || cmd_phys != xhcidev->cmd_trb_phys) {
        // Not the command we are waiting for. Nothing else issues commands
        // in this draft, so this means a stale or aborted completion.
        log_msg("TRB CMD is stale or aborted\n");
        return;
    }

    xhcidev->cmd_code = XHCI_EVT_COMP_CODE(trb->status);
    xhcidev->cmd_slot_id = XHCI_TRB_GET_SLOT(trb->control);
    cpu_barrier();
    xhcidev->cmd_pending = 0;

    //log_msg("Command completed successfully on xhci %#lx\n",xhcidev);
}


void xhcihcd_process_events(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;

    //adr_t mmioregbase = xhcidev->mmio_regbase;
    //adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t reg_rt_mmiobase = xhcidev->reg_rt_mmiobase;

    uint32_t consumed = 0;

    for (;;) {
        xhci_trb_t* eventring = (xhci_trb_t*) xhcidev->er_virt;
        xhci_trb_t* trb = &eventring[xhcidev->event_dequeue];
        uint32_t control = trb->control;

        //log_msg("Dequeue event control = %#x\n",control);

        // The cycle bit tells producer from consumer: when it stops matching
        // our cycle state, the controller has not written this slot yet.
        if ((control & XHCI_TRB_CYCLE) != (xhcidev->event_cycle ? 1 : 0)) {
            //log_msg("No more events in this cycle\n");
            break;
        }
        cpu_barrier();

        switch (XHCI_TRB_TYPE(control)) {
            case XHCI_TRB_TRANSFER_EVENT:
                //log_msg("TRANSFER EVENT!\n");
                xhcihcd_handle_transfer_event(hcd,trb);
                //PANIC("TODO XHCI_TRB_TRANSFER_EVENT");
                break;
            case XHCI_TRB_CMD_COMPLETION:
                xhcihcd_handle_command_completion(hcd,trb);
                break;
            case XHCI_TRB_PORT_STATUS_CHANGE:
                // TODO: hot-plug. Until the root hub handles these, a Port
                // Status Change Event (for example after a port reset, or a
                // QEMU device_add) is logged and dropped rather than fatal.
                // The change bits stay set in PORTSC, so nothing is lost.
                //xhci_hub_port_changed(XHCI_EVT_PORT_ID(trb->parameter_lo));
                PANIC("TODO XHCI_TRB_PORT_STATUS_CHANGE");
                break;
            case XHCI_TRB_HOST_CONTROLLER:
                // The controller is telling us it hit an internal error.
                hcd->hcdstate = HCD_STATE_DEAD;
                PANIC("DEAD");
                break;
            default:
                log_msg("OTHER EVENT!");
                // Unknown event types are counted by being ignored, never
                // interpreted. Guessing at an event layout corrupts state.
                break;
        }


        consumed++;
        xhcidev->event_dequeue++;
        if (xhcidev->event_dequeue >= XHCI_RING_TRBS) {
            xhcidev->event_dequeue = 0;
            xhcidev->event_cycle = (uint8_t)(xhcidev->event_cycle ? 0 : 1);
        }
    }


    if (consumed) {
        uint64_t erdp = V2P(xhcidev->er_virt) + (xhcidev->event_dequeue * XHCI_TRB_SIZE);
        // EHB is write-1-to-clear and must be cleared as part of the same
        // write that advances the dequeue pointer, or the controller will
        // not raise the next interrupt.
        mmio_write64(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_ERDP, erdp | XHCI_ERDP_EHB);
    }
}

irq_status_t xhcihcd_irqhandler(void *ctx) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) ctx;
    usb_hcd_t* hcd = (usb_hcd_t*) xhcidev;

    //adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    adr_t reg_rt_mmiobase = xhcidev->reg_rt_mmiobase;

    //log_msg("--XHCI IRQ : %s\n",xhcidev->usb_hcd.name);

    if (hcd==0 || hcd->hcdstate!=HCD_STATE_STARTED) {
        log_msg("Skip handling IRQ for NON STARTED state in xhci");
        return 0;
    }

    uint32_t status = mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS);

    if (status & (XHCI_STS_HSE | XHCI_STS_HCE)) {
        // Host System Error or Host Controller Error. The controller is not
        // recoverable by anything this driver does; mark it dead so no
        // further work is queued against it.
        //g_xhci.dead = 1;
        hcd->hcdstate=HCD_STATE_DEAD;
        PANIC("XHCI IS DEAD\n");
    }

    // Clear only the RW1C bits that are actually set. Writing the whole
    // register back would clear status the caller has not seen.
    uint32_t ack = status & XHCI_STS_RW1C_MASK;
    if (ack) {
        mmio_write32(reg_op_mmiobase + XHCI_OP_USBSTS, ack);
    }

    //Clear Interrupt Pending bit
    uint32_t iman = mmio_read32(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_IMAN);
    //log_msg("iman = #%x\n",iman);
    if (iman & XHCI_IMAN_IP) {
        // IP is RW1C; keep IE set while clearing it.
        mmio_write32(reg_rt_mmiobase + XHCI_RT_IR0 + XHCI_IR_IMAN, iman | XHCI_IMAN_IP);
    }

    xhcihcd_process_events(hcd);

    //PANIC("XHCI IRQ Handler\n");
    return IRQ_HANDLED;
}


// ===== Control transfers ===========================================
//
// A control transfer is a Setup Stage TRB, an optional Data Stage TRB and a
// Status Stage TRB on the endpoint's transfer ring. Only the last one has
// Interrupt On Completion set, so exactly one Transfer Event arrives per
// transfer.

USBRESULT xhcihcd_control_transfer_hw(xhci_hcd_device_t* xhcidev,xhci_device_t *dev,const usb_setup_packet_t *setup,void *buffer, uint16_t length,uint32_t *actual_length) {
    usb_hcd_t* hcd = (usb_hcd_t *) xhcidev;

    // IN or OUT
    xhci_trb_t trb;
    uint32_t trt = XHCI_TRT_NO_DATA;
    int data_in = 0;

    if (length > 0 && buffer) {
        data_in = (setup->bmRequestType & USB_DIR_IN) ? 1 : 0;
        trt = data_in ? XHCI_TRT_IN_DATA : XHCI_TRT_OUT_DATA;
    }

    // Setup Stage. IDT means the 8 setup bytes travel in the TRB itself
    // rather than through a pointer, which is how this stage always works.
    memset(&trb,0,sizeof(xhci_trb_t));
    memcpy(&trb.parameter_lo,setup,8);
    trb.status = 8u;
    trb.control = XHCI_TRB_SET_TYPE(XHCI_TRB_SETUP_STAGE) | XHCI_TRB_SET_TRT(trt) | XHCI_TRB_IDT;
    xhcihcd_ring_enqueue(dev->ep0_ring_virt, &dev->ep0_enqueue, &dev->ep0_cycle, &trb);

    // Data Stage.
    if (trt != XHCI_TRT_NO_DATA) {
        uint64_t buffer_phys = V2P(buffer);
        //xhci_zero(&trb, sizeof(trb));
        memset(&trb,0,sizeof(xhci_trb_t));
        trb.parameter_lo = (uint32_t)(buffer_phys & 0xFFFFFFFFu);
        trb.parameter_hi = (uint32_t)(buffer_phys >> 32);
        trb.status = length;
        trb.control = XHCI_TRB_SET_TYPE(XHCI_TRB_DATA_STAGE);
        if (data_in) {
            trb.control |= XHCI_TRB_DIR_IN;
        }
        //xhci_ring_enqueue(dev->ep0_ring_virt, &dev->ep0_enqueue,  &dev->ep0_cycle, &trb);
        xhcihcd_ring_enqueue(dev->ep0_ring_virt, &dev->ep0_enqueue, &dev->ep0_cycle, &trb);
    }

    // Status Stage. Its direction is the opposite of the data stage, and IN
    // when there was no data stage at all.
    memset(&trb,0,sizeof(xhci_trb_t));
    trb.control = XHCI_TRB_SET_TYPE(XHCI_TRB_STATUS_STAGE) | XHCI_TRB_IOC;
    if (trt == XHCI_TRT_NO_DATA || !data_in) {
        trb.control |= XHCI_TRB_DIR_IN;
    }

    dev->xfer_code = XHCI_CC_INVALID;
    dev->xfer_residual = 0;
    dev->xfer_pending = 1;
    cpu_barrier();

    xhcihcd_ring_enqueue(dev->ep0_ring_virt, &dev->ep0_enqueue, &dev->ep0_cycle, &trb);
    xhcihcd_doorbell(hcd,dev->slot_id, XHCI_DB_EP0_TARGET);


    // Same bootstrap concession as xhci_command_execute() - see the comment
    // there before copying this pattern anywhere else.
    int tloops = 500; //timeout/2;
    while ( dev->xfer_pending && tloops-- ) {
        //TODO test if hcd is dead.... hcd->dead
        if (hcd->hcdstate == HCD_STATE_DEAD) {
            dev->xfer_pending = 0;
            //return XHCI_ERR_HW;
            return USB_ERROR;
        }
        mdelay(2);
    }
    if (dev->xfer_pending) {
        dev->xfer_pending = 0;
        //return XHCI_ERR_TIMEOUT;
        return USB_ERROR;
    }
    if (actual_length) {
        // The event reports the residual - what was NOT transferred.
        uint32_t residual = dev->xfer_residual;
        *actual_length = (residual <= length) ? (length - residual) : 0u;
    }


    switch (dev->xfer_code) {
    case XHCI_CC_SUCCESS:
    case XHCI_CC_SHORT_PACKET:
        //return XHCI_OK;
        return USB_OK;
    case XHCI_CC_STALL_ERROR:
       //return XHCI_ERR_CMD_FAILED;
       return USB_ERROR;
    default:
       //return XHCI_ERR_CMD_FAILED;
       return USB_ERROR;
    }


}

USBRESULT xhcihcd_submit_ctrl_xfer_request(struct usb_xfer_request *req,int waitforcompletion,int timeout) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) req->dev->usb_hcd;


    usb_setup_packet_t* setup = req->setup_data;
    if (!setup) {
        //return USB_ERR_INVALID;
        return USB_ERROR;
    }

    /*
        // THE INTERCEPTION POINT (AD-9). A control transfer addressed to a
        // virtual root hub must never become TRBs - there is no device on the
        // bus to receive them. Ask the hub emulation first; it returns
        // XHCI_ERR_NO_DEVICE for any address that is not one of its hubs, and
        // only then does the request go to the hardware.
        int rc = xhci_hub_control(addr, setup, buffer, length, actual_length);
        if (rc != XHCI_ERR_NO_DEVICE) {
            switch (rc) {
                case XHCI_OK:              return USB_OK;
                case XHCI_ERR_UNSUPPORTED: return USB_ERR_UNSUPPORTED;
                case XHCI_ERR_BAD_PORT:    return USB_ERR_INVALID;
                default:                   return USB_ERR_IO;
            }
        }
*/
    if (req->dev->isRootHub) {
        PANIC ("THIS IS ROOT HUB, redirect to xhci_hub_control\n");
    }

    uint8_t slot_id = xhcidev->address_to_slot[req->dev->address];
    if (slot_id == 0 || slot_id > XHCI_MAX_SLOTS_SUPPORTED) {
        //return USB_ERR_NO_DEVICE;
        log_msg("Strange Slot\n");
        return USB_ERROR;
    }
    xhci_device_t *dev = getDeviceForSlot(xhcidev,slot_id);
    if (!dev->in_use) {
        //return USB_ERR_NO_DEVICE;
        log_msg("Device Is not inuse\n");
        return USB_ERROR;
    }

    USBRESULT res = xhcihcd_control_transfer_hw(xhcidev,dev, setup,req->xferbuf, req->xferlen, &req->actual_xferlen);
    return res;
}

USBRESULT xhcihcd_submit_endp_xfer_request_sync(struct usb_xfer_request *req,int waitforcompletion,int timeout) {
    PANIC("TODO");
    return USB_OK;
}



// ===== Interrupt and bulk transfers ================================
//
// One transfer ring per endpoint, allocated and configured the first time a
// request is submitted to it (Configure Endpoint, xHCI 1.2 section 4.6.6).
// A request is a single Normal TRB with IOC, so exactly one Transfer Event
// completes it, and xhcihcd_handle_transfer_event() calls the request's
// xfer_cb_func from interrupt context.
//
// Limits of this version - enough for a HID boot keyboard or mouse:
//   - one outstanding request per endpoint; the callback resubmits.
//   - xferbuf must be physically contiguous and must not cross a 64 KiB
//     boundary (xHCI 1.2 section 6.4.1: a TRB buffer may not).
//   - the FIRST submit to an endpoint issues Configure Endpoint and waits
//     for its completion interrupt, so it must come from process context
//     with interrupts enabled. Resubmits from the completion callback only
//     enqueue a TRB and ring the doorbell, which is safe in IRQ context.
//   - interrupt and bulk only. No isochronous, no streams.
//   - SuperSpeed Max Burst belongs to the SS Endpoint Companion descriptor,
//     which usb_core does not keep yet, so 0 is used for SS endpoints.
//   - a halted endpoint stays halted (no Reset Endpoint yet).
//   - Configure Endpoint runs after usb_core has sent SET_CONFIGURATION.
//     The spec order is the other way round (section 4.3.5); QEMU and
//     common controllers accept this, but it is the first suspect if a real
//     controller rejects the command with a Context State or Bandwidth error.

// USB 2.0 table 9-13, bmAttributes bits 1:0.
#define XHCI_USB_XFER_ISOCH     1u
#define XHCI_USB_XFER_BULK      2u
#define XHCI_USB_XFER_INTR      3u

// Average TRB Length hints, xHCI 1.2 section 4.14.1.1.
#define XHCI_AVG_TRB_LEN_INTR   1024u
#define XHCI_AVG_TRB_LEN_BULK   3072u


// Device Context Index for an endpoint address, xHCI 1.2 section 4.5.1:
// DCI = endpoint number * 2 + direction (1 = IN).
static uint32_t xhci_ep_dci(uint8_t bEndpointAddress) {
    uint32_t num = bEndpointAddress & 0x0Fu;
    uint32_t in = (bEndpointAddress & 0x80u) ? 1u : 0u;
    return num * 2u + in;
}

// Endpoint Context Interval field: the service period is 2^Interval * 125 us
// (xHCI 1.2 section 6.2.3.6, table 6-12).
static uint32_t xhci_ep_interval(usb_speed_t speed, uint32_t xfer_type, uint8_t bInterval) {
    if (xfer_type != XHCI_USB_XFER_INTR) {
        return 0;   // bulk: not periodic
    }
    if (speed == USB_SPEED_HIGH || speed == USB_SPEED_SUPER ||
        speed == USB_SPEED_SUPER_PLUS) {
        // bInterval is already an exponent: period = 2^(bInterval-1)
        // microframes, bInterval 1..16.
        uint32_t b = bInterval;
        if (b < 1) {
            b = 1;
        }
        if (b > 16) {
            b = 16;
        }
        return b - 1;
    }
    // Full/low speed: bInterval is a frame count (1..255 ms). Convert to
    // 125 us units and round DOWN to a power of two, so the endpoint is
    // polled at least as often as it asked. Valid range is 3..10.
    uint32_t units = (uint32_t)(bInterval ? bInterval : 1) * 8u;
    uint32_t exp = 0;
    while ((2u << exp) <= units) {
        exp++;
    }
    if (exp < 3) {
        exp = 3;
    }
    if (exp > 10) {
        exp = 10;
    }
    return exp;
}



// Allocate the endpoint's transfer ring and tell the controller about it
// with Configure Endpoint. Blocks on the command completion interrupt.
static USBRESULT xhcihcd_configure_endpoint(usb_hcd_t *hcd, xhci_device_t *dev,
                                            const struct USB20_endpoint_descriptor *desc,
                                            uint32_t dci) {

    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    uint32_t xfer_type = desc->bmAttributes & 0x3u;
    int in = (desc->bEndpointAddress & 0x80u) ? 1 : 0;

    uint32_t ep_type;
    if (xfer_type == XHCI_USB_XFER_INTR) {
        ep_type = in ? XHCI_EP_TYPE_INTERRUPT_IN : XHCI_EP_TYPE_INTERRUPT_OUT;
    } else if (xfer_type == XHCI_USB_XFER_BULK) {
        ep_type = in ? XHCI_EP_TYPE_BULK_IN : XHCI_EP_TYPE_BULK_OUT;
    } else {
        log_msg("XHCI: endpoint %#x type %d not supported\n",
                desc->bEndpointAddress, xfer_type);
        return USB_ERROR;
    }

    xhci_endpoint_t *ep = dev->eps[dci];

    if (!ep->ring_virt) {
        ep->ring_virt = (xhci_trb_t *) page_alloc_pages_virt(ZONENORMAL, 0);
        if (!ep->ring_virt) {
            return USB_ERROR;
        }
    }
    xhcihcd_ring_init(ep->ring_virt);
    ep->enqueue = 0;
    ep->cycle = 1;
    ep->req = NULL;
    ep->halted = 0;

    // wMaxPacketSize bits 10:0 are the packet size. For high-speed periodic
    // endpoints bits 12:11 are the additional transactions per microframe,
    // which is what xHCI calls Max Burst Size (section 6.2.3.4).
    uint32_t wmps = desc->wMaxPacketSize;
    uint32_t mps = wmps & 0x7FFu;
    uint32_t burst = 0;
    if (dev->speed == USB_SPEED_HIGH && xfer_type == XHCI_USB_XFER_INTR) {
        burst = (wmps >> 11) & 0x3u;
    }
    uint32_t interval = xhci_ep_interval(dev->speed, xfer_type, desc->bInterval);
    // Max ESIT Payload: bytes per service interval, periodic endpoints only.
    uint32_t esit = (xfer_type == XHCI_USB_XFER_INTR) ? mps * (burst + 1u) : 0u;
    uint32_t avg = (xfer_type == XHCI_USB_XFER_INTR) ? XHCI_AVG_TRB_LEN_INTR
                                                     : XHCI_AVG_TRB_LEN_BULK;

    // Build the input context from scratch every time.
    memset(dev->in_ctx_virt, 0, PAGESIZE);

    // Add the slot context (A0, for Context Entries) and this endpoint.
    xhci_input_control_context_t *icc = (xhci_input_control_context_t *)dev->in_ctx_virt;
    icc->drop_flags = 0;
    icc->add_flags = (1u << 0) | (1u << dci);

    // Slot context: start from what the controller holds now (output context
    // entry 0), then raise Context Entries to cover the new DCI.
    xhci_slot_context_t *in_slot = (xhci_slot_context_t *) xhci_context_at(hcd, dev->in_ctx_virt, 1);
    const xhci_slot_context_t *out_slot = (const xhci_slot_context_t *) xhci_context_at(hcd, dev->dev_ctx_virt, 0);
    memcpy(in_slot, out_slot, xhcidev->context_size);
    uint32_t entries = (in_slot->dw0 >> 27) & 0x1Fu;
    if (dci > entries) {
        entries = dci;
    }
    in_slot->dw0 = (in_slot->dw0 & ~XHCI_SLOT_SET_CTX_ENTRIES(0x1Fu)) | XHCI_SLOT_SET_CTX_ENTRIES(entries);

    // Endpoint context: input context entry DCI + 1 (entry 0 is the ICC).
    xhci_endpoint_context_t *epc = (xhci_endpoint_context_t *) xhci_context_at(hcd, dev->in_ctx_virt, dci + 1u);
    epc->dw0 = XHCI_EP_SET_INTERVAL(interval) | (((esit >> 16) & 0xFFu) << 24);   // 31:24 Max ESIT Payload Hi
    epc->dw1 = XHCI_EP_SET_TYPE(ep_type) | XHCI_EP_SET_CERR(3u) |
               XHCI_EP_SET_MAX_BURST(burst) | XHCI_EP_SET_MAX_PACKET(mps);
    epc->deq_lo = (uint32_t)(V2P(ep->ring_virt) & 0xFFFFFFFFu) | XHCI_EP_DCS;
    epc->deq_hi = (uint32_t)(V2P(ep->ring_virt) >> 32);
    epc->dw4 = ((esit & 0xFFFFu) << 16) | (avg & 0xFFFFu);

    log_msg("XHCI configure ep %#x: slot=%d dci=%d type=%d mps=%d burst=%d interval=%d esit=%d entries=%d\n",
            desc->bEndpointAddress, dev->slot_id, dci, ep_type, mps, burst, interval, esit, entries);
    cpu_barrier();

    xhci_trb_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.parameter_lo = (uint32_t)(V2P(dev->in_ctx_virt) & 0xFFFFFFFFu);
    cmd.parameter_hi = (uint32_t)(V2P(dev->in_ctx_virt) >> 32);
    cmd.control = XHCI_TRB_SET_TYPE(XHCI_TRB_CONFIG_ENDPOINT) | XHCI_TRB_SET_SLOT(dev->slot_id);

    USBRESULT res = xhcihcd_command_execute(hcd, &cmd, NULL, 1000);
    if (res != USB_OK) {
        log_msg("XHCI Configure Endpoint failed, cc=%d\n", xhcidev->cmd_code);
        return res;
    }
    ep->configured = 1;

    return USB_OK;

}

// Asynchronous submit: queue one Normal TRB, ring the endpoint doorbell and
// return. Completion is reported through req->xfer_cb_func.
USBRESULT xhcihcd_submit_endp_xfer_request(struct usb_xfer_request *req) {
    usb_hcd_t *hcd = (usb_hcd_t *) req->dev->usb_hcd;
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;

    if (!req || !req->dev || !req->endpoint_desc) {
        return USB_ERROR;
    }
    if (hcd->hcdstate != HCD_STATE_STARTED) {
        return USB_ERROR;
    }

    uint8_t slot_id = xhcidev->address_to_slot[req->dev->address];
    if (slot_id == 0 || slot_id > XHCI_MAX_SLOTS_SUPPORTED) {
        log_msg("XHCI endp submit: no slot for address %d\n", req->dev->address);
        return USB_ERROR;
    }
    //xhci_device_t *dev = &xhcidev->devices[slot_id];
    xhci_device_t *dev = getDeviceForSlot(xhcidev,slot_id);
    if (!dev->in_use) {
        return USB_ERROR;
    }

    const struct USB20_endpoint_descriptor *desc = req->endpoint_desc;
    uint32_t dci = xhci_ep_dci(desc->bEndpointAddress);
    if (dci < 2 || dci > XHCI_MAX_DCI) {
        return USB_ERROR;
    }

    xhci_endpoint_t *ep = dev->eps[dci];
    if (ep==NULL) {
        ep = kmalloc(sizeof(xhci_endpoint_t),0);
        memset(ep,0,sizeof(xhci_endpoint_t));
        dev->eps[dci] = ep;
    }


    if (!ep->configured) {
        USBRESULT res = xhcihcd_configure_endpoint(hcd, dev, desc, dci);
        if (res != USB_OK) {
            return res;
        }
    }
    if (ep->halted) {
        log_msg("XHCI endp submit: dci=%d is halted (Reset Endpoint not implemented)\n", dci);
        return USB_ERROR;
    }
    if (ep->req) {
        log_msg("XHCI endp submit: dci=%d already has a request outstanding\n", dci);
        return USB_ERROR;
    }

    uint32_t len = req->xferlen;
    uint64_t phys = V2P(req->xferbuf);
    // TRB Transfer Length is 17 bits and one TRB buffer may not cross a
    // 64 KiB boundary (xHCI 1.2 section 6.4.1).
    if (len == 0 || len > 0x10000u || ((phys & 0xFFFFu) + len) > 0x10000u) {
        log_msg("XHCI endp submit: buffer %#lx len %d not supported in one TRB\n", phys, len);
        return USB_ERROR;
    }

    xhci_trb_t trb;
    memset(&trb, 0, sizeof(trb));
    trb.parameter_lo = (uint32_t)(phys & 0xFFFFFFFFu);
    trb.parameter_hi = (uint32_t)(phys >> 32);
    // 16:0 TRB Transfer Length, 21:17 TD Size = 0 (last TRB of the TD),
    // 31:22 Interrupter Target = 0.
    trb.status = len & 0x1FFFFu;
    // ISP so a short packet completes the request at once, IOC so the
    // controller posts an event at all. With both set on the same TRB, a
    // short packet still produces exactly one event (section 4.10.1.1).
    trb.control = XHCI_TRB_SET_TYPE(XHCI_TRB_NORMAL) | XHCI_TRB_IOC | XHCI_TRB_ISP;

    req->actual_xferlen = 0;
    ep->req = req;
    cpu_barrier();

    xhcihcd_ring_enqueue(ep->ring_virt, &ep->enqueue, &ep->cycle, &trb);
    xhcihcd_doorbell(hcd, dev->slot_id, dci);

    return USB_OK;
}

int xhcihcd_pci_probe(device_t *dev) {
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    //log_msg("hcihcd_pci_probe\n");


    xhci_hcd_device_t* priv = kmalloc(sizeof(xhci_hcd_device_t),0);
    if (!priv) {
        return -ENOMEM;
    }
    // Everything below assumes zeroed state: devices[].in_use, the
    // endpoint rings, cmd_pending and the usb_hcd ops not set here.
    memset(priv, 0, sizeof(xhci_hcd_device_t));
    dev->priv = priv;

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "xhcihcd PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    // BAR0 is a 64-bit memory BAR on every xHCI implementation. Probe its
    // size the standard PCI way: write all ones, read back, restore.
    if (pcidev->bar[0].bartype != BARTYPE_MEM) {
        PANIC("XHCI ONLY SUPPORTS MEMORY BAR");
    }


    log_msg("BAR 0 (MEMORY)   = 0x%lx \n",pcidev->bar[0].addr);
    adr_t reg_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    reg_base = pci_iomap(pcidev, &pcidev->bar[0], 0);
    if (!reg_base) {
        pci_release_region(pcidev, &pcidev->bar[0]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map reg_base via pci_iomap");
        return -ENOMEM;
    }
    priv->io_regbase=0;
    priv->mmio_regbase=reg_base; //disable
    adr_t mmioregbase = priv->mmio_regbase;

    //test USB HCD Version från PCI config extended field 0x60
    uint8_t usbversion = pci_config_read8(pcidev,PCICONFIG_USB_EXT_SBRN); //0x60, USB version of UHCI=0x10, EHCI=0x20, xHCI=0x30
    log_msg("usbversion= %#x\n",usbversion);
    if ((usbversion==0x30) || (usbversion==0x31) || (usbversion==0x32)){
        log_msg("USB Version OK!");
    }else {
        PANIC("STRANGE USB VERSION FOR XHCI CONTROLLER");
        return ERROR;
    }


    //Enable I/O + bumastering + intx disable
    uint16_t command = pci_config_read16(pcidev,PCICONFIG_COMMAND);
    log_msg("command= %#x\n",command);
    command = command | PCICONFIG_CMD_MEM_SPACE | PCICONFIG_CMD_BUS_MASTER | PCICONFIG_CMD_INTX_DISABLE;  // bit0=I/O Space, bit1=Memory Space, bit2=Bus Master, bit10 = intx disable
    pci_config_write16(pcidev,PCICONFIG_COMMAND,command);

    //test MSI support
    uint32_t capptr_msi = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSI);
    uint32_t capptr_msix = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSIX);
    log_msg("Found MSI Capability PTR = 0x%x\n", capptr_msi);
    log_msg("Found MSIX Capability PTR = 0x%x\n", capptr_msix);
    if (capptr_msi==0 && capptr_msix==0) {
        PANIC("This XHCI Controller has no MSI or MSIX support!");
        /*
        uint8_t msiirq=11+100+32; //100 is MSI IRQ OFSETT
        adr_t message_adr_reg32 = 0x0FEE << 20;
        uint32_t core_id = 0x0;
        message_adr_reg32 = message_adr_reg32 | (core_id << 12);
        pci_config_msi_addirq(pcidev,capptr,msiirq,qemuedu_irqhandler,message_adr_reg32) ;
        */
    }

    if (capptr_msi>0) {
        log_msg("USE MSI Capability PTR = 0x%x\n", capptr_msi);
        uint8_t msiirq = irq_get_free_msi();
        log_msg("USE MSI ISR = %d\n", msiirq);

        irq_add_irq_handler(msiirq,xhcihcd_irqhandler, priv);

        adr_t message_adr_reg32 = 0x0FEE << 20;
        uint32_t core_id = 0x0;
        message_adr_reg32 = message_adr_reg32 | (core_id << 12);
        adr_t isradr =  isr_get_isr_stub_addr(msiirq);
        log_msg("USE MSI ISR ADR = %#lx\n", isradr);
        pci_config_msi_addirq(pcidev,capptr_msi,msiirq, (void*)isradr ,message_adr_reg32) ;
    }

    // Register block bases. Everything below depends on these.
    //uint8_t caplength = mmio_read8(mmioregbase+XHCI_CAP_CAPLENGTH);
    //uint16_t hci_version = mmio_read16(mmioregbase+XHCI_CAP_HCIVERSION);

    //Workaround for Qemu. (doesnt allow to do reads that arent 32bit aligned), shoule also work in bare metal
    uint32_t cap_reg32 = mmio_read32(mmioregbase+XHCI_CAP_CAPLENGTH);
    uint8_t caplength = cap_reg32 & 0xFF;   //Extract CAPLENGTH (Bits 0-7)
    uint16_t hci_version = (cap_reg32 >> 16) & 0xFFFF;  //Extract HCIVERSION (Bits 16-31)


    adr_t   opregoff = caplength;
    uint16_t rtsoff = mmio_read32(mmioregbase+XHCI_CAP_RTSOFF) & ~0x1F;
    uint16_t dboff = mmio_read32(mmioregbase+XHCI_CAP_DBOFF) & ~0x03; //clear bit 0:1

    log_msg("XHCI CAPABILLITY registers:\n");
    log_msg("  CAPLENGTH = %d (%#x)\n",caplength,caplength);
    log_msg("  HCIVERSION = %d (%#x)\n",hci_version,hci_version);
    log_msg("  OPREGOFF = %d (%#x)\n",opregoff,opregoff);
    priv->reg_op_mmiobase = mmioregbase+opregoff;
    log_msg("  RTSOFF = %d (%#x)\n",rtsoff,rtsoff);
    log_msg("  DBOFF = %d (%#x)\n",dboff,dboff);
    priv->reg_rt_mmiobase = mmioregbase+rtsoff;
    priv->reg_db_mmiobase = mmioregbase+dboff;

    uint32_t hccp1 = mmio_read32(mmioregbase+XHCI_CAP_HCCPARAMS1);
    log_msg("  HCCPARAMS1 = %#x\n",hccp1);
    uint32_t xECPoff =  ((uint32_t)((hccp1 >> 16) & 0xFFFF))*4;
    priv->xECPoff = xECPoff;
    log_msg("  xECPoff = %d,(%#x)\n",xECPoff,xECPoff);





    uint32_t ac64 =  hccp1 & XHCI_HCCP1_AC64;
    uint32_t csz =  hccp1 & XHCI_HCCP1_AC64;
    log_msg("  ac64 = %d,(%#x)\n",ac64,ac64);
    log_msg("  csz = %d,(%#x)   (1=64 bytes, 0=32 bytes)\n",csz,csz);  //1 = 64 byte, 0 = 32 byte
    uint32_t context_size = (hccp1 & XHCI_HCCP1_CSZ) ? 64u : 32u;
    priv->context_size = context_size;

    if (ac64==0) {
        PANIC("TODO implement support for 32bit adressing");
    }
    if (csz==0) {
        PANIC("TODO implement support for 32byte Context Size");
    }

    priv->usb_hcd.name = "XHCI";
    priv->usb_hcd.setup=xhcihcd_setup;
    priv->usb_hcd.start=xhcihcd_start;
    priv->usb_hcd.stop=xhcihcd_stop;
    priv->usb_hcd.release=xhcihcd_release;
    priv->usb_hcd.poll=xhcihcd_poll;

    //priv->usb_hcd.hcd_roothub_is_port_present = uhcihcd_roothub_is_port_present;
    //priv->usb_hcd.hcd_roothub_port_reset = uhcihcd_roothub_port_reset;
    //priv->usb_hcd.hcd_roothub_is_port_device_present=uhcihcd_roothub_is_port_device_present;
    //priv->usb_hcd.hcd_roothub_port_speed=uhcihcd_roothub_port_speed;

    priv->usb_hcd.submit_ctrl_xfer_request=xhcihcd_submit_ctrl_xfer_request;
    priv->usb_hcd.submit_endp_xfer_request_sync=xhcihcd_submit_endp_xfer_request_sync; //TODO, change to interrupt driver
    priv->usb_hcd.submit_endp_xfer_request=xhcihcd_submit_endp_xfer_request;


    priv->usb_hcd.hcdstate = HCD_STATE_UNDEFINED;

    usb_debug("XHCI PROBED!\n" );

    dev->initialized = 1;
    return 0;
}



int xhcihcd_pci_attach(device_t *dev) {
    xhci_hcd_device_t* priv = dev->priv;
    if (dev->initialized) {
        log_msg("xhcihcd_pci_attach\n");
        priv->usb_hcd.hcdstate = HCD_STATE_INIT;
        usb_attach_hcd(&priv->usb_hcd);
    }else {
        log_msg("Device is not initialized, skip attach\n");
    }

    return 0;
}



// ===== Interrupt self-test =========================================
//
// The smallest possible proof that the interrupt path works, runnable the
// moment xhci_start() returns and long before any port or device is touched.
//
// A No Op Command exercises the command ring, the doorbell, the event ring,
// the ERST and MSI delivery in a single step, and it touches nothing else:
// no ports, no slots, no DCBAA entries, no devices. If this passes, every
// piece of machinery that the rest of the driver stands on is working.
//
// Why this does not simply call xhci_command_execute(): that function drains
// the event ring itself (see the bootstrap concession above it), so it
// reports success even when no interrupt was ever delivered. This test waits
// WITHOUT draining, so the only thing that can complete it is xhci_irq()
// having run. If the wait times out, it then drains once on purpose, which
// is what separates "the interrupt never arrived" from "the command never
// completed" - two failures with completely different causes.
//
// Preconditions, all of which the test checks or documents:
//   - xhci_start() returned XHCI_OK (or XHCI_ERR_BIOS_HANDOFF). Commands are
//     not processed unless USBCMD.RS is set, so this cannot run any earlier.
//   - The kernel has wired its MSI vector to xhci_irq() and unmasked it, and
//     CPU interrupts are enabled.
//   - Nothing else is calling xhci_process_events() concurrently. That is
//     the whole point of the test, and it is also the race documented at
//     xhci_command_execute().

//int xhci_selftest_interrupt(xhci_selftest_result_t *out) {
void xhci_selftest_interrupt(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    //adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    //adr_t reg_rt_mmiobase = xhcidev->reg_rt_mmiobase;
    //PANIC("XHCI SELF TEST");

    /*
    xhci_selftest_result_t local;
    if (!out) {
        out = &local;
    }
    xhci_zero(out, sizeof(*out));

    if (!g_xhci.started) {
        return XHCI_SELFTEST_NOT_RUNNING;
    }

    out->usbsts_before = xhci_read32(g_xhci.op_base + XHCI_OP_USBSTS);
    out->crcr_before = xhci_read64(g_xhci.op_base + XHCI_OP_CRCR);
    out->irq_count_before = g_xhci.irq_count;
    out->event_count_before = g_xhci.event_count;

    if (out->usbsts_before & (XHCI_STS_HSE | XHCI_STS_HCE)) {
        return XHCI_SELFTEST_CONTROLLER_ERROR;
    }
    if (out->usbsts_before & XHCI_STS_HCH) {
        // Halted. Either xhci_start() did not finish or something stopped
        // the controller since.
        return XHCI_SELFTEST_NOT_RUNNING;
    }
    */

    uint32_t usbsts = mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS);
    log_msg("Halted = %#x\n",usbsts);

    xhci_trb_t cmd;
    memset(&cmd,0,sizeof(cmd));
    cmd.control = XHCI_TRB_SET_TYPE(XHCI_TRB_NO_OP_CMD);


    uint32_t out_slot_id=0;
    USBRESULT res = xhcihcd_command_execute(hcd,&cmd, &out_slot_id,1000);
    log_msg("res=%d  out_slot_id=%d\n",res,out_slot_id);

    res = xhcihcd_command_execute(hcd,&cmd, &out_slot_id,1000);
    log_msg("res=%d  out_slot_id=%d\n",res,out_slot_id);

    res = xhcihcd_command_execute(hcd,&cmd, &out_slot_id,1000);
    log_msg("res=%d  out_slot_id=%d\n",res,out_slot_id);

    PANIC("2HALT");



}


struct pcidriver xhcihcd_pci_driver = {
    .driver.name = "USB Universal Host Controller Interface (XHCI)",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = xhcihcd_pci_probe,
    .driver.attach = xhcihcd_pci_attach,
    .idents = (void *) &xhcihcd_pci_idents,
};

REGISTER_DRIVER(xhcihcddriver, "xhcihcd",&xhcihcd_pci_driver);

