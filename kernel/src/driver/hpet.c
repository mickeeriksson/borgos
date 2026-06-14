#include "driver/hpet.h"
#include "error.h"
#include "cpu/mmio.h"
#include "bits.h"

#define HPET_GENCAP_REG 0x0
#define HPET_CONFIG_REG 0x10
#define HPET_INTSTAT_REG 0x20
#define HPET_COUNTER_REG 0xF0


void hpet_disable(struct hpetinfo *hpet) {
    uint64_t configreg = mmio_read64(hpet->base+HPET_CONFIG_REG);
    configreg = bits_64_set(configreg,0x0,0,0);
    mmio_write64(hpet->base+HPET_CONFIG_REG,configreg);
}

void hpet_enable(struct hpetinfo *hpet) {
    uint64_t configreg = mmio_read64(hpet->base+HPET_CONFIG_REG);
    configreg = bits_64_set(configreg,0x1,0,0);
    mmio_write64(hpet->base+HPET_CONFIG_REG,configreg);
}

void hpet_set_counter(struct hpetinfo *hpet,uint64_t counter) {
    mmio_write64(hpet->base+HPET_COUNTER_REG,counter);
}

uint64_t hpet_get_counter(struct hpetinfo *hpet) {
    uint64_t maincounterreg = mmio_read64(hpet->base+HPET_COUNTER_REG);
    return maincounterreg;
}

uint64_t hpet_get_counter_clock_period(struct hpetinfo *hpet) {
    uint64_t capreg = mmio_read64(hpet->base+HPET_GENCAP_REG);
    uint64_t ret = bits_64_get(capreg,32,63);
    return ret;
}

uint64_t hpet_get_millis(struct hpetinfo *hpet) {
    uint64_t millisdiv = 1000000000000 / hpet_get_counter_clock_period(hpet);
    uint64_t counter = hpet_get_counter(hpet);
    uint64_t millis = counter / millisdiv;
    log_msg("Millis = 0x%lx / 0x%lx = 0x%lx (%d)\n", counter,millisdiv, millis, millis);
    return millis;
}


void hpet_debug_regs(struct hpetinfo *hpet) {
    log_msg("hpet_debug_regs()\n");

    uint64_t capreg = mmio_read64(hpet->base+HPET_GENCAP_REG);
    log_msg("General CAPS and ID reg\n");
    log_msg("   COUNTER_CLK_PERIOD 0x%x  (%d)\n", bits_64_get(capreg,32,63),bits_64_get(capreg,32,63));
    log_msg("   VENDOR_ID 0x%x\n", bits_64_get(capreg,16,31));
    log_msg("   LEG_RT_CAP 0x%x\n", bits_64_get(capreg,15,15));
    log_msg("   COUNT_SIZE_CAP 0x%x\n", bits_64_get(capreg,13,13));
    log_msg("   NUM_TIM_CAP 0x%x\n", bits_64_get(capreg,8,12));
    log_msg("   REV_ID 0x%x\n", bits_64_get(capreg,0,7));

    uint64_t configreg = mmio_read64(hpet->base+HPET_CONFIG_REG);
    log_msg("General config reg\n");
    log_msg("   LEG_RT_CNF 0x%x\n", bits_64_get(configreg,1,1));
    log_msg("   ENABLE_CNF 0x%x\n", bits_64_get(configreg,0,0));

    uint64_t isrreg = mmio_read64(hpet->base+HPET_INTSTAT_REG);
    log_msg("General ISR reg\n");
    log_msg("   T2_INT_STS 0x%x\n", bits_64_get(isrreg,2,2));
    log_msg("   T1_INT_STS 0x%x\n", bits_64_get(isrreg,1,1));
    log_msg("   T0_INT_STS 0x%x\n", bits_64_get(isrreg,0,0));

    uint64_t maincounterreg = mmio_read64(hpet->base+HPET_COUNTER_REG);
    log_msg("Main  counter reg\n");
    log_msg("   MAIN_COUNTER_VAL 0x%lx\n", maincounterreg);

    int timers = bits_64_get(capreg,8,12)+1;
    for (int i = 0; i < timers; i++) {
        uint64_t timerreg = mmio_read64(hpet->base+ (0x100+(i*0x20)));
        log_msg("Timer %d Config and Caps reg\n",i);
        log_msg("   T%d_INT_ROUTE_CAP 0x%x\n",i,bits_64_get(timerreg,32,63));
        log_msg("   T%d_FSB_INT_DEL_CAP  0x%x\n",i,bits_64_get(timerreg,15,15));
        log_msg("   T%d_FSB_EN_CNF 0x%x\n",i,bits_64_get(timerreg,14,14));
        log_msg("   T%d_INT_ROUTE_CNF 0x%x\n",i,bits_64_get(timerreg,9,13));
        log_msg("   T%d_32MODE_CNF  0x%x\n",i,bits_64_get(timerreg,8,8));
        log_msg("   T%d_VAL_SET_CNF 0x%x\n",i,bits_64_get(timerreg,6,6));
        log_msg("   T%d_SIZE_CAP 0x%x\n",i,bits_64_get(timerreg,5,5));
        log_msg("   T%d_PER_INT_CAP 0x%x\n",i,bits_64_get(timerreg,4,4));
        log_msg("   T%d_TYPE_CNF 0x%x\n",i,bits_64_get(timerreg,3,3));
        log_msg("   T%d_INT_ENB_CNF 0x%x\n",i,bits_64_get(timerreg,2,2));
        log_msg("   T%d_INT_TYPE_CNF 0x%x\n",i,bits_64_get(timerreg,1,1));
    }

}


uint64_t hpet_vsystimer_get_millis(void* hpetptr) {
    struct hpetinfo *hpet = hpetptr;
    uint64_t millisdiv = 1000000000000 / hpet_get_counter_clock_period(hpet);
    uint64_t counter = hpet_get_counter(hpet);
    uint64_t millis = counter / millisdiv;
    //log_msg("Millis = 0x%lx / 0x%lx = 0x%lx (%d)\n", counter,millisdiv, millis, millis);
    return millis;
}


void hpet_init(struct hpetinfo *hpet) {
    //hpet_debug_regs(hpet);

    //log_msg("Disable HPET\n");
    hpet_disable(hpet);
    hpet_set_counter(hpet,0x0);
    uint64_t hz = 1000000000000000 / hpet_get_counter_clock_period(hpet);
    log_msg("HPET HZ = %d\n",hz);
    hpet_enable(hpet);

    /*
    uint64_t s3 = 3 * hz;
    while (hpet_get_counter(hpet) < s3) {
        // do nothing
    }
    uint64_t millis = hpet_get_millis(hpet);
    log_msg("DONE! millis = %d,\n\n\n",millis);
    hpet_debug_regs(hpet);
    */

    log_msg("HPET Enabled\n");
    //PANIC("HPET");
}
