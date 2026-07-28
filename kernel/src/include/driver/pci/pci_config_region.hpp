#ifndef _DRIVER_PCI_CONFIG_REGION_HPP
#define _DRIVER_PCI_CONFIG_REGION_HPP

class PciConfigRegion {
private:

public:
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t bus_number_start;
    uint8_t bus_number_end;

    PciConfigRegion(uint64_t _base_address,uint16_t _pci_segment_group,uint8_t _bus_number_start,uint8_t _bus_number_end) {
        base_address = _base_address;
        pci_segment_group = _pci_segment_group;
        bus_number_start = _bus_number_start;
        bus_number_end = _bus_number_end;
    }
    ~PciConfigRegion();


};

#endif