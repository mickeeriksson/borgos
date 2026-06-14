#ifndef _ACPI_H_
#define _ACPI_H_


// structure for revision 0 (version 1.0)
typedef struct RSDP {
    char signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdtAddress;
} __attribute__ ((packed)) RSDP_t;

// structure for revision 2 (version 2.0+)
typedef struct XSDP {
    char signature[8];
    uint8_t checksum;
    char OEMID[6];
    uint8_t revision;
    uint32_t rsdtAddress;      // deprecated since version 2.0

    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed)) XSDP_t;

typedef struct SDT {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t creatorID;
    uint32_t creatorRevision;
} __attribute__ ((packed)) SDT_t;

typedef struct RSDT {
    SDT_t header;
    uint32_t entries[];
} __attribute__ ((packed)) RSDT_t;

struct sdt_address_structure
{
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed));

typedef struct HPET {
    SDT_t header;
    uint8_t hardware_rev_id;
    int8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    struct sdt_address_structure address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__ ((packed)) HPET_t;

typedef struct MADT_entry {
    uint8_t type;
    uint8_t size;
} __attribute__ ((packed)) MADT_entry_t;
#define MADT_type_CPULAPIC 0
#define MADT_type_IOAPIC 1
#define MADT_type_IOAPICINTOVERRIDE 2
#define MADT_type_IOAPICNMISOURCE 3
#define MADT_type_LAPICNMI 4
#define MADT_type_LAPICADR 5
#define MADT_type_CPUX2APIC 9

struct MADT_entry_CPULAPIC {
    struct MADT_entry entryheader;
    uint8_t acpiid;
    uint8_t apicid;
    uint32_t flags;
} __attribute__ ((packed)) ;


typedef struct MADT {
    SDT_t header;
    uint32_t local_apic_addr;
    uint32_t flags;
    //struct MADT_entry entries[];
} __attribute__ ((packed)) MADT_t;

extern void acpi_debugprint_entries(void);
extern SDT_t* acpi_find(char* signature);

#endif
