#include "log.h"
#include "driver/pci/pci.h"
#include "mm.h"


int pci_request_region(pcidevice_t* pcidev, pcibar_t* pcibar,char* name) {

    /*
    När din drivrutin anropar pci_request_region(pdev, bar, "namn"),
    kollar kärnan i sin interna databas om någon annan drivrutin redan har lagt beslag på det minnesblock som enhetens BAR (Base Address Register) pekar på.
    Om området är ledigt: Kärnan markerar området som upptaget av din drivrutin och returnerar 0 (framgång).
    Om området är upptaget: Funktionen returnerar ett felmeddelande (oftast -EBUSY). Detta hindrar två drivrutiner från att skriva till samma hårdvara samtidigt,
    vilket annars skulle krascha systemet.

    2. Gör drivrutinen synlig i /proc
    När du har anropat funktionen framgångsrikt kommer namnet på din drivrutin att dyka upp i systemets resurslistor.
    Du kan se detta live i Linux-terminalen via:
    *cat /proc/iomem (för minnesmappad I/O / MMIO)
    *cat /proc/ioports (för klassiska I/O-portar)
    Detta är extremt användbart för felsökning eftersom du direkt ser vilken drivrutin som tagit kontroll över vilket hårdvaruområde.
    */
    log_msg("Always asume pci_request_region is successfull for now\n");
    log_msg(" TODO log and compare with register in feature\n");
    return 0;
}

int pci_release_region(pcidevice_t* pcidev, pcibar_t* pcibar) {
    log_msg("Always asume pci_release_region is successfull for now\n");
    log_msg(" TODO log and compare with register in feature\n");
    return 0;
}

adr_t pci_iomap(pcidevice_t* pcidev, pcibar_t* pcibar,char* name) {
    log_msg("Always asume pci_iomap is successfull for now\n");

    log_msg(" TODO MAP in kernel pagetabel\n");
    adr_t addr = pcibar->addr;
    adr_t size = pcibar->size;

    mmu_map_device_region(addr, addr+size-1);

    return P2V(addr);
}

int pci_enable_device(pcidevice_t* pcidev) {
    /*
    1. Aktiverar MMIO och I/O-portar i PCI Command RegisterVarje PCI-enhet har ett konfigurationsregister som heter PCI Command Register.
      Innan pci_enable_device() anropas är bitarna för Memory Space (MMIO) och I/O Space avslagna av säkerhetsskäl.
      Funktionen sätter dessa bitar så att chippet (i ditt fall CL-GD5446) överhuvudtaget börjar lyssna på de adresser som tilldelats dess BAR (Base Address Registers).
    2. Väcker enheten från strömsparläge (D3 -> D0)Om enheten har satts i ett strömsparläge (t.ex. D3hot eller D3cold) av ACPI eller BIOS under uppstarten, flyttar denna funktion enheten till fullt aktivt strömläge (D0).
    3. Allokerar och aktiverar IRQ (Avbrottshantering)Funktionen tilldelar en giltig IRQ-linje (Interrupt Request) till enheten. Om du kör på en modern PCI-e-arkitektur förbereder den även systemet för att enheten ska kunna använda MSI/MSI-X (Message Signaled Interrupts) om drivrutinen begär det senare.
    4. Fixar PCI-routing i systemetPå komplexa moderkort routar funktionen om avbrottslinjer genom ACPI-tabeller så att hårdvaruavbrott från kortet faktiskt når fram till rätt CPU-kärna.
    */
    log_msg("pci_enable_device does nothing for now\n");
    return 0;
}

int pci_disable_device(pcidevice_t* pcidev) {
    /*
     1. Inaktiverar I/O- och MMIO-avkodningFunktionen rensar bitarna för Memory Space och I/O Space i enhetens PCI Command Register. Detta gör att chippet (GD5446) slutar lyssna på adressbussen. Om hårdvaran skulle få för sig att skriva till minnet efter detta blockeras det direkt på bussnivå.
     2. Stänger av IRQ (Avbrottshantering)Den inaktiverar enhetens förmåga att skicka hårdvaruavbrott (interrupts) till CPU:n. Detta är helt kritiskt, för om enheten skickar ett avbrott efter att drivrutinen har avinstallerats finns det ingen kod kvar som kan hantera det, vilket omedelbart leder till en Kernel Panic ("irq lister loop" eller "unhandled interrupt").
     3. Sätter enheten i strömsparlägeKärnan kan, beroende på systemets konfiguration och ACPI, flytta enheten från aktivt läge (D0) till ett strömsparläge (t.ex. D3) för att spara energi.
        */
    log_msg("pci_disable_device does nothing for now\n");
    return 0;
}