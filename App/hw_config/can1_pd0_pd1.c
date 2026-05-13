/**
 * @file can1_pd0_pd1.c
 */

#include "can1_pd0_pd1.h"
#include "main.h"
#include "FreeRTOSConfig.h"

FDCAN_HandleTypeDef hfdcan1;

/* Bit time: FDCAN kernel 120 MHz / (6 * 20) = 1 Mbit/s, ~80% sample point */
#define CAN1_NOM_PRESCALER  6U
#define CAN1_NOM_TSEG1      15U
#define CAN1_NOM_TSEG2      4U
#define CAN1_NOM_SJW        4U

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan->Instance != FDCAN1) {
        return;
    }

    RCC_PeriphCLKInitTypeDef pclk = {0};
    pclk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    pclk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) {
        Error_Handler();
    }

    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    HAL_NVIC_SetPriority(FDCAN1_IT1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
}

void can1_init(void)
{
    hfdcan1.Instance                  = FDCAN1;
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode                 = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = ENABLE;

    hfdcan1.Init.NominalPrescaler     = CAN1_NOM_PRESCALER;
    hfdcan1.Init.NominalSyncJumpWidth = CAN1_NOM_SJW;
    hfdcan1.Init.NominalTimeSeg1      = CAN1_NOM_TSEG1;
    hfdcan1.Init.NominalTimeSeg2      = CAN1_NOM_TSEG2;

    hfdcan1.Init.DataPrescaler        = CAN1_NOM_PRESCALER;
    hfdcan1.Init.DataSyncJumpWidth    = CAN1_NOM_SJW;
    hfdcan1.Init.DataTimeSeg1         = CAN1_NOM_TSEG1;
    hfdcan1.Init.DataTimeSeg2         = CAN1_NOM_TSEG2;

    hfdcan1.Init.MessageRAMOffset     = 0U;
    hfdcan1.Init.StdFiltersNbr        = 1U;
    hfdcan1.Init.ExtFiltersNbr        = 0U;
    hfdcan1.Init.RxFifo0ElmtsNbr      = 8U;
    hfdcan1.Init.RxFifo0ElmtSize      = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo1ElmtsNbr      = 0U;
    hfdcan1.Init.RxFifo1ElmtSize      = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxBuffersNbr         = 0U;
    hfdcan1.Init.RxBufferSize         = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.TxEventsNbr          = 0U;
    hfdcan1.Init.TxBuffersNbr         = 0U;
    hfdcan1.Init.TxFifoQueueElmtsNbr  = 4U;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;
    hfdcan1.Init.TxElmtSize           = FDCAN_DATA_BYTES_8;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }

    FDCAN_FilterTypeDef flt = {0};
    flt.IdType       = FDCAN_STANDARD_ID;
    flt.FilterIndex  = 0U;
    flt.FilterType   = FDCAN_FILTER_RANGE;
    flt.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    flt.FilterID1    = 0x000U;
    flt.FilterID2    = 0x7FFU;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &flt) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                     FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
}

HAL_StatusTypeDef can1_tx_std8(uint32_t std_id, const uint8_t data[8])
{
    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier          = std_id;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.DataLength          = FDCAN_DLC_BYTES_8;
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker       = 0U;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &hdr, (uint8_t *)data);
}

void FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}

void FDCAN1_IT1_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&hfdcan1);
}
