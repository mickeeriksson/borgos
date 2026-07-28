#include "types.h"

char* pci_class_typename(uint8_t class, uint8_t subclass,uint8_t prog_if) {
    switch(class) {
        case 0x01:
            switch(subclass) {
            case 0x06:
                return "Serial ATA Controller (unspecified)";
            default:
                return "Mass Storage Controller (Unspecified)";
            }
        case 0x03:
            switch(subclass) {
            case 0x00:
                return "VGA Compatible Controller (unspecified)";
            default:
                return "Display Controller (Unspecified)";
            }
        case 0x06:
            switch(subclass) {
            case 0x00:
                return "Host Bridge";
            case 0x01:
                return "ISA Bridge";
            default:
                return "Bridge (Unspecified)";
            }
        case 0x0C:
            switch(subclass) {
            case 0x03:
                    switch(prog_if) {
                        case 0x00:
                            return "USB controller (UHCI)";
                        case 0x10:
                            return "USB controller (OHCI)";
                        case 0x20:
                            return "USB controller (EHCI)";
                        case 0x30:
                            return "USB controller (XHCI)";
                        case 0x80:
                            return "USB controller (Unspecified)";
                        case 0xFE:
                            return "USB device (not controller)";
                        default:
                            return "USB controller (Unknown)";
                    }
                    return "SMBus Controller";
            case 0x05:
                    return "SMBus Controller";
            default:
                    return "Serial Bus Controller (Unspecified)";
            }
        default:
            return "Unknown PCI Device";
    }
}