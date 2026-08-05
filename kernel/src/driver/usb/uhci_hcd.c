#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/usb/usb.h"
#include "error.h"
#include "cpu/mmio.h"
#include "cpu/io.h"
#include "mm.h"
#include "delay.h"

//#define UHCI_DEBUG 1

const struct pci_device_id uhcihcd_pci_idents[] = {
    PCI_DEVICE(0x8086, 0x7020,"Intel Corporation : 82371SB PIIX3 USB [Natoma/Triton II]"),   //UHCI:  Intel Corporation :  82371SB PIIX3 USB [Natoma/Triton II]
    PCI_DEVICE(0x8086, 0x7112,"Intel Corporation : 82371AB/EB/MB PIIX4 USB"),   //UHCI:  Intel Corporation :  82371AB/EB/MB PIIX4 USB
    PCI_DEVICE(0x1106, 0x3038,"VIA Technologies, Inc. : VT82xx/62xx UHCI USB 1.1 Controller"),   //
    PCI_DEVICE_END,
};

typedef struct uhci_td {
    uint32_t   link;
    uint32_t   ctrl;
    uint32_t   token;
    uint32_t   buffer;
    //uint32_t   resv0[4];          // the last 4 dwords are reserved for software use.

    //Internal driver use only (not needed by controller
    struct uhci_td* tdnext; //if this transfer has several linked td's
} __attribute__((packed,aligned(16))) uhci_td_t;

typedef struct uhci_qh {
    //defined by controller
    uint32_t   head_link;            //hlink;       //horizontal link
    uint32_t   element_link;         //vlink;       //vertical element link

    //Internal driver use only (not needed by controller
    struct usb_xfer_request *xferReq;
    uhci_td_t* tdhead;
    uint32_t   parent_qh;    //TOP QH ie one of Q1.Q2,Q4 etc (phys address)
    //uint32_t   resv1;       //filler to make struct of size 16 bytes
    //uint32_t   resv2;       //filler to make struct of size 16 bytes
} __attribute__((packed,aligned(16))) uhci_qh_t;   //must be 16 bytes aligned to work with uhci controller.

#define QSLOTS 11
typedef struct uhci_hcd_device {
    usb_hcd_t  usb_hcd;   //must be first, this way a pointer to usb_hcd is also pointer to usb_hcd...

    adr_t mmio_regbase;
    adr_t io_regbase;
    uint32_t *stackframe;  //Frame List: 1024 x 32-bit pointers, 4096-byte aligned.
    //adr_t queueframe;       //adress to block of allocated qh's
    uhci_qh_t* queue[QSLOTS];   // the different queues.
    //uhci_qh_t* curTransfer;   //change this to a linked list...
} uhci_hcd_device_t;

// ============================== UHCI I/O registers =======================
#define UHCI_REG_USBCMD    0x00   // 16-bit
#define UHCI_REG_USBSTS    0x02   // 16-bit
#define UHCI_REG_USBINTR   0x04   // 16-bit
#define UHCI_REG_FRNUM     0x06   // 16-bit
#define UHCI_REG_FRBASEADD 0x08   // 32-bit
#define UHCI_REG_SOFMOD    0x0C   // 8-bit
#define UHCI_REG_PORTSC1   0x10   // 16-bit
#define UHCI_REG_PORTSC2   0x12   // 16-bit

//  USBCMD bits
#define UHCI_CMD_RS      0x0001 /* Run/Stop: 1 = execute the frame list   */
#define UHCI_CMD_HCRESET 0x0002 /* Host Controller Reset, self-clearing   */
#define UHCI_CMD_GRESET  0x0004 /* Global Reset (resets attached devices) */
#define UHCI_CMD_EGSM    0x0008 /* Enter Global Suspend Mode              */
#define UHCI_CMD_FGR     0x0010 /* Force Global Resume                    */
#define UHCI_CMD_SWDBG   0x0020 /* Software Debug                         */
#define UHCI_CMD_CF      0x0040 /* Configure Flag (informational to HC)   */
#define UHCI_CMD_MAXP    0x0080 /* Max Packet: 0=32 bytes, 1=64 bytes     */

// USBSTS bits — all write-1-to-clear except Halted, which just mirrors RS.
#define UHCI_STS_USBINT  0x0001
#define UHCI_STS_ERROR   0x0002
#define UHCI_STS_RESUME  0x0004
#define UHCI_STS_HSE     0x0008 /* Host System Error (fatal, e.g. bad DMA) */
#define UHCI_STS_HCPE    0x0010 /* Host Controller Process Error          */
#define UHCI_STS_HALTED  0x0020

//  PORTSC1/2 bits
#define UHCI_PORTSC_CCS   0x0001 /* Current Connect Status (RO)           */
#define UHCI_PORTSC_CSC   0x0002 /* Connect Status Change (R/WC)          */
#define UHCI_PORTSC_PE    0x0004 /* Port Enable (R/W)                     */
#define UHCI_PORTSC_PEC   0x0008 /* Port Enable Change (R/WC)             */
#define UHCI_PORTSC_LS    0x0030 /* Line Status (RO)                      */
#define UHCI_PORTSC_RD    0x0040 /* Resume Detect (R/W)                   */
#define UHCI_PORTSC_RSVD1 0x0080 /* reserved, always reads 1              */
#define UHCI_PORTSC_LSDA  0x0100 /* Low Speed Device Attached (RO)        */
#define UHCI_PORTSC_PR    0x0200 /* Port Reset (R/W)                      */
#define UHCI_PORTSC_SUSP  0x1000 /* Suspend (R/W)                         */

/* Bits that are Write-1-to-Clear; a plain read-modify-write of the whole
 * register would accidentally clear these if they happened to read back 1,
 * so every read-modify-write below masks them out before OR-ing in the bit
 * it actually means to change. */
#define UHCI_PORTSC_WC_MASK (UHCI_PORTSC_CSC | UHCI_PORTSC_PEC)

/* ============================== ================== ======================= */

#define UHCI_QH_PTR_MASK  0xFFFFFFF0
#define UHCI_QH_Q         0x00000002
#define UHCI_QH_T         0x00000001


// TD Control and Status
//#define TD_CS_ACTLEN                    0x000007ff
#define TD_CS_BITSTUFF                  (1 << 17)     // Bitstuff Error
#define TD_CS_CRC_TIMEOUT               (1 << 18)     // CRC/Time Out Error
#define TD_CS_NAK                       (1 << 19)     // NAK Received
#define TD_CS_BABBLE                    (1 << 20)     // Babble Detected
#define TD_CS_DATABUFFER                (1 << 21)     // Data Buffer Error
#define TD_CS_STALLED                   (1 << 22)     // Stalled
#define TD_CS_ACTIVE                    (1 << 23)     // Active
#define TD_CS_IOC                       (1 << 24)     // Interrupt on Complete
//#define TD_CS_IOS                       (1 << 25)     // Isochronous Select
//#define TD_CS_LOW_SPEED                 (1 << 26)     // Low Speed Device
//#define TD_CS_ERROR_MASK                (3 << 27)     // Error counter
//#define TD_CS_ERROR_SHIFT               27
//#define TD_CS_SPD                       (1 << 29)     // Short Packet Detect

