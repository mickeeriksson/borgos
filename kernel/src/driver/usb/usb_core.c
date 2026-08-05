#include "log.h"
#include "error.h"
#include "driver/usb/usb.h"
#include "mm.h"
#include "unicode.h"
#include <string.h>

#define MAX_HCD_DEVS 8

usb_hcd_t *hcd_devices[MAX_HCD_DEVS] = {0,0,0,0,0,0,0,0};


//replace this with some routine from hcd, that keep tracks of attached devices and free adressess on bus.
uint8_t nextUsbAdress = 1;


/** Table of USB device drivers that have been registered with the USB core. */
struct usb_device_driver *usb_device_drivers[64];

/** Number of currently registered USB device drivers.  There is currently no
 * way to remove a registered device driver; hence this can only increase.  */
static int usb_num_device_drivers = 0;

void usb_register_driver(struct usb_device_driver *driver){
    usb_device_drivers[usb_num_device_drivers++] = driver;
    usb_info("Registered %s\n", driver->name);
}


void usb_attach_hcd(usb_hcd_t *hcd) {
    //Simple database of all HCD's
    for (int i=0;i<MAX_HCD_DEVS;i++) {
        if (hcd_devices[i]==0) {
            log_msg("Attaching %s\n", hcd->name);
            hcd_devices[i]=hcd;
            return;
        }
    }
}

usb_device_t* usb_alloc_device(void)
{
    usb_device_t *devx = kmalloc(sizeof(usb_device_t),0);
    devx->manufacturer = kmalloc(USB_MAXSTRINGLEN,0);
    devx->product = kmalloc(USB_MAXSTRINGLEN,0);

    devx->isRootHub = 0;
    devx->address = 0;
    devx->manufacturer[0]='\0';
    devx->product[0]='\0';
    devx->config_descriptor=NULL;
    return devx;
}


struct usb_xfer_request* usb_alloc_xfer_request(uint16_t bufsize)
{
    struct usb_xfer_request *req;

    req = kmalloc(sizeof(struct usb_xfer_request),0);
    memset(req,0,sizeof(struct usb_xfer_request));
    if (req == 0){
        PANIC("usb_alloc_xfer_request");
        return NULL;
    }
    //usb_init_xfer_request(req);
    req->xferbuf = kmalloc(bufsize,GFP_DMA16);
    req->setup_data = kmalloc(sizeof(struct USB20_control_setup_data),GFP_DMA16);
    memset(req->xferbuf,0,bufsize);
    memset(req->setup_data,0,sizeof(struct USB20_control_setup_data));

    req->xferlen = bufsize;
    usb_debug("alloc xfer_request @ %#x with buf @ %#x\n",req,req->xferbuf);
    return req;
}

void usb_free_xfer_request(struct usb_xfer_request* req)
{
    kfree(req->setup_data);
    if(req->xferbuf){
        kfree(req->xferbuf);
    }
    kfree(req);
}

USBRESULT usb_submit_xfer_request(struct usb_xfer_request *req){
    usb_debug("submit xfer_request @ %#x \n",req);

    //DO only sync requests for now....
    //uhci_submit_xfer_request(req,1,2000);

#ifdef USB_DEBUG
    enum usb_transfer_type type;
    enum usb_direction dir;
    if (req->endpoint_desc){
        type = req->endpoint_desc->bmAttributes & 0x3;
        dir = req->endpoint_desc->bEndpointAddress >> 7;
    }else{
        type = USB_TRANSFER_TYPE_CONTROL;
        dir = req->setup_data->bmRequestType >> 7;
    }
    if(dir){
        //dummy to use dir....
    }
    usb_dev_debug3(req->dev, "Submitting xfer request (%u bytes, type=%s, dir=%s)\n",req->xferlen,usb_transfer_type_to_string(type),usb_direction_to_string(dir));
    if (type == USB_TRANSFER_TYPE_CONTROL)
    {
        usb_dev_debug(req->dev, "Control message: {.bmRequestType=0x%02x, "
                                ".bRequest=0x%02x, wValue=0x%04x, wIndex=0x%04x, wLength=0x%04x}\n",
                      req->setup_data->bmRequestType,
                      req->setup_data->bRequest,
                      req->setup_data->wValue,
                      req->setup_data->wIndex,
                      req->setup_data->wLength);

    }
#endif
    //PANIC("TODO Implement usb_submit_xfer_request");

    struct usb_hcd* usbhcd = req->dev->usb_hcd;

    if(req->endpoint_desc){
        //return usbhcd->submit_endp_xfer_request(req,1,2000);
        return usbhcd->submit_endp_xfer_request(req);  //ASYNC
    }else{
        return usbhcd->submit_ctrl_xfer_request(req,1,2000); //SYNC
    }
    return USB_OK;
}


