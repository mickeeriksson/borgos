
#include "log.h"
#include "error.h"
#include "driver/net/net_if.h"
#include "mm.h"
#include "unicode.h"
#include <string.h>



net_if_device_t *if_devices[MAX_IF_DEVS] = {0,0,0,0,0,0,0,0};


void net_if_attach_if(net_if_device_t *ifdev) {
    //Simple database of all interfaces
    for (int i=0;i<MAX_IF_DEVS;i++) {
        if (if_devices[i]==0) {
            log_msg("Attaching %s\n", ifdev->name);
            if_devices[i]=ifdev;
            return;
        }
    }
}


void net_if_init(void) {
    log_msg("NET IF setup!\n");
    for (int i=0;i<MAX_IF_DEVS;i++) {
        net_if_device_t* ifdev = if_devices[i];
        if (ifdev>0 && ifdev->ifstate==IF_STATE_INIT) {
            ifdev->setup(ifdev);
        }
    }

    log_msg("NET IF start!\n");
    for (int i=0;i<MAX_IF_DEVS;i++) {
        net_if_device_t* ifdev = if_devices[i];
        if (ifdev>0 && ifdev->ifstate==IF_STATE_SETUP) {
            ifdev->start(ifdev);
        }
    }

    log_msg("NET init Done!\n");
}