/*
 * sent.h
 *
 *  Created on: 16 May 2022
 *      Author: alexv
 */

#pragma once

#define SENT_MODE_ICU 1
#define SENT_MODE_PAL 0

#define SENT_SILABS_SENS    0   // Silabs Si7215, tick 5 us
#define SENT_GM_ETB         1   // GM ETB throttle, tick 3.25 us

#define SENT_DEV  SENT_GM_ETB

#include "sent_conf.h"

#define SENT_OFFSET_INTERVAL 12
#define SENT_SYNC_INTERVAL   (56 - SENT_OFFSET_INTERVAL) // 56 ticks - 12

#define SENT_MIN_INTERVAL 12
#define SENT_MAX_INTERVAL 15

#define SENT_CRC_SEED 0x05

/* collect statistic */
#define SENT_STATISTIC_COUNTERS 1

#define SENT_THROTTLE_OPEN_VAL   435     // Sensor position of fully open throttle
#define SENT_THROTTLE_CLOSE_VAL  3665    // Sensor position of fully closed throttle

enum
{
    SENT_CH1 = 0,
    SENT_CH2,
#if SENT_DEV == SENT_SILABS_SENS
    SENT_CH3,
    SENT_CH4,
#endif
    SENT_CH_MAX,
};

typedef enum
{
        SM_SENT_INIT_STATE = 0,
        SM_SENT_SYNC_STATE,
        SM_SENT_STATUS_STATE,
        SM_SENT_SIG1_DATA1_STATE,
        SM_SENT_SIG1_DATA2_STATE,
        SM_SENT_SIG1_DATA3_STATE,
        SM_SENT_SIG2_DATA1_STATE,
        SM_SENT_SIG2_DATA2_STATE,
        SM_SENT_SIG2_DATA3_STATE,
        SM_SENT_CRC_STATE,
}SM_SENT_enum;

/* SENT init */
void InitSent();

/* Decoder task init, called from PAL or ICU implementation */
void SentDecoder_Init(void);

/* ISR hook */
void SENT_ISR_Handler(uint8_t ch, uint16_t val_res);

uint16_t SENT_GetData(uint8_t ch);

/* Stat counters */
uint32_t SENT_GetShortIntervalErrCnt(void);
uint32_t SENT_GetLongIntervalErrCnt(void);
uint32_t SENT_GetCrcErrCnt(void);
uint32_t SENT_GetSyncErrCnt(void);
uint32_t SENT_GetSyncCnt(void);
uint32_t SENT_GetFrameCnt(uint32_t n);
uint32_t SENT_GetErrPercent(void);
uint32_t SENT_GetTickTimeNs(void);

/* Debug */
void SENT_GetRawNibbles(uint8_t * buf);

uint8_t SENT_IsRawData(void);
uint16_t SENT_GetOpenThrottleVal(void);
uint16_t SENT_GetClosedThrottleVal(void);
void SENT_SetRawDataProp(void);
void SENT_ResetRawDataProp(void);
uint8_t SENT_GetThrottleValPrec(void);

/* Slow Channel */
uint16_t SENT_GetSlowMessagesFlags(uint32_t n);
uint16_t SENT_GetSlowMessage(uint32_t n, uint32_t i);
uint16_t SENT_GetSlowMessageID(uint32_t n, uint32_t i);

/* Si7215 decoded data */
int32_t Si7215_GetMagneticField(uint32_t n);
int32_t Si7215_GetCounter(uint32_t n);