USBRESULT usb_control_msg(usb_device_t *dev,
                const struct USB20_endpoint_descriptor *endpoint_desc,
                uint8_t bRequest, uint8_t bmRequestType,
                uint16_t wValue, uint16_t wIndex, void *data, uint16_t wLength)
{
    USBRESULT res;

    struct usb_xfer_request *req;
    req = usb_alloc_xfer_request(wLength);
    //log_msg("alloc xfer_request at %#lx\n",req);

    req->dev = dev;
    req->endpoint_desc = endpoint_desc;
    //req->recvbuf = data;  // no need copy is done i usb_core later in this method....
    req->xferlen = wLength;
    req->setup_data->bmRequestType = bmRequestType;
    req->setup_data->bRequest = bRequest;
    req->setup_data->wValue = wValue;
    req->setup_data->wIndex = wIndex;
    req->setup_data->wLength = wLength;
    //req->completion_cb_func = signal_control_msg_done;
    //req->private = (void*)sem;
    res = usb_submit_xfer_request(req);
    if(res!=OK){
        PANIC("usb_control_msg");
        goto error;
    }

    res = req->status;
    if(res!=OK){
        PANIC("usb_control_msg");
        goto error;
    }

    if (req->actual_xferlen != req->xferlen)
    {
        //status = USB_STATUS_INVALID_DATA;
        //req->dev->error_count++;
        //req->dev->last_error = status;
        PANIC("usb_control_msg actual_xferlen differs");
    }
    //copy bytes from reqbuffer to *data
    memcpy(data,req->xferbuf,wLength);


    // if (status == USB_STATUS_SUCCESS)
    // {
    //     // Wait for transfer to complete (or fail).
    //     wait(sem);
    //     status = req->status;
    //
    //     // Force error if actual size was not the same as requested size.
    //     if (status == USB_STATUS_SUCCESS && req->actual_size != req->size)
    //     {
    //         status = USB_STATUS_INVALID_DATA;
    //         req->dev->error_count++;
    //         req->dev->last_error = status;
    //     }
    // }
    // usb_free_xfer_request(req);
    // semfree(sem);
    // return status;

    usb_free_xfer_request(req);
    return res;
    error:
        usb_free_xfer_request(req);
    return USB_ERROR;
}


USBRESULT usb_get_descriptor(struct usb_device *dev, uint8_t bRequest, uint8_t bmRequestType,
                       uint16_t wValue, uint16_t wIndex, void *buf, uint16_t buflen)
{
    USBRESULT res;
    uint16_t len;

    if (buflen > sizeof(struct USB20_descriptor_header)) {
        /* Get descriptor length.  */
        struct USB20_descriptor_header hdr;
        //log_msg(">>>>>>>>>>>>>>>>>33333333333333333333333333\n");

        if((res=usb_control_msg(dev, NULL, bRequest, bmRequestType,
                                 wValue, wIndex, &hdr, sizeof(hdr))) != USB_OK){
            return res;
                                 }

        if (hdr.bLength < sizeof(hdr)) {
            //usb_dev_error(dev, "Descriptor length too short\n");
            //return USB_STATUS_INVALID_DATA;
            PANIC("Descriptor length too short");
        }

        /* Length to read is the minimum of the descriptor's actual length and
         * the buffer length.  */
        usb_dev_debug(dev,"Set len to MIN of hdr.bLength=%d and buflen=%d\n",hdr.bLength,buflen);
        len = MIN(hdr.bLength, buflen);
    }else{
        len = buflen;
    }

    //log_msg(">>>>>>>>>>>>>>>>>44444444444444444444444444444444\n");


    /* Read the descriptor for real.  */
    return usb_control_msg(dev, NULL, bRequest, bmRequestType, wValue, wIndex, buf, len);
    //return USB_ERROR;
}