// TD Token
//#define TD_TOK_PID_MASK                 0x000000ff    // Packet Identification
//#define TD_TOK_DEVADDR_MASK             0x00007f00    // Device Address
#define TD_TOK_DEVADDR_SHIFT            8
//#define TD_TOK_ENDP_MASK                00x0078000    // Endpoint
#define TD_TOK_ENDP_SHIFT               15
//#define TD_TOK_D                        0x00080000    // Data Toggle
#define TD_TOK_D_SHIFT                  19
//#define TD_TOK_MAXLEN_MASK              0xffe00000    // Maximum Length
#define TD_TOK_MAXLEN_SHIFT             21



#define TOKEN_OUT    0xE1
#define TOKEN_IN     0x69
#define TOKEN_SETUP  0x2D


//Queue-chain indexes (to uhci_hcd_device_t.queue[])
#define Q128  0
#define Q64  1
#define Q32  2
#define Q16  3
#define Q8   4
#define Q4   5
#define Q2   6
#define Q1   7
#define QINTR  8
#define QCTRL  9
#define QBULK  10


// Reset wait times.  USB 2.0 specs, page 153, section 7.1.7.5, paragraph 3
#define USB_TDRSTR   50   // reset on a root hub
#define USB_TDRST    10   // minimum delay for a reset
#define USB_TRHRSI    3   // No more than this between resets for root hubs
#define USB_TRSTRCY  10   // reset recovery

#define UHCI_PORT_WRITE_MASK  0x124E    //  0001 0010 0100 1110

void uhcihcd_irqhandler() {
    log_msg("hcihcd_irqhandler\n");
}

uhci_qh_t* uhci_allocQH(){
    //TODO replace with cahce later on.
    uhci_qh_t* qh = kmalloc_aligned(sizeof(uhci_qh_t),16,GFP_DMA16);
    qh->head_link = 0x00000001;
    qh->element_link = 0x00000001;
    qh->xferReq=0;
    return qh;
}
void uhci_freeQH(uhci_qh_t* qh){
    //TODO replace with cahce later on.
    kfree_aligned(qh);
}


uhci_td_t* uhci_allocTDs(size_t tdcount){
    //TODO replace with cahce later on.
    uhci_td_t* td = kmalloc_aligned(sizeof(uhci_td_t)*tdcount,16,GFP_DMA16);
    for (int i=0; i<tdcount; i++) {
        td[i].link=0;
        td[i].ctrl=0;
        td[i].token=0;
        td[i].buffer=0;
        td[i].tdnext=&td[i+1];
        log_msg("alloc td[%d] at %#lx\n",i,&td[i]);
    }
    td[tdcount-1].tdnext=0;

    return td;
}
void uhci_freeTDs(uhci_td_t* td){
    //TODO replace with cahce later on.
    kfree_aligned(td);
}

#ifdef UHCI_DEBUG
void uhci_debug_td_chain(adr_t qptr,adr_t tdphysptr){

    struct uhci_td *td;
    struct uhci_qh *queue;

    queue = (struct uhci_qh *) P2V(qptr);

    usb_debug("----------------------------------------------------\n");
    usb_debug("- DEBUG TD CHAIN FROM QH @%#x, head_link=%#x, element_link=%#x\n",queue,queue->head_link,queue->element_link);

    td = (struct uhci_td *) P2V(tdphysptr);

    uint8_t end = 0;
    int tdidx = 0;
    while(!end){
        usb_debug("td[%d]@%#x, link=%#x, ctrl=%#x, token=%#x, buf=%#x\n",tdidx,td,td->link,td->ctrl,td->token,td->buffer);
        if(td->buffer){
            uint8_t *ptr = (uint8_t *) P2V(td->buffer);
            usb_debug("  - buf = %x %x %x %x %x %x %x %x \n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
        }
        if(td->link == 1){
            end=1;
        }else{
            td = (struct uhci_td *)  P2V((td->link & 0xFFFFFFF0));
        }
        tdidx++;
        if(tdidx > 10){
            PANIC("LONG CHain???");
        }
    }
    usb_debug("----------------------------------------------------\n");
}


void uhci_debug_Q_chain(struct uhci_qh *qh, int stopAtTD){
    log_msg("---------------------------\n");
    log_msg("DEBUG QH CHAIN FROM  Q@%#x, horiz=%#x, vert=%#x  stopAtTD=%d\n",qh,qh->head_link,qh->element_link,stopAtTD);
    int cont=1;
    while(cont){
        cont=0;
        log_msg("- Q@%#x, horiz=%#x, vert=%#x   ",qh,qh->head_link,qh->element_link);
        if(qh->element_link>8){
            if(qh->element_link & UHCI_QH_Q){
                cont=1;
                qh = (struct uhci_qh *)  P2V((qh->element_link & 0xFFFFFFF0));
                log_msg("=> VERT  \n");
            }else{
                log_msg("=> VERT TD  @ %#x \n",qh->element_link);
                if(stopAtTD==0){
                    //continue horizontally instead....
                    if(qh->head_link & UHCI_QH_Q){
                        cont=1;
                        qh = (struct uhci_qh *)  P2V((qh->head_link & 0xFFFFFFF0));
                        log_msg("=> HORIZ  \n");
                    }else{
                        log_msg("=> HORIZ END @ %#x \n",qh->head_link);
                    }
                }
            }
        }else if(qh->head_link>8){
            if(qh->head_link & UHCI_QH_Q){
                cont=1;
                qh = (struct uhci_qh *)  P2V((qh->head_link & 0xFFFFFFF0));
                log_msg("=> HORIZ  \n");
            }else{
                log_msg("=> HORIZ END @ %#x \n",qh->head_link);
            }
        }else {
            log_msg("=> UNKNWON END ???? \n");
        }
    }
    log_msg("\n---------------------------\n");

}
#else
void uhci_debug_td_chain(adr_t qptr,adr_t tdphysptr){
}
void uhci_debug_Q_chain(struct uhci_qh *qh, int stopAtTD){
}
#endif


/* *********************************************************************************** */
/*      ROOT HUB STUFF, Rewrite to virtual hub later                                   */

// See if there is a valid UHCI port at address base+port
// Bit 7 set = Always set.
// See if we can clear it.
BOOL uhcihcd_roothub_is_port_present(usb_hcd_t *hcd, uint8_t portidx) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    adr_t ioregbase = priv->io_regbase;

    if (portidx>2) {
        //uhci roothub only has 2 roothub ports
        return FALSE;
    }

    //portbase starts at 0x10 offsett from base;
    uint8_t port = UHCI_REG_PORTSC1+(portidx*2);

    // if bit 7 is 0, not a port
    if ((io_inw(ioregbase+port) & 0x0080) == 0)
        return FALSE;

    // try to clear it
    io_outw(ioregbase+port, io_inw(ioregbase+port) & ~0x0080);
    if ((io_inw(ioregbase+port) & 0x0080) == 0)
        return FALSE;

    // try to write/clear it
    io_outw(ioregbase+port, io_inw(ioregbase+port) | 0x0080);
    if ((io_inw(ioregbase+port) & 0x0080) == 0)
        return FALSE;

    // let's see if we write a 1 to bits 3:1, if they come back as zero
    io_outw(ioregbase+port, io_inw(ioregbase+port) | 0x000A);
    if ((io_inw(ioregbase+port) & 0x000A) != 0)
        return FALSE;

    // we should be able to assume this is a valid port if we get here
    return TRUE;
}

