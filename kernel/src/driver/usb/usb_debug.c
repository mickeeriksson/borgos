#include "kconsole.h"
#include "log.h"
#include "driver/usb/usb.h"
#include <stdarg.h>
#include <string.h>
#include "stdio.h"


#ifdef USB_DEBUG


void usb_debug_device_info(struct usb_device* dev){
    //log_msg("Got FULL descriptor dev = %d!\n",dev_address);

    struct USB20_device_descriptor *devdesc;
    devdesc = &dev->descriptor;

    log_msg("\n  Device Descriptor:"
            "\n                 len: %i"
            "\n                type: %i"
            "\n             version: %#x.%#x"
            "\n               class: %i"
            "\n            subclass: %i"
            "\n            protocol: %i"
            "\n     max packet size: %i"
            "\n           vendor id: %#x"
            "\n          product id: %#x"
            "\n         release ver: %i%i.%i%i"
            "\n   manufacture index: %i (%s)"
            "\n       product index: %i (%s)"
            "\n        serial index: %i"
            "\n   number of configs: %i\n",
            devdesc->bLength,
            devdesc->bDescriptorType,
            devdesc->bcdUSB >> 8,
            devdesc->bcdUSB & 0xFF,
            devdesc->bDeviceClass,
            devdesc->bDeviceSubClass,
            devdesc->bDeviceProtocol,
            devdesc->bMaxPacketSize0,
            devdesc->idVendor,
            devdesc->idProduct,
            (devdesc->bcdDevice & 0xF000) >> 12,
            (devdesc->bcdDevice & 0x0F00) >> 8,
            (devdesc->bcdDevice & 0x00F0) >> 4,
            (devdesc->bcdDevice & 0x000F) >> 0,
            devdesc->iManufacturer, dev->manufacturer,
            devdesc->iProduct, dev->product,
            devdesc->iSerialNumber,
            devdesc->bNumConfigurations
    );
}

void usb_log(int priority, const char *func, struct usb_device *dev, const char *format, ...)
{
    va_list va;
//    irqmask im;

//    if (priority < USB_MIN_LOG_PRIORITY)
//    {
//        return;
//    }

//    im = disable();

    log_msg("USB: ");
    if (priority <= USB_LOG_PRIORITY_DEBUG)
    {
        log_msg("[DEBUG] ");
    }
    else if (priority >= USB_LOG_PRIORITY_ERROR)
    {
        log_msg("[ERROR] ");
    }
    if (dev != NULL)
    {
        log_msg("Device %d: ", dev->address);
    }
    if (priority <= USB_LOG_PRIORITY_DEBUG)
    {
        log_msg("%s(): ", func);
    }
    va_start(va, format);
    ///kvprintf(format, va);
    log_vmsg(format, va);
    //log_msg(format, __VA_ARGS__);
    va_end(va);
//    restore(im);
}



const char *usb_speed_to_string(enum usb_speed speed)
{
    switch (speed) {
        case USB_SPEED_HIGH:
            return "high";
        case USB_SPEED_FULL:
            return "full";
        case USB_SPEED_LOW:
            return "low";
        case USB_SPEED_SUPER:
            return "super";
        case USB_SPEED_SUPER_PLUS:
            return "super plus";
        case USB_SPEED_UNKNOWN:
            return "unknown";
    }
    return "unknown";
}

static const char *usb_bcd_version_to_string(uint16_t bcdUSB)
{
    static char string[3 + 1 + 2 + 1 + 2 + 1];
    char *p = string;
    p += sprintf(string, "%d.%d",
                 (bcdUSB >> 8) & 0xff,  // At most 3 digits
                 (bcdUSB >> 4) & 0xf);  // At most 2 digits
                                        // (plus period)
    if (bcdUSB & 0xf) {
        sprintf(p, ".%d", bcdUSB & 31);  // At most 2 digits (plus period)
    }
    return string;
}