/* Read a USB device's device descriptor, or a prefix of it, into
 * dev->descriptor.  */
USBRESULT usb_read_device_descriptor(usb_device_t *dev, uint16_t maxlen)
{
    log_msg("Read device descriptor\n");
    /* Note: we do not really need to use usb_get_descriptor() here because we
     * never read more than the minimum length of the device descriptor.  */
    return usb_control_msg(dev, NULL,
                           USB_DEVICE_REQUEST_GET_DESCRIPTOR,
                           USB_BMREQUESTTYPE_DIR_IN |
                           USB_BMREQUESTTYPE_TYPE_STANDARD |
                           USB_BMREQUESTTYPE_RECIPIENT_DEVICE,
                           USB_DESCRIPTOR_TYPE_DEVICE << 8, 0,
                           &dev->descriptor, maxlen);
}


/* Read the specified configuration descriptor, or a prefix of it, from a USB
 * device into a buffer.  */
USBRESULT usb_get_configuration_descriptor(struct usb_device *dev, uint8_t configuration_idx, void *buf, uint16_t buflen)
{
    return usb_control_msg(dev, NULL, USB_DEVICE_REQUEST_GET_DESCRIPTOR,
                           USB_BMREQUESTTYPE_DIR_IN |
                           USB_BMREQUESTTYPE_TYPE_STANDARD |
                           USB_BMREQUESTTYPE_RECIPIENT_DEVICE,
                           (USB_DESCRIPTOR_TYPE_CONFIGURATION << 8) | configuration_idx,
                           0, buf, buflen);
}