USBRESULT uhcihcd_roothub_port_reset(usb_hcd_t *hcd, uint8_t portidx) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    adr_t ioregbase = priv->io_regbase;


    int i;
    uint16_t val = 0;
    RESULT ret = ERROR;

    //portbase starts at 0x10 offsett from base;
    uint8_t port = UHCI_REG_PORTSC1+(portidx*2);


    io_outw(ioregbase+port, io_inw(ioregbase + port) | (1<<9));
    mdelay(USB_TDRSTR);
    io_outw(ioregbase + port, io_inw(ioregbase+port) & ~(1<<9));

    for (i=0; i<10; i++) {
        mdelay(USB_TRSTRCY);  // hold for USB_TRSTRCY ms (reset recovery time)
        val = io_inw(ioregbase + port);

        // if bit 0 is clear, nothing attached, don't enable
        if (!(val & (1<<0))) {
            ret = OK;
            break;
        }

        // if either enable_change or connection_change, clear them and continue.
        if (val & ((1<<3) | (1<<1))) {
            io_outw(ioregbase + port, val & UHCI_PORT_WRITE_MASK);
            continue;
        }

        // if the enable bit is set, break.
        if (val & (1<<2)) {
            ret = OK;
            break;
        }

        // else, set the enable bit
        io_outw(ioregbase + port, val | (1<<2));
    }

    return ret;
}

static inline uint16_t uhcihcd_roothub_port_status(usb_hcd_t *hcd, uint8_t portidx) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    adr_t ioregbase = priv->io_regbase;

    //portbase starts at 0x10 offsett from base;
    uint8_t port = UHCI_REG_PORTSC1+(portidx*2);
    return io_inw(ioregbase + port);
}

BOOL uhcihcd_roothub_is_port_device_present(usb_hcd_t *hcd, uint8_t portidx) {
    if (uhcihcd_roothub_port_status(hcd,portidx) & 0x01) {
        return TRUE;
    }
    return FALSE;
}

int uhcihcd_roothub_port_speed(usb_hcd_t *hcd, uint8_t portidx) {

    if(uhcihcd_roothub_port_status(hcd,portidx) & (1<<8)){
        return kUSB_SPEED_LOW;
    }else{
        return kUSB_SPEED_FULL;
    }
}

/* **********************        END ROOT HUB STUFF              ********************* */


void uhcihcd_insertTransferQH(struct uhci_qh* qhTransfer,struct uhci_qh* qhTop){
    if(qhTop->element_link == 1){
        //empty, just put first.
        qhTransfer->parent_qh = V2P(qhTop);
        qhTop->element_link = V2P(qhTransfer) | UHCI_QH_Q;
        //log_msg("Insert transfer QH @%#x to qhTOp @ %#x  \n",qhTransfer,qhTop);
    }else{
        //log_msg("Insert transfer QH @%#x to qhTOp @ %#x First search end of hlist \n",qhTransfer,qhTop);
        //GOTO end of queue
        struct uhci_qh* curQH = (struct uhci_qh*) P2V(qhTop->element_link & UHCI_QH_PTR_MASK);
        int i=0;
        while(curQH->head_link != 1){
            //log_msg(" curQH @ %#x hlink %#x !=1 try next \n",curQH,curQH->hlink);
            curQH = (struct uhci_qh*) P2V(curQH->head_link & UHCI_QH_PTR_MASK);
            if(i++>10){
                PANIC("10");
            }
        }
        //log_msg(" curQH @ %#x hlink %#x ==1 >>INSERT \n",curQH,curQH->hlink);
        qhTransfer->parent_qh = V2P(qhTop);
        curQH->head_link = V2P(qhTransfer) | UHCI_QH_Q;
    }
}

void uhcihcd_removeTransferQH(struct uhci_qh* qh){
    struct uhci_qh* qhTop = (struct uhci_qh*) P2V(qh->parent_qh);
    //log_msg(".................................\n");

    //log_msg("remove transfer QH@%#x  from topQH @ %#x\n",qh,qhTop);

    //traverse to find self
    struct uhci_qh* curQH = (struct uhci_qh*) P2V(qhTop->element_link & UHCI_QH_PTR_MASK);
    //First transfer Queue is always vertical from topQH
    //log_msg("Start link = %#x\n",curQH);
    if(curQH==qh){
        //remove and replace topQH with this qh's next hlink
        //log_msg("Remove top/first entry\n");
        qhTop->element_link = curQH->head_link;
        //log_msg(".................................\n");
        return;
    }
    struct uhci_qh* prevQH=curQH;
    while(curQH != qh){
        //next transfer Queues is always horizontal from each other?
        prevQH=curQH;
        curQH = (struct uhci_qh*) P2V(curQH->head_link & UHCI_QH_PTR_MASK);
        //log_msg("   > curQH = %#x\n",curQH);
    }
    //Remove..
    //log_msg("remove curQH %#x from prevQH %#x\n",curQH,prevQH);
    prevQH->head_link = curQH->head_link;
}



