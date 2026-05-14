# cawlib-emb-examples

**English** | [简体中文](README.zh-CN.md)

Usage examples for the [`cawlib_emb`](cawlib_emb/) C library — a STM32H7 firmware project that drives a brushless motor over **classic CAN**. The `App/` directory provides a complete host-side reference: FDCAN1 initialization, driver bootstrap (DR protocol), periodic FOC broadcast at 1 kHz, and a thread-safe ring buffer for motor feedback data (position, speed, current) — all running on FreeRTOS.

---

## Hardware

| Item | Detail |
|------|--------|
| MCU | STM32H743 (Cortex-M7, 480 MHz) |
| CAN peripheral | FDCAN1 — **PD0 = RX**, **PD1 = TX** |
| Bus rate | 1 Mbit/s, classic CAN (no BRS/FD) |
| RTOS | FreeRTOS (CMSIS-RTOS v2) |
| Toolchain | GNU Arm Embedded (`arm-none-eabi-gcc`) |

---

## Repository Layout

```
App/
  hw_config/
    can1_pd0_pd1.c/.h     FDCAN1 init + tx helper
  motor_host/
    motor_host.c/.h       TX/RX tasks, TIM2 tick, DR bootstrap, sine demo
    motor_feedback_buf.c/.h  Thread-safe telemetry ring buffer

cawlib_emb/
  include/
    cawlib.h              Motor-bus API  (caw_motor_bus_t, FOC broadcast)
    driver.h              Driver-control API + caw_driver_feedback_t
  library/
    libcawlib_0.0.1_thumbv7em-none-eabihf.a   Cortex-M7 hard-float  ← linked by default
    libcawlib_0.0.1_thumbv7em-none-eabi.a     Cortex-M7 soft-float
    libcawlib_0.0.1_thumbv7m-none-eabi.a      Cortex-M3/M4 soft-float

Core/                     STM32CubeIDE / HAL generated files
Drivers/                  STM32H7 HAL + CMSIS
Middlewares/              FreeRTOS source
```

Integration entry point: `Core/Src/freertos.c` — `StartDefaultTask` calls `can1_init()` then `motor_host_start()` after the scheduler is running.

---

## cawlib_emb API Overview

### Motor Bus (`cawlib.h`)

The library manages up to **4 motor slots** per CAN bus frame. Each slot holds one motor's node ID, FOC mode, and current target.

```c
caw_motor_bus_t bus;

// Initialize the bus
caw_motor_bus_init(&bus);

// Register a motor: slot 0, node ID 0x200, position mode
caw_motor_bus_register(&bus, 0, 0x200, CAW_MOTOR_FOC_POSITION);

// Update the position target (radians)
caw_motor_bus_set_target(&bus, 0, 1.57f);

// Pack an 8-byte CAN payload and transmit at 1 kHz
uint8_t payload[8];
caw_motor_bus_build_broadcast(&bus, payload);
can1_tx_std8(0x100, payload);

// Feed received feedback frames back to the bus
caw_motor_bus_on_feedback(&bus, 0x200, rx_data);
```

FOC modes: `CAW_MOTOR_FOC_CURRENT`, `CAW_MOTOR_FOC_SPEED`, `CAW_MOTOR_FOC_POSITION`.

### Driver Control (`driver.h`)

Before the motor accepts FOC commands it must be placed into the correct mode through the **DR (Driver-control)** protocol — a lightweight CAN-based configuration session.

```c
uint16_t dest = caw_driver_control_encode_dest(0x200);

// 1. Open a unicast config session (two 8-byte frames)
uint8_t pair[16];
caw_driver_control_enter_configuring_unicast_pair(dest, pair);
can1_tx_std8(0x100, &pair[0]);
can1_tx_std8(0x100, &pair[8]);

// 2. Set FOC mode to position control
uint8_t cmd[8];
caw_driver_control_set_foc_mode(dest, CAW_DRIVER_CONTROL_FOC_MODE_POSITION, cmd);
can1_tx_std8(0x100, cmd);

// 3. Close the config session
uint8_t exit_cmd[8];
caw_driver_control_exit_configure(dest, exit_cmd);
can1_tx_std8(0x100, exit_cmd);
```

### Feedback Telemetry (`driver.h`)

Each motor node replies with an 8-byte frame that encodes position, speed, and current:

