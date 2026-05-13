/**
 * @file motor_host.c
 *
 * TIM2 @ 1 kHz gives a binary semaphore; MotorTx sends FOC broadcast on CAN1.
 * FDCAN RX ISR queues raw 8-byte frames to MotorRx (decode + motor_feedback_buf_push).
 */

#include "motor_host.h"
#include "motor_feedback_buf.h"
#include "can1_pd0_pd1.h"
#include "cawlib.h"
#include "driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "cmsis_os.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DR_AFTER_ENTER_MS   10U
#define DR_AFTER_CMD_MS     10U

#define TX_TASK_STACK_WORDS  256U
#define RX_TASK_STACK_WORDS  256U
#define TX_TASK_PRIORITY     (osPriorityAboveNormal)
#define RX_TASK_PRIORITY     (osPriorityHigh)
#define RX_RAW_QUEUE_DEPTH   16U

/* TIM2: 240 MHz / 240 = 1 MHz counter, period 1000 -> 1 kHz (APB1 prescaled, timer clock doubled). */
#define MOTOR_TIM_PSC   (240U - 1U)
#define MOTOR_TIM_ARR   (1000U - 1U)

static caw_motor_bus_t      s_bus;
static SemaphoreHandle_t    s_bus_mutex    = NULL;
static QueueHandle_t        s_rx_raw_queue = NULL;
static TIM_HandleTypeDef    s_motor_tim;
static SemaphoreHandle_t    s_tx_sem       = NULL;
static TickType_t           s_tx_start_tick = 0U;

static void bootstrap_motor_via_dr(void);
static void motor_tim_init(void);
static void motor_tx_task(void *arg);
static void motor_rx_task(void *arg);

void motor_host_start(void)
{
    motor_feedback_buf_init();

    s_bus_mutex = xSemaphoreCreateMutex();
    configASSERT(s_bus_mutex != NULL);

    caw_motor_bus_init(&s_bus);
    int reg_ret = caw_motor_bus_register(&s_bus, MOTOR_SLOT_INDEX,
                                         MOTOR_NODE_ID, (int)CAW_MOTOR_FOC_POSITION);
    configASSERT(reg_ret == 0);

    s_rx_raw_queue = xQueueCreate(RX_RAW_QUEUE_DEPTH, 8U);
    configASSERT(s_rx_raw_queue != NULL);

    s_tx_sem = xSemaphoreCreateBinary();
    configASSERT(s_tx_sem != NULL);

    bootstrap_motor_via_dr();

    BaseType_t ret;
    ret = xTaskCreate(motor_tx_task, "MotorTx",
                      TX_TASK_STACK_WORDS, NULL, TX_TASK_PRIORITY, NULL);
    configASSERT(ret == pdPASS);

    ret = xTaskCreate(motor_rx_task, "MotorRx",
                      RX_TASK_STACK_WORDS, NULL, RX_TASK_PRIORITY, NULL);
    configASSERT(ret == pdPASS);

    s_tx_start_tick = xTaskGetTickCount();
    motor_tim_init();
}

void motor_host_set_target(float target_rad)
{
    xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
    caw_motor_bus_set_target(&s_bus, MOTOR_SLOT_INDEX, target_rad);
    xSemaphoreGive(s_bus_mutex);
}

static void bootstrap_motor_via_dr(void)
{
    uint16_t dest = caw_driver_control_encode_dest(MOTOR_NODE_ID);

    uint8_t pair[16];
    caw_driver_control_enter_configuring_unicast_pair(dest, pair);
    can1_tx_std8(MOTOR_HOST_TX_STD_ID, &pair[0]);
    can1_tx_std8(MOTOR_HOST_TX_STD_ID, &pair[8]);

    vTaskDelay(pdMS_TO_TICKS(DR_AFTER_ENTER_MS));

    uint8_t cmd[8];
    caw_driver_control_set_foc_mode(dest, (int)CAW_DRIVER_CONTROL_FOC_MODE_POSITION, cmd);
    can1_tx_std8(MOTOR_HOST_TX_STD_ID, cmd);

    vTaskDelay(pdMS_TO_TICKS(DR_AFTER_CMD_MS));

    uint8_t exit_cmd[8];
    caw_driver_control_exit_configure(dest, exit_cmd);
    can1_tx_std8(MOTOR_HOST_TX_STD_ID, exit_cmd);

    vTaskDelay(pdMS_TO_TICKS(DR_AFTER_CMD_MS));
}

