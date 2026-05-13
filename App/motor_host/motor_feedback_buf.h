/**
 * @file motor_feedback_buf.h
 *
 * Ring buffer of caw_driver_feedback_t; mutex-protected (task context only).
 * MOTOR_FEEDBACK_BUF_SIZE must be a power of two.
 */

#ifndef MOTOR_FEEDBACK_BUF_H
#define MOTOR_FEEDBACK_BUF_H

#include "cawlib.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_FEEDBACK_BUF_SIZE  32U

void motor_feedback_buf_init(void);
void motor_feedback_buf_push(const caw_driver_feedback_t *feedback);
bool motor_feedback_buf_pop(caw_driver_feedback_t *out);
bool motor_feedback_buf_peek_latest(caw_driver_feedback_t *out);
uint32_t motor_feedback_buf_count(void);

#ifdef __cplusplus
}
#endif

#endif