void uhcihcd_poll_qh_xfer(uhci_hcd_device_t* uhci_hcd_device,uhci_qh_t *qh){
    struct usb_xfer_request *req;
    req = qh->xferReq;

    //usb_dev_debug(req->dev,"Check Req=%#x QH=%#x  qh->vlink=%#x qh->tdhead=%#x\n",req,qh,qh->vlink,qh->tdhead);

    uhci_td_t *td = NULL;
    uhci_td_t *tdend = NULL;

    td = qh->tdhead;
    if (td==NULL) {
        return;
    }

    //if ((td->ctrl & TD_CS_ACTIVE)==0) {
    //    log_msg("Terminate");
    //}

    tdend = td;
    while(tdend->tdnext) {
        tdend = tdend->tdnext;
    }

    //log_msg("TD at %#x\n",td);
    //log_msg("TDEND at %#x\n",tdend);
    if(td==NULL){
        PANIC("TD IS NULL ???");
    }


    //TODO check error on all TD's , not just tdend

    //check status
    if((tdend->ctrl & TD_CS_ACTIVE) == 0){
        //tranfer finished
        //usb_dev_debug(req->dev,"TD_CS_ACTIVE = %d\n",(td->ctrl & TD_CS_ACTIVE));
        //uhci_debug_td_chain(V2P(qh),V2P(td));
        req->status = USB_OK;


        if (tdend->ctrl & TD_CS_NAK) {
            usb_dev_debug(req->dev," ERROR NAK\n");
        }

        if (tdend->ctrl & TD_CS_STALLED) {
            usb_dev_debug(req->dev," ERROR TD is stalled\n");
//            t->success = false;
//            t->complete = true;
        }

        if (tdend->ctrl & TD_CS_DATABUFFER) {
            usb_dev_debug(req->dev," ERROR TD data buffer error\n");
        }
        if (tdend->ctrl & TD_CS_BABBLE) {
            usb_dev_debug(req->dev," ERROR TD babble error\n");
        }
        if (tdend->ctrl & TD_CS_CRC_TIMEOUT){
            usb_dev_debug(req->dev," ERROR TD CRC/Timeout error\n");
        }
        if (tdend->ctrl & TD_CS_BITSTUFF){
            usb_dev_debug(req->dev," ERROR TD bitstuff error\n");
        }


        uhcihcd_removeTransferQH(qh);
        uhci_freeQH(qh);
        uhci_freeTDs(td);
        //usb_dev_debug(req->dev," TODO freeup queue of TD'd for this transfer.\n");
        //usb_dev_debug(req->dev," TODO toggle toggle bit for this endpoint\n");
        //uhcicd->curTransfer=0;

        log_msg("TODO check actual_xferlen by checking all TD's....\n");
        req->actual_xferlen = (1 + (tdend->ctrl & 0x3F));  //(11bit)

        if(req->xfer_cb_func){
            req->xfer_cb_func(req);
        }
    }else{
        //usb_dev_debug(req->dev,"TD_CS_ACTIVE = %#x\n",(tdend->ctrl & TD_CS_ACTIVE));
    }

}

USBRESULT uhcihcd_poll(usb_hcd_t *hcd) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    //adr_t ioregbase = priv->io_regbase;

    //log_msg("uhcihcd_poll %#lx\n",priv);

    for (int i=0; i<QSLOTS; i++){
        uhci_qh_t* qh = priv->queue[i];

        if(qh->element_link > 8){
            qh = (uhci_qh_t*) P2V(qh->element_link & UHCI_QH_PTR_MASK);
            while (qh!=NULL) {
                //log_msg("Check QH for completion : %#lx\n",qh);
                adr_t qhnext = qh->head_link & UHCI_QH_PTR_MASK;
                uhcihcd_poll_qh_xfer(priv,qh);


                if (qhnext>0) {
                    qh = (uhci_qh_t*) P2V(qhnext);
                }else {
                    qh=NULL;
                }
            }
        }
    }
    return USB_OK;
}


USBRESULT uhcihcd_submit_ctrl_xfer_request(struct usb_xfer_request *req,int waitforcompletion,int timeout){
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) req->dev->usb_hcd;
    adr_t ioregbase = priv->io_regbase;

    int packet_size = 8;
    uint8_t dev_address = req->dev->address;
    int direction = req->setup_data->bmRequestType >> 7;
    uint8_t tddirection = 0;
    if(direction){
        usb_debug("setup_data has Direction IN\n");
    }else{
        usb_debug("setup_data has Direction OUT\n");
    }

    log_msg("..packet_size=%d\n",packet_size);
    log_msg("..dev_address=%d\n",dev_address);
    log_msg("..tddirection=%d\n",tddirection);
    log_msg("..ioregbase=%#lx\n",ioregbase);

    struct USB20_control_setup_data *setup_data = req->setup_data;
    usb_debug("setup_data at %#lx\n",setup_data);

    uhci_qh_t *qh = uhci_allocQH();
    usb_debug("queue at %#lx\n",qh);

    //calc no of TD for xferlen
    int xferlen = req->xferlen;
    int tdcount = (xferlen+(packet_size-1)) / packet_size;
    usb_debug("RAW TD Count=%d for xferlen=%d\n",tdcount,xferlen);





    tdcount += 2; //add 1 for setup + 1 for ack.
    //struct uhci_transfer_descriptor *td = kmalloc(sizeof(struct uhci_transfer_descriptor)*tdcount,GFP_DMA16,NULL);
    uhci_td_t* td = uhci_allocTDs(tdcount);
    usb_debug("td at %#lx\n",td);

    uint8_t *retbuf = req->xferbuf;
    usb_debug("retbuf at %#lx\n",retbuf);

    qh->head_link = 0x00000001;   //terminate
    qh->element_link= V2P(td);      //queue element points to first td
    qh->tdhead = td;

    uint8_t toggle = 0;
    td[0].link = V2P(&td[1]) ;      //| 0x4; //bread first
    if(req->dev->speed == USB_SPEED_LOW){
        td[0].ctrl = (1<<26)  | (3<<27) | (0x80 << 16);  //islowspeed , errcount=3, active
    }else{
        td[0].ctrl = (3<<27) | (0x80 << 16);    //errcount=3, active
    }
    //td[0].token = (7<<21) | ((dev_address & 0x7F)<<8) | TOKEN_SETUP;
    td[0].token =         (7 << TD_TOK_MAXLEN_SHIFT) |
                          (toggle << TD_TOK_D_SHIFT) |
                          (0 << TD_TOK_ENDP_SHIFT) |
                          ((dev_address & 0x7F) << TD_TOK_DEVADDR_SHIFT) |
                        TOKEN_SETUP;
    td[0].buffer = V2P(setup_data);




    int i = 1;
    int t;
    int sz = xferlen;
    tddirection = direction ? TOKEN_IN : TOKEN_OUT;

    toggle ^= 1;
    while ((sz > 0) && (i<9)) {
        //log_msg("Handling TD %d \n",i);
        td[i].link = V2P(&td[i+1]) ;   //| 0x4; //bread first;
        td[i].ctrl = td[0].ctrl;
        t = ((sz <= packet_size) ? sz : packet_size);  //left to transfer in this td.
        //log_msg("Handling TD %d t=%d\n",i,t);
        //td[i].token = ((t-1)<<21) | ((i & 1) ? (1<<19) : 0) | ((dev_address & 0x7F)<<8) | TOKEN_IN;
        td[i].token =         ((t-1) << TD_TOK_MAXLEN_SHIFT) |
                              (toggle << TD_TOK_D_SHIFT) |
                              (0 << TD_TOK_ENDP_SHIFT) |
                              ((dev_address & 0x7F) << TD_TOK_DEVADDR_SHIFT) |
                tddirection;
        td[i].buffer = V2P(&retbuf[packet_size*(i-1)]);
        sz -= t;
        i++;
    }

    toggle = 1;
    tddirection = direction ? TOKEN_OUT : TOKEN_IN;
    td[i].link = 0x00000001;
    td[i].ctrl = td[0].ctrl | (1<<24);
    td[i].token = (0x7FF<<21) | (toggle<<TD_TOK_D_SHIFT) | ((dev_address & 0x7F)<<8) | tddirection;
    td[i].buffer = 0x00000000;
    i++; // for a total count

    // make sure status:int bit is clear
    io_outw(ioregbase+UHCI_REG_USBSTS, 1);