USBRESULT usb_read_configuration_descriptor(struct usb_device *dev, uint8_t configuration)
{
    USBRESULT res;
    struct USB20_configuration_descriptor desc;
//    usb_status_t status;
    int interface_idx;
    int endpoint_idx;
//    uint i;
    struct USB20_descriptor_header *hdr;
    BOOL in_alternate_setting;
//
    /* Get configuration descriptor size */
    if((res=usb_get_configuration_descriptor(dev, configuration, &desc, sizeof(desc))) != USB_OK) {
        return res;
    }

    /* Allocate buffer for full configuration descriptor */
    dev->config_descriptor = kmalloc(desc.wTotalLength,0);
    if (dev->config_descriptor == NULL ) {
        return USB_STATUS_OUT_OF_MEMORY;
    }

    /* Get configuration descriptor for real */
    if((res=usb_get_configuration_descriptor(dev, configuration,dev->config_descriptor,desc.wTotalLength)) != USB_OK) {
        return res;
    }

    /* Set up pointers to the interface descriptors and endpoint descriptors.   */
    interface_idx = -1;
    endpoint_idx = -1;
    in_alternate_setting = FALSE;
    for (int i = 0;  i + sizeof(struct USB20_descriptor_header) <= desc.wTotalLength; i += hdr->bLength) {
        hdr = (struct USB20_descriptor_header*)((uint8_t*)dev->config_descriptor + i);

        if (hdr->bLength < sizeof(struct USB20_descriptor_header)) {
            goto out_invalid;
        }
        usb_debug("Handle hdr at %#x  i=%d ,  interface_idx=%d ,  endpoint_idx=%d \n",hdr,i,interface_idx,endpoint_idx);

        switch (hdr->bDescriptorType)
        {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                usb_dev_debug(dev,"USB_DESCRIPTOR_TYPE_INTERFACE \n");

                if (i + sizeof(struct USB20_interface_descriptor) > desc.wTotalLength){
                    goto out_invalid;
                }
                if (interface_idx >= 0 && !in_alternate_setting &&  endpoint_idx + 1 != dev->interfaces[interface_idx]->bNumEndpoints) {
                    usb_dev_debug(dev, "Number of endpoints incorrect\n");
                    goto out_invalid;
                }
                if (((struct USB20_interface_descriptor*)hdr)->bAlternateSetting != 0) {
                    usb_dev_debug(dev, "in_alternate_setting = TRUE\n");
                    in_alternate_setting = TRUE;
                }
                else
                {
                    usb_dev_debug(dev, "in_alternate_setting = FALSE\n");
                    in_alternate_setting = FALSE;
                    if (++interface_idx >= USB_DEVICE_MAX_INTERFACES)
                    {
                        usb_dev_error(dev,"Too many interfaces (this driver only supports %d per configuration)\n", USB_DEVICE_MAX_INTERFACES);
                        return USB_STATUS_DEVICE_UNSUPPORTED;
                    }
                    dev->interfaces[interface_idx] = (struct USB20_interface_descriptor*)hdr;
                    endpoint_idx = -1;
                }
                break;
            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                usb_dev_debug(dev,"USB_DESCRIPTOR_TYPE_ENDPOINT \n");
                if (interface_idx < 0){
                    goto out_invalid;
                }
                if (i + sizeof(struct USB20_endpoint_descriptor) > desc.wTotalLength) {
                    goto out_invalid;
                }
                if (!in_alternate_setting){
                    if (++endpoint_idx >= USB_DEVICE_MAX_ENDPOINTS) {
                        usb_dev_error(dev,"Too many endpoints (this driver only supports %d per interface)\n", USB_DEVICE_MAX_ENDPOINTS);
                        return USB_STATUS_DEVICE_UNSUPPORTED;
                    }
                    dev->endpoints[interface_idx][endpoint_idx] = (struct USB20_endpoint_descriptor*)hdr;
                }
                break;
            default:
                usb_dev_debug(dev,"UNKNOWN USB_DESCRIPTOR_TYPE %#x \n",hdr->bDescriptorType);
                break;
        }
    }
    if (interface_idx + 1 != dev->config_descriptor->bNumInterfaces) {
        usb_dev_debug(dev, "Number of interfaces incorrect (interface_idx=%d)\n",interface_idx);
        goto out_invalid;
    }

    return USB_OK;

out_invalid:
    usb_dev_error(dev, "Configuration descriptor invalid\n");
    return USB_STATUS_INVALID_DATA;
}


USBRESULT usb_set_configuration(struct usb_device *dev, uint8_t configuration)
{
    USBRESULT res;

    res = usb_control_msg(dev,
                             NULL, /* endpoint 0 */
                             USB_DEVICE_REQUEST_SET_CONFIGURATION,
                             USB_BMREQUESTTYPE_DIR_OUT |
                             USB_BMREQUESTTYPE_TYPE_STANDARD |
                             USB_BMREQUESTTYPE_RECIPIENT_DEVICE,
                             configuration, /* wValue: configuration index */
                             0, NULL, 0); /* wIndex, data, wLength: unused */
    if (res == USB_OK){
        dev->configuration = configuration;
    }
    return res;
}


/* Sets the bus address of a USB device.  */
USBRESULT usb_set_address(struct usb_device *dev, uint8_t address)
{
    USBRESULT res;
    res = usb_control_msg(dev, NULL, /* default control endpoint */
                             USB_DEVICE_REQUEST_SET_ADDRESS,
                             USB_BMREQUESTTYPE_DIR_OUT |
                             USB_BMREQUESTTYPE_TYPE_STANDARD |
                             USB_BMREQUESTTYPE_RECIPIENT_DEVICE,
                             address, /* wValue: address */
                             0, NULL, 0); /* wIndex, buf, wLength: unused */
    if (res == USB_OK){
        dev->address = address;
    }else{
        PANIC("FAILED TO SET ADRESS");
    }
    return res;
}


