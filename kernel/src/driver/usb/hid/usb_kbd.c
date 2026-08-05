#include "driver/usb/usb.h"
#include "mm.h"
//#include "usb_hcd.h"
#include "delay.h"
#include "error.h"
#include "log.h"

/** USB keyboard control block  */
struct usbkbd
{
    BOOL initialized;           /**< Has the keyboard structure been  initialized?  */
    BOOL attached;              /**< Is the keyboard attached?          */
//    uchar iflags;               /**< Input flags                        */
//    semaphore isema;            /**< Count of input bytes ready         */
//    ushort istart;              /**< Index of first byte ready          */
//    ushort icount;              /**< Bytes in input buffer              */
//    uchar in[USBKBD_IBLEN];     /**< Input buffer                       */
//    struct usb_xfer_request *intr;  /**< USB transfer request for keyboard interrupt data  */
//    uchar recent_usage_ids[6];  /**< Keys that were reported as "down" in
//                                     the last report  */

    struct usb_xfer_request *xferRequest;  /**< USB transfer request for keyboard interrupt data  */

};



void usb_kbd_xfercbfunc(struct usb_xfer_request *req){
    //usb_dev_debug(req->dev,"KBD Xfer completed with status = %d\n",req->status);
    //usb_dev_debug(req->dev,"KBD Xfer completed with actlen=%d xferlen=%d\n",req->actual_xferlen,req->xferlen);

    int l = req->actual_xferlen;
    log_msg("USB KBD: CALLBACK GOT DATA LEN = %d \n",l);
    log_msg("USB KBD: ----------------------- \n",l);
    log_msg("         ",l);
    for(int i=0;(i<l) && (i<16);i++){
        log_msg("%#02x ",((uint8_t*)req->xferbuf)[i]);
    }
    log_msg("\n");
    log_msg("USB KBD: ----------------------- \n",l);

    ///send again
    req->status = -999;
    //USBRESULT res = usb_submit_xfer_request(req);
    usb_submit_xfer_request(req);

}