#ifdef UHCI_DEBUG
    uhci_debug_td_chain(V2P(qh),V2P(&td[0]));
#endif

    uhcihcd_insertTransferQH(qh,priv->queue[QCTRL]);
    uhci_debug_Q_chain(priv->queue[Q16],1);
    //PANIC("TODO Implement uhcihcd_submit_ctrl_xfer_request");

    int count = 0;
    if(waitforcompletion){
        while (!(io_inw(ioregbase+UHCI_REG_USBSTS) & 1) && (count<timeout)) {
            count++;
            mdelay(1);
        }
        if (count >= timeout) {
            usb_debug(" UHCI timed out.\n");
            goto error;
        }

        //PANIC("END");
        io_outw(ioregbase+UHCI_REG_USBSTS, 1);  // acknowledge the interrupt

        for (t=0; t<i; t++) {
            if (((td[t].ctrl & (0xFF<<16)) != 0)){
                usb_debug("td[%d] has error in reply\n",t);
                goto error;
            }
        }
        uint16_t actlen = 0;
        for (t=1; t<tdcount-1; t++) {
            actlen+= (1 + (td[t].ctrl & 0x3F));  //(11bit)
        }
        req->actual_xferlen=actlen;
        usb_debug(" Transfer success on IOC at timeout %d, actlen=%d \n",count,actlen);
#ifdef UHCI_DEBUG
        uhci_debug_td_chain(V2P(qh),V2P(&td[0]));
#endif
    }else{
        PANIC("NO support for async transfers yet!");
    }

    uhcihcd_removeTransferQH(qh);
    uhci_freeQH(qh);
    uhci_freeTDs(td);

    return USB_OK;

error:
    uhcihcd_removeTransferQH(qh);
    uhci_freeQH(qh);
    uhci_freeTDs(td);
    return USB_ERROR;
}

USBRESULT uhcihcd_submit_endp_xfer_request_sync(struct usb_xfer_request *req,int waitforcompletion,int timeout) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) req->dev->usb_hcd;
    adr_t ioregbase = priv->io_regbase;


    const struct USB20_endpoint_descriptor *endpoint_desc = req->endpoint_desc;
    int packet_size = endpoint_desc->wMaxPacketSize;


    //struct uhci_hcd_priv* priv = req->dev->pdev->priv;

    //struct uhci_hcd_priv* priv = (struct uhci_hcd_priv*) req->dev->usb_hcd;

    //replace with data from endpoint
    //int packet_size = 8;
    uint8_t dev_address = req->dev->address;
    uint8_t endp_address = endpoint_desc->bEndpointAddress & 0x0F;

    //struct USB20_control_setup_data *setup_data = req->setup_data;

    //usb_debug("setup_data at %#x\n",setup_data);

    struct uhci_qh *qh = uhci_allocQH();
    usb_debug("queue at %#x\n",qh);

    //calc no of TD for xferlen
    int xferlen = req->xferlen;
    //int tdcount=1;
    int tdcount = (xferlen+(packet_size-1)) / packet_size;
    usb_debug("RAW TD Count=%d for xferlen=%d\n",tdcount,xferlen);



    log_msg("..packet_size=%d\n",packet_size);
    log_msg("..dev_address=%d\n",dev_address);
    log_msg("..endp_address=%d\n",endp_address);
    //log_msg("..tddirection=%d\n",tddirection);
    log_msg("..ioregbase=%#lx\n",ioregbase);
    log_msg("..xferlen=%d\n",xferlen);
    log_msg("..tdcount=%d\n",tdcount);

    if (tdcount!=1) {
        PANIC("uhcihcd_submit_endp_xfer_request_sync cant handle tdcount > 1. Should it do that???");
    }

    //tdcount += 2; //add 1 for setup + 1 for ack.
    //struct uhci_td *td = kmalloc(sizeof(struct uhci_transfer_descriptor),GFP_DMA16,NULL);
    uhci_td_t* td = uhci_allocTDs(tdcount);
    usb_debug("td at %#x\n",td);

    uint8_t *retbuf = req->xferbuf;
    usb_debug("retbuf at %#x\n",retbuf);

    qh->head_link = 0x00000001;   //terminate
    qh->element_link = V2P(&td[0]);      //queue element points to first td
    qh->tdhead = td;


    uint16_t tdlen = (xferlen-1) & 0x7ff;
    uint8_t tdendpoint = req->endpoint_desc->bEndpointAddress & 0x0F;
    uint8_t tdaddr = req->dev->address;
    uint8_t packetType = 0;
    uint8_t tdtoggle = 0;
    if((req->endpoint_desc->bEndpointAddress >> 7) == USB_DIRECTION_IN){
        packetType = TOKEN_IN;
    }else{
        packetType = TOKEN_OUT;
    }

    usb_dev_debug3(req->dev,"ENDP xfer len=%d  endpoint=%d  addr=%d  type=0x%x  toggle=%d\n",tdlen,tdendpoint,tdaddr,packetType,tdtoggle);

    //req->dev->speed = USB_SPEED_LOW;

    td[0].link = 0x00000001;   //terminate

    if(req->dev->speed == USB_SPEED_LOW){
        usb_dev_debug(req->dev,"set td[0].ctrl with low speed\n");
//        td[0].ctrl = TD_CS_IOC | (1<<26)  | (3<<27) | (0x80 << 16);  //islowspeed , errcount=3, active
        td[0].ctrl = (1<<26)  | (3<<27) | (0x80 << 16);  //islowspeed , errcount=3, active
    }else{
        usb_dev_debug(req->dev,"set td[0].ctrl with full speed\n");
//        td[0].ctrl = TD_CS_IOC | (3<<27) | (0x80 << 16);    //errcount=3, active
        td[0].ctrl = (3<<27) | (0x80 << 16);    //errcount=3, active
    }
    td[0].token =         (tdlen << TD_TOK_MAXLEN_SHIFT) |
                          (tdtoggle << TD_TOK_D_SHIFT) |
                          (tdendpoint << TD_TOK_ENDP_SHIFT) |
                          (tdaddr << TD_TOK_DEVADDR_SHIFT) |
                          packetType;
    td[0].buffer = V2P(retbuf);


    // make sure status:int bit is clear
//    io_outw(regbase+UHCI_STATUS, 1);
#ifdef UHCI_DEBUG
    uhci_debug_td_chain(V2P(qh),V2P(&td[0]));
