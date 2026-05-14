# cawlib-emb-examples

[English](README.md) | **简体中文**

[`cawlib_emb`](cawlib_emb/) C 库的使用示例——一个在 **STM32H7** 上通过**经典 CAN** 驱动无刷电机的固件工程。`App/` 目录提供完整的主机侧参考实现：FDCAN1 初始化、驱动器引导（DR 协议）、1 kHz 周期性 FOC 广播，以及用于缓存电机反馈数据（位置、速度、电流）的线程安全环形缓冲区——全部运行在 FreeRTOS 上。

---

## 硬件环境

| 项目 | 说明 |
|------|------|
| MCU | STM32H743（Cortex-M7，480 MHz） |
| CAN 外设 | FDCAN1 — **PD0 = RX**，**PD1 = TX** |
| 总线速率 | 1 Mbit/s，经典 CAN（无 BRS/FD） |
| RTOS | FreeRTOS（CMSIS-RTOS v2） |
| 工具链 | GNU Arm Embedded（`arm-none-eabi-gcc`） |

---

## 仓库结构

```
App/
  hw_config/
    can1_pd0_pd1.c/.h     FDCAN1 初始化 + 发送辅助函数
  motor_host/
    motor_host.c/.h       TX/RX 任务、TIM2 节拍、DR 引导、正弦演示
    motor_feedback_buf.c/.h  线程安全遥测环形缓冲区

cawlib_emb/
  include/
    cawlib.h              电机总线 API（caw_motor_bus_t、FOC 广播）
    driver.h              驱动器控制 API + caw_driver_feedback_t
  library/
    libcawlib_0.0.1_thumbv7em-none-eabihf.a   Cortex-M7 硬浮点  ← 默认链接
    libcawlib_0.0.1_thumbv7em-none-eabi.a     Cortex-M7 软浮点
    libcawlib_0.0.1_thumbv7m-none-eabi.a      Cortex-M3/M4 软浮点

Core/                     STM32CubeIDE / HAL 生成文件
Drivers/                  STM32H7 HAL + CMSIS
Middlewares/              FreeRTOS 源码
```

对接入口：`Core/Src/freertos.c` 中 `StartDefaultTask` 在调度器启动后依次调用 `can1_init()` 与 `motor_host_start()`。

---

## cawlib_emb API 概览

### 电机总线（`cawlib.h`）

库最多管理每帧 **4 个电机槽位**，每个槽位保存一个电机的节点 ID、FOC 模式和当前目标值。

```c
caw_motor_bus_t bus;

// 初始化总线
caw_motor_bus_init(&bus);

// 注册电机：槽位 0，节点 ID 0x200，位置模式
caw_motor_bus_register(&bus, 0, 0x200, CAW_MOTOR_FOC_POSITION);

// 更新位置目标（弧度）
caw_motor_bus_set_target(&bus, 0, 1.57f);

// 打包 8 字节 CAN 载荷并以 1 kHz 发送
uint8_t payload[8];
caw_motor_bus_build_broadcast(&bus, payload);
can1_tx_std8(0x100, payload);

// 将收到的反馈帧送回总线
caw_motor_bus_on_feedback(&bus, 0x200, rx_data);
```

FOC 模式：`CAW_MOTOR_FOC_CURRENT`（电流）、`CAW_MOTOR_FOC_SPEED`（速度）、`CAW_MOTOR_FOC_POSITION`（位置）。

### 驱动器控制（`driver.h`）

电机在接受 FOC 指令前，需要通过 **DR（Driver-control）协议** 进入正确的运行模式。该协议是一个基于 CAN 的轻量配置会话。

```c
uint16_t dest = caw_driver_control_encode_dest(0x200);

// 1. 打开单播配置会话（两帧，共 16 字节）
uint8_t pair[16];
caw_driver_control_enter_configuring_unicast_pair(dest, pair);
can1_tx_std8(0x100, &pair[0]);
can1_tx_std8(0x100, &pair[8]);

// 2. 设置 FOC 模式为位置控制
uint8_t cmd[8];
caw_driver_control_set_foc_mode(dest, CAW_DRIVER_CONTROL_FOC_MODE_POSITION, cmd);
can1_tx_std8(0x100, cmd);

// 3. 关闭配置会话
uint8_t exit_cmd[8];
caw_driver_control_exit_configure(dest, exit_cmd);
can1_tx_std8(0x100, exit_cmd);
```

### 反馈遥测（`driver.h`）

每个电机节点以 8 字节帧回复位置、速度和电流信息：

