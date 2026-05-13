/**
 * @file motor_host.h
 *
 * Example host: FDCAN1 TX (see MOTOR_HOST_TX_STD_ID), motor feedback on MOTOR_NODE_ID.
 * Call can1_init() then motor_host_start() from a task after the scheduler runs.
 */

#ifndef MOTOR_HOST_H
#define MOTOR_HOST_H

#include "cawlib.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_HOST_TX_STD_ID    0x100U
#define MOTOR_NODE_ID           0x200U
#define MOTOR_SLOT_INDEX        0U

/** Sine trajectory: center (rad). */
#define MOTOR_SINE_CENTER_RAD       0.0f
/** Peak deviation (rad); 2π ≈ one mechanical revolution each way from center. */
#define MOTOR_SINE_AMPLITUDE_RAD    6.2832f
/** Sine period (ms); targets are quantized by caw_motor_bus / library encoding. */
#define MOTOR_SINE_PERIOD_MS        4000.0f

void motor_host_start(void);
void motor_host_set_target(float target_rad);

#ifdef __cplusplus
}
#endif

#endif
