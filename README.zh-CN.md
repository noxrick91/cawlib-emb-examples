# cawlib-emb-examples

[English](README.md) | **简体中文**

基于 **cawlib** C 接口（`cawlib_emb`）、通过 **经典 CAN** 驱动电机的 STM32H7 固件示例。`App/` 下的应用代码实现主机侧：**FDCAN1**、周期性 **FOC 广播**、driver-control 上电会话、遥测缓冲。

## App 目录（电机演示）

| 路径 | 作用 |
|------|------|
| `App/hw_config/can1_pd0_pd1.c` | FDCAN1：**PD0 = RX**，**PD1 = TX**，**1 Mbit/s**，仅经典 CAN |
| `App/motor_host/motor_host.c` | TIM2 **1 kHz** → 发送任务 → `caw_motor_bus_*` 组帧并发出；RX 回调 → 队列 → 解码 |
| `App/motor_host/motor_host.h` | **CAN 节点 ID**、FOC 槽位、正弦演示参数 |
| `App/motor_host/motor_feedback_buf.c` | 线程安全的 `caw_driver_feedback_t` 环形缓冲区 |

对接入口：`Core/Src/freertos.c` 中 `StartDefaultTask` 在调度器运行后依次调用 `can1_init()` 与 `motor_host_start()`。

### 协议与参数（在 `motor_host.h` 中按实车修改）

- 主机下发的 FOC / 控制帧：**标准帧** ID **`MOTOR_HOST_TX_STD_ID`**（`0x100`）。
- 电机节点应答：**标准帧** ID **`MOTOR_NODE_ID`**（`0x200`），载荷 **8 字节**。
- 4 槽广播中本电机占用 **槽位** `MOTOR_SLOT_INDEX`（默认 `0`）。
- 演示运动：正弦 **位置** 目标——中心 `MOTOR_SINE_CENTER_RAD`、幅值 `MOTOR_SINE_AMPLITUDE_RAD`（默认在 `MOTOR_SINE_PERIOD_MS` 内约 ±2π rad）。

请根据总线与机械限位调整上述宏。

### 依赖

- **cawlib** 头文件：`cawlib_emb/include/`（`cawlib.h`、`driver.h`）
- **预编译静态库**（见 `Makefile` 链接选项）：`cawlib_0.0.1_thumbv7em-none-eabihf`（对应 `libcawlib_0.0.1_thumbv7em-none-eabihf.a`）  
  若修改内核/FPU，可选用 `cawlib_emb/library/` 下其它 triple（如 `thumbv7em-none-eabi`、`thumbv7m-none-eabi`）。

### 编译

需要 **GNU Arm Embedded** 工具链（`arm-none-eabi-gcc`）。在仓库根目录执行：

```bash
make -j
```

输出位于 `build/`（`*.elf`、`*.hex`、`*.bin`）。

### 运行说明

- **TIM2** 提供 1 kHz 节拍（未使用 FreeRTOS 软件定时器）。分频与自动重载与当前工程 **RCC / APB1 定时器时钟**（H7 上 APB 分频非 1 时定时器时钟常 ×2）匹配；若改时钟树需重新计算 **PSC / ARR**。
- **FDCAN** RX 中断优先级与 `HAL_FDCAN_RxFifo0Callback` 中 **FromISR** 类 API 兼容（详见 `FreeRTOSConfig.h` 与 `can1_pd0_pd1.c`）。
- 其它任务读取最新遥测：`motor_feedback_buf_peek_latest()`。

## 许可证

在此补充你的开源许可证说明。Cube 生成的 HAL、CMSIS 等仍遵循原版权方条款。
