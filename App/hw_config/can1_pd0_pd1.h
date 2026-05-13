/**
 * @file can1_pd0_pd1.h
 *
 * FDCAN1 on PD0 (RX) / PD1 (TX), classic CAN 1 Mbit/s. Clock from PLL1Q (see can1_init).
 */

#ifndef CAN1_PD0_PD1_H
#define CAN1_PD0_PD1_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern FDCAN_HandleTypeDef hfdcan1;

void can1_init(void);
HAL_StatusTypeDef can1_tx_std8(uint32_t std_id, const uint8_t data[8]);

#ifdef __cplusplus
}
#endif

#endif
