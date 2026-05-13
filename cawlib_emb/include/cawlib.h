#ifndef CAWLIB_H
#define CAWLIB_H

#include "driver.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAW_MOTOR_SLOT_COUNT 4

typedef enum {
    CAW_MOTOR_FOC_CURRENT = 0,
    CAW_MOTOR_FOC_SPEED = 1,
    CAW_MOTOR_FOC_POSITION = 2,
} caw_motor_foc_mode_t;

float caw_motor_foc_broadcast_scale(int mode);
int16_t caw_motor_foc_encode_target(int mode, float value);

void caw_motor_broadcast_pack_slots(const int16_t slots[CAW_MOTOR_SLOT_COUNT], uint8_t out[8]);
void caw_motor_broadcast_unpack_slots(const uint8_t data[8], int16_t slots[CAW_MOTOR_SLOT_COUNT]);

typedef struct {
    uint32_t node_id;
    uint8_t valid;
    uint8_t _pad_align[3];
    uint32_t mode;
    float target_phys;
    caw_driver_feedback_t telemetry;
} caw_motor_slot_t;

typedef struct {
    caw_motor_slot_t slot[CAW_MOTOR_SLOT_COUNT];
} caw_motor_bus_t;

_Static_assert(sizeof(caw_driver_feedback_t) == sizeof(float) * 3, "caw_driver_feedback_t");

void caw_motor_bus_init(caw_motor_bus_t *bus);

int caw_motor_bus_register(caw_motor_bus_t *bus, uint8_t slot_index, uint32_t node_id,
                           int mode);

void caw_motor_bus_unregister(caw_motor_bus_t *bus, uint8_t slot_index);

void caw_motor_bus_set_target(caw_motor_bus_t *bus, uint8_t slot_index, float value);

float caw_motor_bus_get_target(const caw_motor_bus_t *bus, uint8_t slot_index);

void caw_motor_bus_build_broadcast(const caw_motor_bus_t *bus, uint8_t out[8]);

int caw_motor_bus_on_feedback(caw_motor_bus_t *bus, uint32_t can_id, const uint8_t data[8]);

const caw_motor_slot_t *caw_motor_bus_slot_const(const caw_motor_bus_t *bus,
                                                 uint8_t slot_index);

#ifdef __cplusplus
}
#endif

#endif
