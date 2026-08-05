#ifndef _NET_IF_H
#define _NET_IF_H



enum if_state {
    IF_STATE_UNDEFINED = 0,
    IF_STATE_INIT      = 1,      //after attached or released
    IF_STATE_SETUP     = 2,      //after successfull setup, or stop
    IF_STATE_STARTED   = 3,      //after successfull start
};

// ===== Statistics ==================================================
//
// Not cosmetic. During bring-up the difference between "no packet arrived" and
// "128 packets arrived and were dropped because the ring was full" is the whole
// debugging session, and without counters it is invisible.
typedef struct {
    uint32_t rx_ok;
    uint32_t rx_err;         // header status reported a bad frame
    uint32_t rx_overflow;    // RBO or FOVW seen
    uint32_t rx_too_big;     // size outside [64, 1518]
    uint32_t rx_dropped;     // ring full
    uint32_t tx_ok;
    uint32_t tx_err;
    uint32_t tx_timeout;
    uint32_t irq_spurious;   // ISR read as 0: another device on the shared line
    //uint32_t missed;         // snapshot of MPC
} if_stats_t;


typedef struct net_if_device {
    const char *name;
    int ifstate;
    int (*setup)(struct net_if_device *ifdev);    //allocate structures, and init controller
    int (*release)(struct net_if_device *ifdev);  //stop controller and relerase structures.
    int (*start)(struct net_if_device *ifdev);    //start , start irq
    int (*stop)(struct net_if_device *ifdev);     //stop , stop irq
    int (*poll)(struct net_if_device *ifdev);

    if_stats_t stats;
    //operations
    int (*send)(struct net_if_device *ifdev,const void* frame,uint16_t length);
} net_if_device_t;

typedef struct net_if_eth_device {
    net_if_device_t  net_if_dev;   //must be first, this way a pointer to net_if_eth_device is also pointer to ifdev...
    uint8_t  mac[6];
} net_if_eth_device_t;


extern net_if_device_t *if_devices[];

extern void net_if_attach_if(net_if_device_t *ifdev);

#endif