```c
caw_driver_feedback_t fb;
caw_driver_feedback_decode(rx_data, &fb);

float pos_rad   = fb.position_rad;  // 位置（弧度）
float speed_rps = fb.speed_rad_s;   // 速度（弧度/秒）
float current_a = fb.current_a;     // 电流（安培）
```

---

## 协议参数

所有总线与运动参数均定义于 `App/motor_host/motor_host.h`，请在编译前根据实际硬件调整。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `MOTOR_HOST_TX_STD_ID` | `0x100` | 主机 → 电机所有帧使用的标准帧 ID |
| `MOTOR_NODE_ID` | `0x200` | 电机节点反馈帧的标准帧 ID |
| `MOTOR_SLOT_INDEX` | `0` | 4 槽广播包中的槽位编号 |
| `MOTOR_SINE_CENTER_RAD` | `0.0` | 正弦轨迹中心位置（弧度） |
| `MOTOR_SINE_AMPLITUDE_RAD` | `6.2832` | 正弦幅值 ≈ ±2π rad（约一整圈） |
| `MOTOR_SINE_PERIOD_MS` | `4000` | 正弦周期（毫秒） |

---

## 编译

安装 **GNU Arm Embedded 工具链**（`arm-none-eabi-gcc`），然后在仓库根目录执行：

```bash
make -j
```

构建产物位于 `build/`：

| 文件 | 说明 |
|------|------|
| `build/cawlib-emb-examples.elf` | 含调试符号的 ELF |
| `build/cawlib-emb-examples.hex` | Intel HEX（适用于大多数烧录工具） |
| `build/cawlib-emb-examples.bin` | 原始二进制 |

清理构建：

```bash
make clean
```

### 切换库版本

默认链接 `libcawlib_0.0.1_thumbv7em-none-eabihf.a`（Cortex-M7 硬浮点）。如需适配其他内核或 ABI，修改 `Makefile` 中的 `LIBS` 行：

| 目标 | 库文件 | `LIBS` 值 |
|------|--------|-----------|
| Cortex-M7 硬浮点（**默认**） | `libcawlib_0.0.1_thumbv7em-none-eabihf.a` | `-lcawlib_0.0.1_thumbv7em-none-eabihf` |
| Cortex-M7 软浮点 | `libcawlib_0.0.1_thumbv7em-none-eabi.a` | `-lcawlib_0.0.1_thumbv7em-none-eabi` |
| Cortex-M3 / M4 软浮点 | `libcawlib_0.0.1_thumbv7m-none-eabi.a` | `-lcawlib_0.0.1_thumbv7m-none-eabi` |

---

## 运行说明

- **TIM2** 通过更新中断 → 二值信号量 → `MotorTx` 任务提供 1 kHz TX 节拍。预分频与自动重载值基于 APB1 定时器时钟 240 MHz（STM32H743 默认 PLL1 配置）。若修改时钟树，需重新计算 `MOTOR_TIM_PSC` / `MOTOR_TIM_ARR`。
- **FDCAN RX 中断**优先级设置为 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`，以确保 `HAL_FDCAN_RxFifo0Callback` 中可安全使用 FreeRTOS `FromISR` 系列 API。若启动时出现断言失败，请检查 `FreeRTOSConfig.h`。
- **遥测读取**：在其他任务中调用 `motor_feedback_buf_peek_latest(&fb)` 可获取最新解码帧，不会从缓冲区中移除（环形缓冲区大小 32，互斥锁保护）。
- **DR 引导序列**在 `motor_host_start()` 内部同步执行，在 TX/RX 任务创建前完成。每条配置指令之间有短暂延时（`DR_AFTER_ENTER_MS` / `DR_AFTER_CMD_MS`，各 10 ms），以确保电机节点有足够时间处理。

---

## 移植到其他开发板

1. **CAN 引脚 / 外设** — 用自己的 FDCAN 初始化代码替换 `App/hw_config/can1_pd0_pd1.c`，保持 `can1_tx_std8()` 接口不变，或同步更新 `motor_host.c` 中的调用。
2. **时钟树** — 重新计算 `MOTOR_TIM_PSC` / `MOTOR_TIM_ARR` 以保持 1 kHz 定时器中断频率。
3. **库 ABI** — 从 `cawlib_emb/library/` 中选择匹配的预编译库并更新 `Makefile` 的 `LIBS`。
4. **CAN ID** — 根据总线拓扑修改 `MOTOR_HOST_TX_STD_ID` 与 `MOTOR_NODE_ID`。

---

## 许可证

本项目采用 **MIT License**。

`Drivers/` 下的 HAL 及 CMSIS 文件遵循 STMicroelectronics 的许可条款。  
`Middlewares/` 下的 FreeRTOS 文件采用 MIT License。
