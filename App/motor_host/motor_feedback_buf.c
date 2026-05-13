/**
 * @file motor_feedback_buf.c
 */

#include "motor_feedback_buf.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define BUF_MASK  (MOTOR_FEEDBACK_BUF_SIZE - 1U)
_Static_assert((MOTOR_FEEDBACK_BUF_SIZE & BUF_MASK) == 0U,
               "MOTOR_FEEDBACK_BUF_SIZE must be a power of 2");

static caw_driver_feedback_t s_buf[MOTOR_FEEDBACK_BUF_SIZE];
static uint32_t              s_wr = 0U;
static uint32_t              s_rd = 0U;
static SemaphoreHandle_t     s_mutex = NULL;

void motor_feedback_buf_init(void)
{
    s_wr    = 0U;
    s_rd    = 0U;
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex != NULL);
}

void motor_feedback_buf_push(const caw_driver_feedback_t *feedback)
{
    configASSERT(feedback != NULL);
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_buf[s_wr & BUF_MASK] = *feedback;
    s_wr++;

    if ((s_wr - s_rd) > MOTOR_FEEDBACK_BUF_SIZE) {
        s_rd++;
    }

    xSemaphoreGive(s_mutex);
}

bool motor_feedback_buf_pop(caw_driver_feedback_t *out)
{
    configASSERT(out != NULL);
    bool result = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_wr != s_rd) {
        *out = s_buf[s_rd & BUF_MASK];
        s_rd++;
        result = true;
    }
    xSemaphoreGive(s_mutex);

    return result;
}

bool motor_feedback_buf_peek_latest(caw_driver_feedback_t *out)
{
    configASSERT(out != NULL);
    bool result = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_wr != s_rd) {
        *out   = s_buf[(s_wr - 1U) & BUF_MASK];
        result = true;
    }
    xSemaphoreGive(s_mutex);

    return result;
}

uint32_t motor_feedback_buf_count(void)
{
    uint32_t cnt = 0U;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    cnt = s_wr - s_rd;
    xSemaphoreGive(s_mutex);
    return cnt;
}
