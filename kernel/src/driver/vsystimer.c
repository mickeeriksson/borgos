#include "driver/vsystimer.h"
#include "error.h"
//This is the implementation of the system virtual timer

struct vsystimer_info *vsystimer_info;

void vsystimer_init(struct vsystimer_info *info) {
    vsystimer_info = info;
}

uint64_t vsystimer_millis(void) {
    if (vsystimer_info==NULL || vsystimer_info->private_timerdata==NULL || vsystimer_info->ops==NULL) {
        PANIC("vsystimer_millis(): vsystimer_info is NOT initialized!");
    }
    return vsystimer_info->ops->getmillis(vsystimer_info->private_timerdata);
}