USBRESULT usb_get_string_descriptor(struct usb_device *dev, uint8_t index, uint16_t lang_id, struct USB20_string_descriptor *buf, uint16_t buflen)
{
    usb_dev_debug(dev,"usb_get_string_descriptor index=%d lang_id=%#x buflen=%d\n",index,lang_id,buflen);
    return usb_get_descriptor(dev,
                              USB_DEVICE_REQUEST_GET_DESCRIPTOR,
                              USB_BMREQUESTTYPE_DIR_IN |
                              USB_BMREQUESTTYPE_TYPE_STANDARD |
                              USB_BMREQUESTTYPE_RECIPIENT_DEVICE,
                              (USB_DESCRIPTOR_TYPE_STRING << 8) | index, lang_id,
                              buf, buflen);
}

USBRESULT usb_get_ascii_string(struct usb_device *dev, uint32_t iString, char *strbuf, uint32_t strbufsize)
{
    USBRESULT res;
//    struct {
//        struct USB20_string_descriptor desc;
//        uint16_t padding[USB_MAXSTRINGLEN];
//    } buf;

    uint8_t buf[sizeof(struct USB20_string_descriptor)+(USB_MAXSTRINGLEN*2)];
    struct USB20_string_descriptor *desc;
    desc = (struct USB20_string_descriptor *) buf;

    uint16_t lang_id;
//    usb_status_t status;
//    uint i;
//    uint num_languages;
    uint16_t num_chars=0;
    int langCount;
    uint16_t alignedbuf[USB_MAXSTRINGLEN];

    //log_msg("11111111111111111111111111111111111111\n");


    /* Get the list of available languages.  */
    if((res=usb_get_string_descriptor(dev, 0, 0, desc, sizeof(buf))) != USB_OK){
        PANIC("usb_get_string_descriptor failed");
        return res;
    }
    usb_debug("GOT string descriptor type=%d len=%d strbufsize=%d\n",desc->bDescriptorType,desc->bLength,strbufsize);


    /* Make sure the list of available languages is nonempty.  */
    langCount = (desc->bLength - sizeof(struct USB20_descriptor_header)) / sizeof(uint16_t);
    if (langCount == 0){
//        usb_dev_error(dev, "String descriptor language list is empty\n");
        PANIC("String descriptor language list is empty\n");
        return USB_STATUS_INVALID_DATA;
    }

    /* Choose the first listed variant of English, or fall back to the first
     * language listed if English is not found.  */
    lang_id = desc->bString[0];
    for (int i = 0; i < langCount; i++){
        if ((desc->bString[i] & USB_PRIMARY_LANGUAGE_MASK) == USB_LANG_ENGLISH){
            lang_id = desc->bString[i];
            break;
        }
    }
    usb_debug("GOT lang_id=%#x\n",lang_id);
    //log_msg("2222222222222222222222222222222222222222222\n");

    /* Get the actual string descriptor we wanted.  */
    if((res = usb_get_string_descriptor(dev, iString, lang_id, desc, sizeof(buf))) != USB_OK) {
        PANIC("FAILED usb_get_string_descriptor");
        return res;
    }

    /* "Translate" the string from UTF-16LE to ASCII.  */
    num_chars = MIN((desc->bLength - sizeof(struct USB20_descriptor_header)) / sizeof(uint16_t), strbufsize - 1);
    usb_dev_debug(dev,"num_chars = %d\n",num_chars);
    if(num_chars>USB_MAXSTRINGLEN){
        //trunkate to 128, since alignedbuf is only 128
        num_chars=USB_MAXSTRINGLEN;
    }
    memcpy(alignedbuf,desc->bString,num_chars*2); //copy chars to an aligned buffer
    utf16le_to_ascii(alignedbuf, num_chars, strbuf);
    strbuf[num_chars] = '\0';
//    return USB_STATUS_SUCCESS;
    return USB_OK;
}