#endif

    //PANIC("TODO Implement uhcihcd_submit_endp_xfer_request_sync\n");
    //return 0;


    uhcihcd_insertTransferQH(qh,priv->queue[Q16]);
    uhci_debug_Q_chain(priv->queue[Q16],1);

    int count = 0;
    if(waitforcompletion){
        while ((td[0].ctrl & TD_CS_ACTIVE) && (count < (timeout*10))) {
            count++;
            mdelay(1);
        }
        if (count >= timeout) {
            usb_debug(" UHCI timed out.\n");
            goto error;
        }

        //for (t=0; t<1; t++) {
        if (((td[0].ctrl & (0xFF<<16)) != 0)){
            usb_debug("td[%d] has error in reply at timeout %d,\n",0,count);

#ifdef UHCI_DEBUG
            uhci_debug_td_chain(V2P(qh),V2P(&td[0]));
            usb_debug("td[%d] active=%d\n",0,td[0].ctrl & TD_CS_ACTIVE);
            usb_debug("td[%d] TD_CS_NAK=%d\n",0,td[0].ctrl & TD_CS_NAK);
            usb_debug("td[%d] TD_CS_STALLED=%d\n",0,td[0].ctrl & TD_CS_STALLED);
            usb_debug("td[%d] TD_CS_DATABUFFER=%d\n",0,td[0].ctrl & TD_CS_DATABUFFER);
            usb_debug("td[%d] TD_CS_BABBLE=%d\n",0,td[0].ctrl & TD_CS_BABBLE);
            usb_debug("td[%d] TD_CS_CRC_TIMEOUT=%#x\n",0,td[0].ctrl & TD_CS_CRC_TIMEOUT);
            usb_debug("td[%d] TD_CS_BITSTUFF=%d\n",0,td[0].ctrl & TD_CS_BITSTUFF);
#endif
            goto error;
        }
        //}
        uint16_t actlen = 0;
        //for (t=1; t<tdcount-1; t++) {
        actlen+= (1 + (td[0].ctrl & 0x3F));  //(11bit)
        //}
        req->actual_xferlen=actlen;
        usb_debug(" Transfer success on IOC at timeout %d, actlen=%d \n",count,actlen);

    }else{
        PANIC("NO support for async transfers yet!");
    }

    uhcihcd_removeTransferQH(qh);
    uhci_freeQH(qh);
    uhci_freeTDs(td);
    return USB_OK;

error:
    uhcihcd_removeTransferQH(qh);
    uhci_freeQH(qh);
    uhci_freeTDs(td);
    return USB_ERROR;

}


USBRESULT uhcihcd_submit_endp_xfer_request(struct usb_xfer_request *req){
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) req->dev->usb_hcd;
    adr_t ioregbase = priv->io_regbase;


    const struct USB20_endpoint_descriptor *endpoint_desc = req->endpoint_desc;
    int packet_size = endpoint_desc->wMaxPacketSize;

    struct uhci_qh *qh = uhci_allocQH();
    qh->head_link = 0x00000001;   //terminate
    usb_debug("queue at %#x\n",qh);

    //calc no of TD for xferlen
    int xferlen = req->xferlen;
    int tdcount = (xferlen+(packet_size-1)) / packet_size;
    usb_debug("RAW TD Count=%d for xferlen=%d\n",tdcount,xferlen);

    uint8_t dev_address = req->dev->address;
    uint8_t endp_address = endpoint_desc->bEndpointAddress & 0x0F;

    log_msg("..packet_size=%d\n",packet_size);
    log_msg("..dev_address=%d\n",dev_address);
    log_msg("..endp_address=%d\n",endp_address);
    //log_msg("..tddirection=%d\n",tddirection);
    log_msg("..ioregbase=%#lx\n",ioregbase);
    log_msg("..xferlen=%d\n",xferlen);
    log_msg("..tdcount=%d\n",tdcount);

    if (tdcount!=1) {
        PANIC("uhcihcd_submit_endp_xfer_request_sync cant handle tdcount > 1. Should it do that???");
    }
    uhci_td_t* td = uhci_allocTDs(tdcount);
    usb_debug("td at %#lx\n",td);

    uint8_t *retbuf = req->xferbuf;
    usb_debug("retbuf at %#lx\n",retbuf);
    qh->element_link = V2P(&td[0]);      //queue element points to first td
    qh->tdhead = td;



    uint16_t tdlen = (req->xferlen-1) & 0x7ff;
    uint8_t tdendpoint = req->endpoint_desc->bEndpointAddress & 0x0F;
    uint8_t tdaddr = req->dev->address;
    uint8_t packetType = 0;
    uint8_t tdtoggle = 0;
    if((req->endpoint_desc->bEndpointAddress >> 7) == USB_DIRECTION_IN){
        packetType = TOKEN_IN;
    }else{
        packetType = TOKEN_OUT;
    }

    usb_dev_debug3(req->dev,"ENDP xfer len=%d  endpoint=%d  addr=%d  type=0x%x  toggle=%d\n",tdlen,tdendpoint,tdaddr,packetType,tdtoggle);

    //req->dev->speed = USB_SPEED_LOW;

    td[0].link = 0x00000001;   //terminate
    if(req->dev->speed == USB_SPEED_LOW){
//        usb_dev_debug3(req->dev,"set td[0].ctrl with low speed\n");
//        td[0].ctrl = TD_CS_IOC | (1<<26)  | (3<<27) | (0x80 << 16);  //islowspeed , errcount=3, active
        td[0].ctrl = (1<<26)  | (3<<27) | (0x80 << 16);  //islowspeed , errcount=3, active
    }else{
//        usb_dev_debug3(req->dev,"set td[0].ctrl with full speed\n");
//        td[0].ctrl = TD_CS_IOC | (3<<27) | (0x80 << 16);    //errcount=3, active
        td[0].ctrl = (3<<27) | (0x80 << 16);    //errcount=3, active
    }
    td[0].token =         (tdlen << TD_TOK_MAXLEN_SHIFT) |
                          (tdtoggle << TD_TOK_D_SHIFT) |
                          (tdendpoint << TD_TOK_ENDP_SHIFT) |
                          (tdaddr << TD_TOK_DEVADDR_SHIFT) |
                          packetType;
    td[0].buffer = V2P(retbuf);
    //td[0].tdnext = 0;

    //0xE08169
    //0000 0000 1110 0000 1000 0001 0110 1001
    //0:7 = 69   (IN)
    //14:8 = 1   (addr)
    //15:18 = 1   (endp)
    //19 = 0   (toggle)
    //20 = 0   (reserve)
    //21:31 = 7

    // make sure status:int bit is clear
//    io_outw(regbase+UHCI_STATUS, 1);
#ifdef UHCI_DEBUG
    //uhci_debug_td_chain(V2P(qh),V2P(&td[0]));
#endif
    qh->xferReq = req;
    //qh->tdhead = &td[0];
    //priv->curTransfer = qh;

    //uhci_insertTransferQH(qh,priv->queue[QINTR]);
    uhcihcd_insertTransferQH(qh,priv->queue[Q128]);
    uhci_debug_Q_chain(priv->queue[Q128],1);

    return USB_OK;
}



