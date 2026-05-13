#ifndef CAW_DRIVER_H
#define CAW_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAW_DRIVER_CONTROL_MAGIC_0 ((uint8_t)'D')
#define CAW_DRIVER_CONTROL_MAGIC_1 ((uint8_t)'R')
#define CAW_DRIVER_CONTROL_VERSION_BYTE ((uint8_t)'1')

#define CAW_DRIVER_CONTROL_REQ_ENTER_CONFIGURE_CHALLENGE ((uint8_t)0x73)
#define CAW_DRIVER_CONTROL_REQ_ENTER_CONFIGURE_COMMIT ((uint8_t)0x74)
#define CAW_DRIVER_CONTROL_REQ_CONFIG_SESSION ((uint8_t)0xFF)

#define CAW_DRIVER_CONTROL_CTL_CODE_DR_CONFIGURE_SESSION_ENTER ((uint8_t)0x03)
#define CAW_DRIVER_CONTROL_CTL_CODE_DR_SESSION_END ((uint8_t)0x00)
#define CAW_DRIVER_CONTROL_CTL_CODE_START_ENCODER_ZEROING ((uint8_t)0x01)

#define CAW_DRIVER_CONTROL_CTL_GROUP_SESSION ((uint8_t)0x00)
#define CAW_DRIVER_CONTROL_CTL_GROUP_FOC_MODE ((uint8_t)0x01)

#define CAW_DRIVER_CONTROL_BROADCAST_DEST_ID ((uint16_t)0xFFFF)

typedef enum {
    CAW_DRIVER_CONTROL_FOC_MODE_CURRENT = 0,
    CAW_DRIVER_CONTROL_FOC_MODE_SPEED = 1,
    CAW_DRIVER_CONTROL_FOC_MODE_POSITION = 2,
} caw_driver_control_foc_mode_t;

typedef struct {
    uint8_t  req_ack;
    uint8_t  _pad0;
    uint16_t dest_can_id_le;
    uint8_t  ctl_group;
    uint8_t  ctl_code;
    uint8_t  ctl_byte;
    uint8_t  _pad1;
} caw_driver_control_frame_t;

_Static_assert(sizeof(caw_driver_control_frame_t) == 8,
               "caw_driver_control_frame_t must match Rust CControlFrame");

static inline uint8_t caw_driver_control_pack_ctl(uint8_t group, uint8_t code)
{
    return (uint8_t)(((group & 0x0FU) << 4) | (code & 0x0FU));
}

int caw_driver_control_checksum_ok(const uint8_t *data);

void caw_driver_control_fill_checksum(uint8_t data[8]);

void caw_driver_control_encode(uint8_t out[8], uint8_t req_ack, uint16_t dest, uint8_t ctl_group,
                               uint8_t ctl_code);

void caw_driver_control_decode_payload(const uint8_t data[8],
                                       caw_driver_control_frame_t *out_decoded);

int caw_driver_control_validate_payload(const uint8_t data[8]);

uint16_t caw_driver_control_encode_dest(uint32_t can_id);

void caw_driver_control_enter_configuring_unicast_pair(uint16_t dest, uint8_t out_pair[16]);
void caw_driver_control_enter_configuring_broadcast_pair(uint8_t out_pair[16]);

void caw_driver_control_exit_configure(uint16_t dest, uint8_t out[8]);
void caw_driver_control_calibrate(uint16_t dest, uint8_t out[8]);
void caw_driver_control_set_foc_mode(uint16_t dest, int mode, uint8_t out[8]);

typedef struct {
    float position_rad;
    float speed_rad_s;
    float current_a;
} caw_driver_feedback_t;

void caw_driver_feedback_decode(const uint8_t data[8], caw_driver_feedback_t *out);
void caw_driver_feedback_encode(const caw_driver_feedback_t *in, uint8_t out[8]);

#ifdef __cplusplus
}
#endif

#endif