USBRESULT usb_try_to_bind_device_driver(struct usb_device *dev)
{
    USBRESULT res;
    if (dev->driver != NULL) {
        /* Driver already bound.  */
        return USB_OK;
    }

    res = USB_STATUS_DEVICE_UNSUPPORTED;
    for (int i = 0; i < usb_num_device_drivers; i++) {
        usb_dev_debug(dev, "Attempting to probe %s to device\n", usb_device_drivers[i]->name);
        res = usb_device_drivers[i]->probe(dev);
        if (res == USB_OK) {
            usb_dev_debug(dev, "Attempting to bind %s to device\n", usb_device_drivers[i]->name);
            res = usb_device_drivers[i]->bind_device(dev);
            if (res != USB_STATUS_DEVICE_UNSUPPORTED) {
                if (res == USB_OK) {
                    dev->driver = usb_device_drivers[i];
                    usb_info("Bound %s to %s\n", dev->driver->name, usb_device_description(dev));
                }
                break;
            }
        }
    }
    return res;
}


USBRESULT usb_attach_device(usb_device_t *dev){
    USBRESULT res=0;
    usb_dev_debug(dev,"usb_attach_device %#x\n",dev);


    dev->descriptor.bMaxPacketSize0 = 8;

    if((res = usb_read_device_descriptor(dev, 8)) !=USB_OK){
        PANIC("usb_read_device_descriptor");
        goto error;
    }
    usb_dev_debug(dev,"Using bMaxPacketSize0=%d\n", dev->descriptor.bMaxPacketSize0);

#ifdef USB_DEBUG
    usb_debug_device_info(dev);
#endif

    if (dev->address>0) {
        //this device already has an adress?? XHCI?
    }else {
        uint8_t address = nextUsbAdress++;
        usb_dev_debug(dev,"**** TODO REPLACE, but got address for now = %d\n", address);
        if((res = usb_set_address(dev, address)) !=USB_OK){
            PANIC("usb_set_address");
            goto error;
        }
    }
    log_msg("Set address to %d\n",dev->address);



//    return USB_ERROR;

    // ReRead the device descriptor to find information about this device.
    usb_dev_debug(dev,"ReReading device descriptor.\n");
    if((res = usb_read_device_descriptor(dev, sizeof(dev->descriptor))) != USB_OK) {
        PANIC("usb_read_device_descriptor");
        goto error;
    }

#ifdef USB_DEBUG
    usb_debug_device_info(dev);
#endif


    // Read product and manufacturer strings if present.
    if (dev->descriptor.iProduct != 0) {
        usb_dev_debug(dev,"Reading product string.\n");
        usb_get_ascii_string(dev, dev->descriptor.iProduct, dev->product, USB_MAXSTRINGLEN);
    }
    usb_dev_debug(dev,"GOT PRODUCT STRING : %s\n",dev->product);
    if (dev->descriptor.iManufacturer != 0) {
        usb_dev_debug(dev,"Reading manufacturer string.\n");
        usb_get_ascii_string(dev, dev->descriptor.iManufacturer,dev->manufacturer, USB_MAXSTRINGLEN);
    }
    usb_dev_debug(dev,"GOT MANUFACTURER STRING : %s\n",dev->manufacturer);
#ifdef USB_DEBUG
    usb_debug_device_info(dev);
#endif
    //return USB_ERROR;




    // Read the first configuration descriptor.
    usb_debug("Reading the first configuration descriptor.\n");
    if((res = usb_read_configuration_descriptor(dev, 0)) != USB_OK)  {
        //usb_dev_error(dev, "Failed to read configuration descriptor: %s\n", usb_status_string(res));
        //return status;
        PANIC("Failed to read configuration descriptor");
    }

    // Configure the device with its first reported configuration.
    usb_dev_debug(dev, "Assigning configuration %d (%d interfaces available)\n", dev->config_descriptor->bConfigurationValue, dev->config_descriptor->bNumInterfaces);
    if(( res=usb_set_configuration(dev,dev->config_descriptor->bConfigurationValue)) != USB_OK){
        //usb_dev_error(dev, "Failed to set device configuration: %s\n",usb_status_string(status));
        PANIC("Failed to set device configuration");
        return res;
    }

    // Report the device attachment at an informational log level.
    usb_info("Attaching %s\n", usb_device_description(dev));

    //PANIC("TODO Implement usb_attach_device");

    // Try to bind a driver to the newly configured device.
    res = usb_try_to_bind_device_driver(dev);


    if (res == USB_STATUS_DEVICE_UNSUPPORTED)
    {
        usb_dev_info(dev, "No driver found for device.\n");
        // No currently registered driver supports the new device.  However,
        // * this should not be considered a failure to attach the device, since
        // * the needed driver may just not be registered yet.
        res = USB_OK;
    }
    else if (res != USB_OK)
    {
        //usb_dev_error(dev, "Failed to bind driver to new USB device: %s\n",usb_status_string(status));
        usb_dev_error(dev, "Failed to bind driver to new USB device: %d\n",res);
    }

    return res;
error:
    return USB_ERROR;
}