```c
caw_driver_feedback_t fb;
caw_driver_feedback_decode(rx_data, &fb);

float pos_rad   = fb.position_rad;
float speed_rps = fb.speed_rad_s;
float current_a = fb.current_a;
```

---

## Protocol Parameters

All bus and motion parameters are defined in `App/motor_host/motor_host.h`. Adjust them to match your hardware before building.

| Macro | Default | Description |
|-------|---------|-------------|
| `MOTOR_HOST_TX_STD_ID` | `0x100` | Standard CAN ID used for all host → motor frames |
| `MOTOR_NODE_ID` | `0x200` | Standard CAN ID of the motor node's feedback frame |
| `MOTOR_SLOT_INDEX` | `0` | Slot position in the 4-slot broadcast packet |
| `MOTOR_SINE_CENTER_RAD` | `0.0` | Sine trajectory center position (rad) |
| `MOTOR_SINE_AMPLITUDE_RAD` | `6.2832` | Sine trajectory amplitude ≈ ±2π rad (one full revolution) |
| `MOTOR_SINE_PERIOD_MS` | `4000` | Sine period in milliseconds |

---

## Build

Install **GNU Arm Embedded Toolchain** (`arm-none-eabi-gcc`), then from the repository root:

```bash
make -j
```

Build artifacts are placed in `build/`:

| File | Description |
|------|-------------|
| `build/cawlib-emb-examples.elf` | ELF with debug symbols |
| `build/cawlib-emb-examples.hex` | Intel HEX for most flash tools |
| `build/cawlib-emb-examples.bin` | Raw binary |

To clean:

```bash
make clean
```

### Selecting a Different Library Variant

The default build links `libcawlib_0.0.1_thumbv7em-none-eabihf.a` (Cortex-M7 with hardware FP). If you target a different core or ABI, edit the `LIBS` line in `Makefile`:

| Target | Library file | `LIBS` value |
|--------|-------------|------|
| Cortex-M7, hard-float (**default**) | `libcawlib_0.0.1_thumbv7em-none-eabihf.a` | `-lcawlib_0.0.1_thumbv7em-none-eabihf` |
| Cortex-M7, soft-float | `libcawlib_0.0.1_thumbv7em-none-eabi.a` | `-lcawlib_0.0.1_thumbv7em-none-eabi` |
| Cortex-M3 / M4, soft-float | `libcawlib_0.0.1_thumbv7m-none-eabi.a` | `-lcawlib_0.0.1_thumbv7m-none-eabi` |

---

## Runtime Notes

- **TIM2** provides the 1 kHz TX tick via update interrupt → binary semaphore → `MotorTx` task. The prescaler/ARR assume APB1 timer clock = 240 MHz (PLL1 on STM32H743 with default CubeIDE settings). Recalculate `MOTOR_TIM_PSC` / `MOTOR_TIM_ARR` if you change the clock tree.
- **FDCAN RX interrupt** priority is set to `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` so that `HAL_FDCAN_RxFifo0Callback` can safely call FreeRTOS `FromISR` APIs. Verify `FreeRTOSConfig.h` if you see assertion failures at startup.
- **Telemetry** from another task: `motor_feedback_buf_peek_latest(&fb)` returns the most recent decoded frame without removing it from the ring buffer (size 32, mutex-protected).
- The **DR bootstrap** sequence runs synchronously inside `motor_host_start()` before the TX/RX tasks are created. Short `vTaskDelay` gaps (`DR_AFTER_ENTER_MS`, `DR_AFTER_CMD_MS`, both 10 ms) give the motor node time to process each configuration command.

---

## Porting to Other Boards

1. **CAN pins / peripheral** — replace `App/hw_config/can1_pd0_pd1.c` with your own FDCAN init. Keep the `can1_tx_std8()` signature or update calls in `motor_host.c`.
2. **Clock tree** — recalculate `MOTOR_TIM_PSC` / `MOTOR_TIM_ARR` to maintain a 1 kHz timer interrupt.
3. **Library ABI** — select the matching prebuilt library from `cawlib_emb/library/` and update `LIBS` in `Makefile`.
4. **CAN IDs** — update `MOTOR_HOST_TX_STD_ID` and `MOTOR_NODE_ID` to match your bus topology.

---

## License

This project is licensed under the **MIT License**.

HAL and CMSIS files in `Drivers/` follow STMicroelectronics' license terms.  
FreeRTOS files in `Middlewares/` are licensed under the MIT License.
