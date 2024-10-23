#pragma once

#include "ch.h"
#include "hal.h"
#include "sent_conf.h"

class SentGmFuelSensor {
    public:

/* GM DI fuel pressure, temperature sensor decoded data */
int32_t gm_sig0;
int32_t gm_sig1;
int32_t gm_stat;

/* GM DI fuel pressure, temperature sensor data */
int32_t gm_GetSig0() {
    return gm_sig0;
}

int32_t gm_GetSig1() {
    return gm_sig1;
}

int32_t gm_GetStat() {
    return gm_stat;
}

int32_t gm_GetPressure() {
    /* two pressure signals:
     * Sig0 occupie 3 first nibbles in MSB..LSB order
     * Sig1 occupit next 3 nibbles in LSB..MSB order
     * Signals are close, but not identical.
     * Sig0 shows about 197..198 at 1 Atm (open air) and 282 at 1000 KPa (9.86 Atm)
     * Sig1 shows abour 202..203 at 1 Atm (open air) and 283 at 1000 KPa (9.86 Atm)
     * So for 8.86 Atm delta there are:
     * 84..85 units for sig0
     * 80..81 units for sig1
     * Measurements are not ideal, so lets ASSUME 10 units per 1 Atm
     * Offset is 187..188 for Sig0 and 192..193 for Sig1.
     * Average offset is 180.
     */

    /* in 0.001 Atm */
    return ((gm_GetSig0() - 198 + 10 + gm_GetSig1() - 202 + 10) * 100 / 2);
}

};