USBRESULT usb_kbd_probe_attach(struct usb_device *dev,int doProbe, int doAttach){
    USBRESULT res;
    struct USB20_interface_descriptor *kbd_interface;
    const struct USB20_endpoint_descriptor *in_interrupt_endpoint;
    struct usbkbd *kbd=NULL;



    usb_dev_debug(dev,"Attempting to bind USB device (%s %s: address %u)\n",dev->manufacturer, dev->product, dev->address);


    // ! HID class is per-interface and cannot be in the device descriptor.
    if (USB_CLASS_CODE_INTERFACE_SPECIFIC != dev->descriptor.bDeviceClass) {
        usb_dev_debug(dev,"Device does not have interface specific class: can't have any HID interfaces!\n");
        return USB_STATUS_DEVICE_UNSUPPORTED;
    }

    usb_dev_debug(dev,"Looking for HID interface supporting keyboard boot protocol\n");
    kbd_interface = NULL;
    for (int i = 0; i < dev->config_descriptor->bNumInterfaces; i++) {
        struct USB20_interface_descriptor *interface = dev->interfaces[i];

        usb_dev_debug(dev,"Examining interface (index=%u, bInterfaceNumber=%u) (class=0x%04x, subclass=0x%04x)\n", i, interface->bInterfaceNumber,interface->bInterfaceClass,interface->bInterfaceSubClass);
        if (USB_CLASS_CODE_HID != interface->bInterfaceClass) {
            usb_dev_debug(dev,"Not a HID interface\n");
            continue;
        }

        // Check if the keyboard boot protocol is supported
        if (HID_SUBCLASS_BOOT != interface->bInterfaceSubClass || HID_BOOT_PROTOCOL_KEYBOARD != interface->bInterfaceProtocol) {
            usb_dev_debug(dev,"Interface does not support keyboard boot protocol\n");
            continue;
        }

        // Find the IN interrupt endpoint.
        in_interrupt_endpoint = NULL;
        for (int j = 0; j < interface->bNumEndpoints; j++) {
            if ((dev->endpoints[i][j]->bmAttributes & 0x3) == USB_TRANSFER_TYPE_INTERRUPT
                && (dev->endpoints[i][j]->bEndpointAddress >> 7) == USB_DIRECTION_IN) {
                in_interrupt_endpoint = dev->endpoints[i][j];
                break;
            }
        }
        if (in_interrupt_endpoint == NULL ) {
            usb_dev_debug(dev,"Interface has no IN interrupt endpoint\n");
            continue;
        }

        usb_dev_debug(dev, "Interface is supported at bEndpointAddress=%d\n",(in_interrupt_endpoint->bEndpointAddress & 0xF) );
        usb_dev_debug(dev, "                          wMaxPacketSize=%d\n",(in_interrupt_endpoint->wMaxPacketSize ) );
        usb_dev_debug(dev, "                          bInterval=%d\n",(in_interrupt_endpoint->bInterval) );
        kbd_interface = interface;
        break;
    }

    if (kbd_interface == NULL) {
        usb_dev_debug(dev,"No HID interface with keyboard boot protocol found\n");
        return USB_STATUS_DEVICE_UNSUPPORTED;
    }



    if(!doAttach){
        //end here just do the probe..
        return USB_OK;
    }

    //PANIC("TODO Implement usb_kbd_probe_attach");
    //return 0;

    kbd = kmalloc(sizeof(struct usbkbd),0);
    kbd->initialized = TRUE;



    // Put keyboard in boot protocol mode, not report mode.
    usb_dev_debug(dev,"Placing keyboard in boot protocol mode\n");
    res=usb_control_msg(dev, NULL, HID_REQUEST_SET_PROTOCOL,
                                 USB_BMREQUESTTYPE_TYPE_CLASS |
                                 USB_BMREQUESTTYPE_DIR_OUT |
                                 USB_BMREQUESTTYPE_RECIPIENT_INTERFACE,
                                 HID_BOOT_PROTOCOL,
//                                 1,    //report protocol
                                 kbd_interface->bInterfaceNumber, NULL, 0);
    if (res != USB_OK) {
//            usb_dev_debug(dev,"Failed to place keyboard in boot protocol mode: %s",usb_status_string(status));
        usb_dev_debug(dev,"Failed to place keyboard in boot protocol mode: %d\n",res);
        res=USB_STATUS_DEVICE_UNSUPPORTED;
        goto error;
    }
    mdelay(1);


    // Set keyboard Idle mode.
    usb_dev_debug(dev,">>>>>>>>>>>>>>>>>>>>>>>Keybord Set IDLE\n");
    res=usb_control_msg(dev, NULL, HID_REQUEST_SET_IDLE,
                        USB_BMREQUESTTYPE_TYPE_CLASS |
                        USB_BMREQUESTTYPE_DIR_OUT |
                        USB_BMREQUESTTYPE_RECIPIENT_INTERFACE,
                        0x0000,    //indefinitely & all reports
                        kbd_interface->bInterfaceNumber, NULL, 0);
    if (res != USB_OK) {
//            usb_dev_debug(dev,"Failed to place keyboard in boot protocol mode: %s",usb_status_string(status));
        usb_dev_debug(dev,"Failed to set keyboard idle mode: %d\n",res);
        res=USB_STATUS_DEVICE_UNSUPPORTED;
        goto error;
    }
    mdelay(1);



    dev->priv = kbd;

    // Initialize and submit the transfer request to the keyboard's IN interrupt
    // * endpoint.  When this request completes, usbKbdInterrupt() will be called;
    // * ordinarily this happens when a key is pressed or released, thereby
    // * causing new data to be sent.

    kbd->xferRequest = usb_alloc_xfer_request(8);
    kbd->xferRequest->dev = dev;
    kbd->xferRequest->endpoint_desc = in_interrupt_endpoint;
    //kbd->xferRequest->completion_cb_func = usbKbdInterrupt;
    kbd->xferRequest->priv = kbd;
    kbd->xferRequest->xferFlags = USB_XFERFLAG_ASYNC;
    kbd->xferRequest->xfer_cb_func = usb_kbd_xfercbfunc;

    mdelay(1);

    log_msg("---------------------------------------------------------------------------\n");
    log_msg("---------------------------------------------------------------------------\n");
    log_msg("---------------   ASK KBD FOR DATA                            -------------\n");
    log_msg("---------------------------------------------------------------------------\n");
    log_msg("---------------------------------------------------------------------------\n");

    usb_dev_debug(dev,"Asking keyboard for data\n");
    if((res=usb_submit_xfer_request(kbd->xferRequest))!= USB_OK) {
        usb_dev_debug(dev,"Failed reading data\n");
        goto error;
    }
    kbd->attached = TRUE;

    return USB_OK;

error:
    if(kbd){
        usb_free_xfer_request(kbd->xferRequest);
        kfree(kbd);
    }
    return res;

}

USBRESULT usb_kbd_probe(struct usb_device *dev){
    return usb_kbd_probe_attach(dev,1, 0);
}

USBRESULT usb_kbd_bind(struct usb_device *dev){
    return usb_kbd_probe_attach(dev,1, 1);
}

void usb_kbd_unbind(struct usb_device *dev){

}

struct usb_device_driver usb_kbd_driver = {
        .name = "Generic USB Keyboard driver",
        .probe = usb_kbd_probe,
        .bind_device = usb_kbd_bind,
        .unbind_device = usb_kbd_unbind,
};

/*
void usb_kbd_init(void){
    usb_register_driver(&usb_kbd_driver);
}*/

REGISTER_USB_DRIVER(usbkbddriver, "usbkbd_generic",&usb_kbd_driver);