USBRESULT usb_enumerate_hcd_roothub(usb_hcd_t* hcd) {
    log_msg("Enumerate root hub : %s\n", hcd->name);

    //uint8_t dev_address=1;
    uint8_t port=0;
    while (hcd->hcd_roothub_is_port_present(hcd, port)) {
        usb_debug("PORT %#x is present\n",port);

        if (hcd->hcd_roothub_port_reset(hcd, port)==OK) {
            log_msg("port reset successful\n");
            // is a device is attached?
            if (hcd->hcd_roothub_is_port_device_present(hcd,port)) {
                usb_debug("Found device at port %d, getting descriptor...\n", port);


                usb_device_t *devx = usb_alloc_device();
                devx->usb_hcd = (struct usb_hcd *) hcd;

                //enum kUSB_speed portSpeed = kUSB_SPEED_UNKNOWN;
                //enum kUSB_speed portSpeed = hcd->hcd_roothub_port_speed(hcd,port);
                //devx->speed = USB_SPEED_LOW;
                //if (devx->speed==USB_SPEED_LOW) {
                //    usb_debug("  SPEED_LOW (%d)\n",portSpeed);
                //}else {
                //    usb_debug("  SPEED_FULL (%d)\n",portSpeed);
                //}

                usb_attach_device(devx);

            }else{
                kprintf("No device at port %d\n", port);
            }
        }else{
            usb_debug("Error resetting port %d\n",port);
        }
        port ++;
    }
    return OK;
}


void usb_enumerate_roothubs(void) {
    for (int i=0;i<MAX_HCD_DEVS;i++) {
        usb_hcd_t* hcd = hcd_devices[i];
        if (hcd>0 && hcd->hcdstate==HCD_STATE_STARTED) {
            //usb_enumerate_hcd_roothub(hcd);
        }
    }
}


void usb_poll(void) {
    for (int i=0;i<MAX_HCD_DEVS;i++) {
        usb_hcd_t* hcd = hcd_devices[i];
        if (hcd>0 && hcd->hcdstate==HCD_STATE_STARTED) {
            hcd->poll(hcd);
        }
    }

}

extern void xhci_selftest_interrupt(usb_hcd_t *hcd);
void usb_init(void) {
    log_msg("USB setup!\n");
    for (int i=0;i<MAX_HCD_DEVS;i++) {
        usb_hcd_t* hcd = hcd_devices[i];
        if (hcd>0 && hcd->hcdstate==HCD_STATE_INIT) {
            hcd->setup(hcd);
        }
    }

    log_msg("USB start!\n");
    for (int i=0;i<MAX_HCD_DEVS;i++) {
        usb_hcd_t* hcd = hcd_devices[i];
        if (hcd>0 && hcd->hcdstate==HCD_STATE_SETUP) {
            hcd->start(hcd);
        }
    }

    log_msg("USB init!\n");

    //for (int i=0;i<MAX_HCD_DEVS;i++) {
    //    hcd_devices[i]=0;
    //}
    usb_enumerate_roothubs();


    //xhci_selftest_interrupt(hcd_devices[0]);
}