/* Assumes configTICK_RATE_HZ == 1000: elapsed ticks == ms for sine phase. */
static float sine_target_rad(TickType_t elapsed_ticks)
{
    float t_ms  = (float)elapsed_ticks;
    float phase = (2.0f * (float)M_PI) * (t_ms / MOTOR_SINE_PERIOD_MS);
    return MOTOR_SINE_CENTER_RAD + MOTOR_SINE_AMPLITUDE_RAD * sinf(phase);
}

static void motor_tim_init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    s_motor_tim.Instance              = TIM2;
    s_motor_tim.Init.Prescaler        = MOTOR_TIM_PSC;
    s_motor_tim.Init.CounterMode      = TIM_COUNTERMODE_UP;
    s_motor_tim.Init.Period           = MOTOR_TIM_ARR;
    s_motor_tim.Init.ClockDivision    = TIM_CLOCKDIVISION_DIV1;
    s_motor_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&s_motor_tim);

    HAL_NVIC_SetPriority(TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    __HAL_TIM_CLEAR_FLAG(&s_motor_tim, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&s_motor_tim, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(&s_motor_tim);
}

void TIM2_IRQHandler(void)
{
    if (__HAL_TIM_GET_IT_SOURCE(&s_motor_tim, TIM_IT_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&s_motor_tim, TIM_IT_UPDATE);

        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(s_tx_sem, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void motor_tx_task(void *arg)
{
    (void)arg;

    for (;;) {
        xSemaphoreTake(s_tx_sem, portMAX_DELAY);

        TickType_t elapsed = xTaskGetTickCount() - s_tx_start_tick;
        float target = sine_target_rad(elapsed);

        uint8_t payload[8];
        xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
        caw_motor_bus_set_target(&s_bus, MOTOR_SLOT_INDEX, target);
        caw_motor_bus_build_broadcast(&s_bus, payload);
        xSemaphoreGive(s_bus_mutex);

        can1_tx_std8(MOTOR_HOST_TX_STD_ID, payload);
    }
}

static void motor_rx_task(void *arg)
{
    (void)arg;

    uint8_t raw[8];

    for (;;) {
        if (xQueueReceive(s_rx_raw_queue, raw, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        xSemaphoreTake(s_bus_mutex, portMAX_DELAY);
        caw_motor_bus_on_feedback(&s_bus, MOTOR_NODE_ID, raw);
        xSemaphoreGive(s_bus_mutex);

        caw_driver_feedback_t feedback;
        caw_driver_feedback_decode(raw, &feedback);
        motor_feedback_buf_push(&feedback);
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance != FDCAN1) {
        return;
    }
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U) {
        return;
    }

    FDCAN_RxHeaderTypeDef rx_hdr;
    uint8_t               rx_data[8];

    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_hdr, rx_data) != HAL_OK) {
        return;
    }

    if (rx_hdr.IdType != FDCAN_STANDARD_ID) {
        return;
    }
    if (rx_hdr.Identifier != MOTOR_NODE_ID) {
        return;
    }
    if (rx_hdr.DataLength != FDCAN_DLC_BYTES_8) {
        return;
    }

    BaseType_t higher_prio_task_woken = pdFALSE;
    xQueueSendFromISR(s_rx_raw_queue, rx_data, &higher_prio_task_woken);
    portYIELD_FROM_ISR(higher_prio_task_woken);
}