RESULT uhcihcd_global_reset(adr_t ioregbase) {
    // do a global reset (5 times 10ms each)
    // Global Reset (USBCMD.GRESET): resets the controller silicon and
    // pulses reset to whatever's attached downstream. The UHCI spec (and
    // the wider USB spec's "TDRSTR" parameter) calls for holding this for at least 10ms;
    usb_info("Start UHCI reset!\n");
    for (int i=0; i<5; i++) {
        io_outw(ioregbase+UHCI_REG_USBCMD, UHCI_CMD_GRESET);
        mdelay(USB_TDRST);
        io_outw(ioregbase+UHCI_REG_USBCMD, 0x0000);
    }
    mdelay(USB_TRSTRCY);
    usb_debug("UHCI reset Done!\n");

    //Test if controller contains valid data
    // does the command register contain its default value of 0x0000 ?
    if (io_inw(ioregbase+UHCI_REG_USBCMD) != 0x0000)
        return ERROR;
    // does the status register contain its default value of 0x0020 ?
    if (io_inw(ioregbase+UHCI_REG_USBSTS) != 0x0020)
        return ERROR;
    // The status register is write clear, let's clear it out.
    io_outw(ioregbase+UHCI_REG_USBSTS, 0x00FF);
    // does the SOF register contain its default value of 0x40
    if (io_inw(ioregbase+UHCI_REG_SOFMOD) != 0x40)
        return ERROR;

    usb_debug("UHCI test Done!\n");


    // Host Controller Reset (USBCMD.HCRESET): resets the controller's
    // internal state machine (frame list pointer, FIFOs, ...) without
    // pulsing the bus. The controller clears this bit itself when done;
    // Set bit 1 in the Command register, after a specified time the controller should reset it to 0.
    io_outw(ioregbase+UHCI_REG_USBCMD, UHCI_CMD_HCRESET);
    mdelay(10);
    if (io_inw(ioregbase+UHCI_REG_USBCMD) & UHCI_CMD_HCRESET)
        return ERROR;

    // if we get here, we have a valid UHCI controller, so set it up
    usb_debug("Sucess, we have ourself an UHCI Controller\n");

    return OK;
}

//Init stackframe and setup all QueueHeads
RESULT uhcihcd_setup_schedule(uhci_hcd_device_t* priv) {
    //pcidevice_t* pcidev = (pcidevice_t*) dev;
    //uhci_hcd_device_t* priv = dev->priv;

    page_t *stackpage = page_alloc_pages(GFP_DMA16,0);   //4096 bytes
    priv->stackframe = (void*)PAGE2VIRT(stackpage);
    usb_debug("Setup Schedule for stackframe at %#lx\n",priv->stackframe);


    /*for (int i=0; i<8; i++){
        priv->queue[i] = uhci_alloc_qh();
        usb_debug(" ALLOCATE QH[%d] at %#lx\n",i,priv->queue[i] );
    }*/

    // set up Top level queue heads
    void* qheadlistptr = kmalloc_aligned(8*sizeof(uhci_qh_t), 16,GFP_DMA16);
    usb_debug(" ALLOCATE qh[0] at %#lx\n", qheadlistptr);
    //priv->queue[0] = qheadlistptr;
    uhci_qh_t* qhptr = (uhci_qh_t*) qheadlistptr;
    for (int i=0; i<QSLOTS; i++){
        priv->queue[i] = &qhptr[i];
        priv->queue[i]->head_link = UHCI_QH_T;
        priv->queue[i]->element_link = UHCI_QH_T;
        priv->queue[i]->xferReq=0;
        usb_debug("   qh[%d] allocated at %#lx\n",i,priv->queue[i] );
    }


    // Link them in chain order Q2->Q1->QINTR->QCTRL->QBULK
    //Skip last one, since it willlink to 0x0000001 (T)
    for (int i=0; i<(QSLOTS-1); i++){
        priv->queue[i]->head_link = V2P(priv->queue[i+1]) | UHCI_QH_Q;

    }


    // set up an empty stack frame
    for (int i=0; i<1024; i++){
        priv->stackframe[i] = V2P(priv->queue[Q1]) | UHCI_QH_Q;
        if(i%2==0){
            priv->stackframe[i] = V2P(priv->queue[Q2]) | UHCI_QH_Q;
        }
        if(i%4==0){
            priv->stackframe[i] = V2P(priv->queue[Q4]) | UHCI_QH_Q;
        }
        if(i%8==0){
            priv->stackframe[i] = V2P(priv->queue[Q8]) | UHCI_QH_Q;
        }
        if(i%16==0){
            priv->stackframe[i] = V2P(priv->queue[Q16]) | UHCI_QH_Q;
        }
        if(i%32==0){
            priv->stackframe[i] = V2P(priv->queue[Q32]) | UHCI_QH_Q;
        }
        if(i%64==0){
            priv->stackframe[i] = V2P(priv->queue[Q64]) | UHCI_QH_Q;
        }
        if(i%128==0){
            priv->stackframe[i] = V2P(priv->queue[Q128]) | UHCI_QH_Q;
        }
    }

    return OK;
}


int uhcihcd_setup(usb_hcd_t *hcd) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    adr_t ioregbase = priv->io_regbase;

    log_msg("hcihcd_start! TODO move all schedule stup here!\n");
        if(! (uhcihcd_global_reset(ioregbase)==OK)){
        PANIC("Unable to reset UHCI Controller?\n");
    }

    if(! (uhcihcd_setup_schedule(priv)==OK)){
        PANIC("Unable to setup UHCI Schedule\n");
    }

    // set the Host Controllers schedule
    io_outd(ioregbase+UHCI_REG_FRBASEADD, V2P(priv->stackframe)); // physical address
    io_outw(ioregbase+UHCI_REG_FRNUM, 0);                // start at frame 0
    io_outb(ioregbase+UHCI_REG_SOFMOD, 0x40);            // start of frame to default,standard 12MHz SOF timing value
    io_outw(ioregbase+UHCI_REG_USBINTR, 0x0000);         // no interrupts, means polling
    io_outw(ioregbase+UHCI_REG_USBSTS, 0xFFFF);              // Clear any status bits.




    //dev->initialized = 1;
    priv->usb_hcd.hcdstate = HCD_STATE_SETUP;
    return 0;
}

int uhcihcd_start(usb_hcd_t *hcd) {
    uhci_hcd_device_t* priv = (uhci_hcd_device_t*) hcd;
    adr_t ioregbase = priv->io_regbase;
    //start controller
    //bit0 (RS) = Run/Stop=1 (execute the frame list),
    //bit6 (CF)= Configure Flag=1 (informs the controller/root hub that software has finished configuring it — required by spec before ports will function normally)
    //bit7 (MAXP) = Max Packet=1 (allow 64-byte packets; harmless for low-speed/8-byte-max devices, needed for full-speed ones)
    io_outw(ioregbase+UHCI_REG_USBCMD, (1<<7) | (1<<6) | (1<<0));

    priv->usb_hcd.hcdstate = HCD_STATE_STARTED;
    return 0;
}

int uhcihcd_stop(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}

int uhcihcd_release(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}