const char *usb_class_code_to_string(enum usb_class_code class_code)
{
    switch (class_code)
    {
        case USB_CLASS_CODE_INTERFACE_SPECIFIC:
            return "None (see interface descriptors)";
        case USB_CLASS_CODE_AUDIO:
            return "Audio";
        case USB_CLASS_CODE_COMMUNICATIONS_AND_CDC_CONTROL:
            return "Communications and CDC Control";
        case USB_CLASS_CODE_HID:
            return "HID (Human Interface Device)";
        case USB_CLASS_CODE_IMAGE:
            return "Image";
        case USB_CLASS_CODE_PRINTER:
            return "Printer";
        case USB_CLASS_CODE_MASS_STORAGE:
            return "Mass Storage";
        case USB_CLASS_CODE_HUB:
            return "Hub";
        case USB_CLASS_CODE_VIDEO:
            return "Video";
        case USB_CLASS_CODE_WIRELESS_CONTROLLER:
            return "Wireless Controller";
        case USB_CLASS_CODE_MISCELLANEOUS:
            return "Miscellaneous";
        case USB_CLASS_CODE_VENDOR_SPECIFIC:
            return "Vendor Specific";
    }
    return "Unknown";
}

const char *usb_transfer_type_to_string(enum usb_transfer_type type)
{
    switch (type)
    {
        case USB_TRANSFER_TYPE_CONTROL:
            return "Control";
        case USB_TRANSFER_TYPE_ISOCHRONOUS:
            return "Isochronous";
        case USB_TRANSFER_TYPE_BULK:
            return "Bulk";
        case USB_TRANSFER_TYPE_INTERRUPT:
            return "Interrupt";
    }
    return "Unknown";
}
const char *usb_direction_to_string(enum usb_direction dir)
{
    switch (dir)
    {
        case USB_DIRECTION_OUT:
            return "OUT";
        case USB_DIRECTION_IN:
            return "IN";
    }
    return "Unknown";
}









/**
 * @ingroup usbcore,
 * STOLEN FROM XINU
 *
 * Returns a fairly detailed, human-readable description of a USB device.
 *
 * @param dev
 *      USB device to get a description of.
 *
 * @return
 *      A string describing the device.  The returned string is statically
 *      allocated and will be changed on the next call to this function.
 */

const char *usb_device_description(const struct usb_device *dev)
{
    //uint i;
    enum usb_class_code class;
    static char device_description[512];
    char *p;

    p = device_description;

    // Start with speed and USB version information.
    p += sprintf(p, "%s-speed USB %s", usb_speed_to_string(dev->speed), usb_bcd_version_to_string(dev->descriptor.bcdUSB));

    // Try to find a class description of the device, taking into account that
    //  the class may be stored either in the device descriptor or an interface
    //  descriptor.
    class = dev->descriptor.bDeviceClass;
    if (class == 0){
        for (int i = 0; i < dev->config_descriptor->bNumInterfaces; i++) {
            if (dev->interfaces[i]->bInterfaceClass != 0) {
                class = dev->interfaces[i]->bInterfaceClass;
            }
        }
    }

    // Add the class description if we found one and it was not something
    //  meaningless like the vendor specific class.
    if (class != 0 && class != USB_CLASS_CODE_VENDOR_SPECIFIC && class != USB_CLASS_CODE_MISCELLANEOUS) {
        p += sprintf(p, " %s class",
        usb_class_code_to_string(class));
    }

    // This is indeed a device.
    p += sprintf(p, " device");

    // Add the product name, if the device provides it.
    if (dev->product[0] != '\0') {
        p += sprintf(p, " (%s)", dev->product);
    }

    // Add vendor and product IDs.
    p += sprintf(p, " (idVendor=0x%04x, idProduct=0x%04x)", dev->descriptor.idVendor, dev->descriptor.idProduct);

    // Return the resulting string.
    return device_description;
}


#endif

