#ifndef _DRIVER_UHCI_H_
#define _DRIVER_UHCI_H_

#include "types.h"
#include "driver/usb/usb.h"

//#define UHCI_DEBUG 1



#define UHCI_COMMAND     0x00
#define UHCI_STATUS      0x02

#define UHCI_INTERRUPT   0x04
#define UHCI_FRAME_NUM   0x06
#define UHCI_FRAME_BASE  0x08
#define UHCI_SOF_MOD     0x0C

// Reset wait times.  USB 2.0 specs, page 153, section 7.1.7.5, paragraph 3
#define USB_TDRSTR   50   // reset on a root hub
#define USB_TDRST    10   // minimum delay for a reset
#define USB_TRHRSI    3   // No more than this between resets for root hubs
#define USB_TRSTRCY  10   // reset recovery

#define UHCI_PORT_WRITE_MASK  0x124E    //  0001 0010 0100 1110

#define TOKEN_OUT    0xE1
#define TOKEN_IN     0x69
#define TOKEN_SETUP  0x2D


//Queue-chains
#define Q16  0
#define Q8   1
#define Q4   2
#define Q2   3
#define Q1   4
#define QINTR  5
#define QCTRL  6
#define QBULK  7

struct uhci_hcd_priv {
    struct usb_hcd  usb_hcd;   //must be first, this way a pointer to usb_hcd is also pointer to usb_hcd...

    adr_t mmio_regbase;
    adr_t io_regbase;
    uint32_t *stackframe;
    struct uhci_queue_head* queue[8];   // the different queues.

    struct uhci_queue_head* curTransfer;   //change this to a linked list...
};


#define QUEUE_HEAD_PTR_MASK  0xFFFFFFF0
#define QUEUE_HEAD_Q         0x00000002
#define QUEUE_HEAD_T         0x00000001

struct uhci_queue_head {
    uint32_t   hlink;       //horizontal link
    uint32_t   vlink;       //vertical element link
    //driver use only (not needed by controller
    struct usb_xfer_request *xferReq;
    struct uhci_transfer_descriptor *tdhead;
    uint32_t   parentQH;    //TOP QH ie one of Q1.Q2,Q4 etc (phys address)
//    uint32_t   resv1;       //filler to make struct of size 16 bytes


} __attribute__((packed,aligned(16)));   //must be 16 bytes aligned to work with uhci controller.

//struct LUNT_UHCI_QUEUE_HEAD {
//    uint32_t   horz_ptr;
//    uint32_t   vert_ptr;
//    uint32_t   resv0;       //filler to make struct of size 16 bytes
//    uint32_t   resv1;       //filler to make struct of size 16 bytes
//} __attribute__((packed));

//struct LUNT_UHCI_TRANSFER_DESCRIPTOR {
//    uint32_t   link_ptr;
//    uint32_t   reply;
//    uint32_t   info;
//    uint32_t   buff_ptr;
//    uint32_t   resv0[4];          // the last 4 dwords are reserved for software use.
//} __attribute__((packed));

struct uhci_transfer_descriptor {
    uint32_t   link;
    uint32_t   ctrl;
    uint32_t   token;
    uint32_t   buffer;
    //uint32_t   resv0[4];          // the last 4 dwords are reserved for software use.
    //Internal
    struct uhci_transfer_descriptor *tdnext; //if this transfer has several linked td's
} __attribute__((packed,aligned(16)));



#endif