int uhcihcd_pci_probe(device_t *dev) {
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("hcihcd_pci_probe\n");
    uhci_hcd_device_t* priv = kmalloc(sizeof(uhci_hcd_device_t),0);
    dev->priv = priv;

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "hcihcd PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    if (pcidev->bar[4].bartype != BARTYPE_IO) {
        PANIC("UHCI ONLY SUPPORTS I/O BAR");
    }
    log_msg("BAR 4 (I/O)   = 0x%lx \n",pcidev->bar[4].addr);
    priv->io_regbase=pcidev->bar[4].addr;
    priv->mmio_regbase=0; //disable
    //adr_t ioregbase = priv->io_regbase;

    //test USB HCD Version från PCI config extended field 0x60
    uint8_t usbversion = pci_config_read8(pcidev,PCICONFIG_USB_EXT_SBRN); //0x60, USB version of UHCI=0x10, EHCI=0x20, xHCI=0x30
    log_msg("usbversion= %#x\n",usbversion);
    if(usbversion!=0x10){
        PANIC("STRANGE USB VERSION FOR UHCI CONTROLLER");
        return ERROR;
    }

    //Enable I/O + bumastering
    uint16_t command = pci_config_read16(pcidev,PCICONFIG_COMMAND);
    log_msg("command= %#x\n",command);
    command = command | 0x07;       // bit0=I/O Space, bit1=Memory Space, bit2=Bus Master
    pci_config_write16(pcidev,PCICONFIG_COMMAND,command);


    //Disable legacy support for Keyboard
    uint32_t legacydefault = pci_config_read32(pcidev,PCICONFIG_USB_EXT_USBLEGSUP);
    log_msg("Legacy support = %#x\n",legacydefault);
    pci_config_write16(pcidev,PCICONFIG_USB_EXT_USBLEGSUP,0x8F00);  /* disable all SMI sources, clear their status */


    priv->usb_hcd.name = "UHCI";
    priv->usb_hcd.setup=uhcihcd_setup;
    priv->usb_hcd.start=uhcihcd_start;
    priv->usb_hcd.stop=uhcihcd_stop;
    priv->usb_hcd.release=uhcihcd_release;
    priv->usb_hcd.poll=uhcihcd_poll;

    priv->usb_hcd.hcd_roothub_is_port_present = uhcihcd_roothub_is_port_present;
    priv->usb_hcd.hcd_roothub_port_reset = uhcihcd_roothub_port_reset;
    priv->usb_hcd.hcd_roothub_is_port_device_present=uhcihcd_roothub_is_port_device_present;
    priv->usb_hcd.hcd_roothub_port_speed=uhcihcd_roothub_port_speed;

    priv->usb_hcd.submit_ctrl_xfer_request=uhcihcd_submit_ctrl_xfer_request;
    priv->usb_hcd.submit_endp_xfer_request_sync=uhcihcd_submit_endp_xfer_request_sync; //TODO, change to interrupt driver
    priv->usb_hcd.submit_endp_xfer_request=uhcihcd_submit_endp_xfer_request;


    priv->usb_hcd.hcdstate = HCD_STATE_UNDEFINED;

    usb_debug("UHCI PROBED!\n" );

    /*

    if(! (uhcihcd_global_reset(ioregbase)==OK)){
        PANIC("Unable to reset UHCI Controller?\n");
    }

    if(! (uhcihcd_setup_schedule(dev)==OK)){
        PANIC("Unable to setup UHCI Schedule\n");
    }

    // set the Host Controllers schedule
    io_outd(ioregbase+UHCI_REG_FRBASEADD, V2P(priv->stackframe)); // physical address
    io_outw(ioregbase+UHCI_REG_FRNUM, 0);                // start at frame 0
    io_outb(ioregbase+UHCI_REG_SOFMOD, 0x40);            // start of frame to default,standard 12MHz SOF timing value
    io_outw(ioregbase+UHCI_REG_USBINTR, 0x0000);         // no interrupts, means polling
    io_outw(ioregbase+UHCI_REG_USBSTS, 0xFFFF);              // Clear any status bits.

    //start controller
    //bit0 (RS) = Run/Stop=1 (execute the frame list),
    //bit6 (CF)= Configure Flag=1 (informs the controller/root hub that software has finished configuring it — required by spec before ports will function normally)
    //bit7 (MAXP) = Max Packet=1 (allow 64-byte packets; harmless for low-speed/8-byte-max devices, needed for full-speed ones)
    io_outw(ioregbase+UHCI_REG_USBCMD, (1<<7) | (1<<6) | (1<<0));



    priv->usb_hcd.hcd_roothub_is_port_present = uhcihcd_roothub_is_port_present;
    priv->usb_hcd.hcd_roothub_port_reset = uhcihcd_roothub_port_reset;
    priv->usb_hcd.hcd_roothub_is_port_device_present=uhcihcd_roothub_is_port_device_present;
    priv->usb_hcd.hcd_roothub_port_speed=uhcihcd_roothub_port_speed;

    priv->usb_hcd.submit_ctrl_xfer_request=uhcihcd_submit_ctrl_xfer_request;
    priv->usb_hcd.submit_endp_xfer_request_sync=uhcihcd_submit_endp_xfer_request_sync; //TODO, change to interrupt driver
    priv->usb_hcd.submit_endp_xfer_request=uhcihcd_submit_endp_xfer_request;


*/



    //readback stackfram
    //uint32_t framebase = io_ind(ioregbase+UHCI_REG_FRBASEADD);
    //log_msg("readback Stackframe at %#lx\n",framebase);

    /*
    log_msg("Test some alignes\n");

    void* a1 = kmalloc_aligned(40, 16,GFP_DMA16);
    void* a2 = kmalloc_aligned(40, 16,GFP_DMA16);
    void* a3 = kmalloc_aligned(40, 16,GFP_DMA16);

    log_msg("a1=0x%lx\n",a1);
    log_msg("a2=0x%lx\n",a2);
    log_msg("a3=0x%lx\n",a3);

    kfree_aligned(a1);
    kfree_aligned(a2);
    kfree_aligned(a3);
    */

    dev->initialized = 1;

    return 0;
}

int uhcihcd_pci_attach(device_t *dev) {
    //pcidevice_t* pcidev = (pcidevice_t*) dev;
    uhci_hcd_device_t* priv = dev->priv;
    if (dev->initialized) {
        log_msg("hcihcd_pci_attach\n");
        priv->usb_hcd.hcdstate = HCD_STATE_INIT;
        usb_attach_hcd(&priv->usb_hcd);
    }else {
        log_msg("Device is not initialized, skip attach\n");
    }
    return 0;
}



struct pcidriver uhcihcd_pci_driver = {
    .driver.name = "USB Universal Host Controller Interface (UHCI)",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = uhcihcd_pci_probe,
    .driver.attach = uhcihcd_pci_attach,
    .idents = (void *) &uhcihcd_pci_idents,
};

REGISTER_DRIVER(uhcihcddriver, "uhcihcd",&uhcihcd_pci_driver);

