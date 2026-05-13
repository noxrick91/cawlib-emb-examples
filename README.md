# cawlib-emb-examples

**English** | [简体中文](README.zh-CN.md)

STM32H7 firmware example that drives a motor over **classic CAN** using the **cawlib** C API (`cawlib_emb`). The application code under `App/` implements the host side: FDCAN1, periodic FOC broadcast, driver-control bootstrap, and telemetry buffering.

## App layout (motor demo)

| Path | Role |
|------|------|
| `App/hw_config/can1_pd0_pd1.c` | FDCAN1 init: **PD0 = RX**, **PD1 = TX**, **1 Mbit/s**, classic CAN only |
| `App/motor_host/motor_host.c` | TIM2 **1 kHz** → TX task → `caw_motor_bus_*` + CAN send; RX callback → queue → decode |
| `App/motor_host/motor_host.h` | **CAN IDs**, FOC slot index, sine demo parameters |
| `App/motor_host/motor_feedback_buf.c` | Thread-safe ring buffer of `caw_driver_feedback_t` |

Integration entry point: `Core/Src/freertos.c` — `StartDefaultTask` calls `can1_init()` then `motor_host_start()` after the scheduler is running.

### Protocol assumptions (tune in `motor_host.h`)

- Host transmits FOC / control frames with standard ID **`MOTOR_HOST_TX_STD_ID`** (`0x100`).
- The motor node answers with standard ID **`MOTOR_NODE_ID`** (`0x200`), **8-byte** payloads.
- The motor uses **slot index** `MOTOR_SLOT_INDEX` (`0`) in the 4-slot broadcast.
- Demo motion: sinusoidal **position** target — center `MOTOR_SINE_CENTER_RAD`, amplitude `MOTOR_SINE_AMPLITUDE_RAD` (default ±2π rad over `MOTOR_SINE_PERIOD_MS`).

Change these defines to match your bus and mechanical limits.

### Dependencies

- **cawlib** headers: `cawlib_emb/include/` (`cawlib.h`, `driver.h`)
- **Prebuilt static library** (link line in `Makefile`): `libcawlib_0.0.1_thumbv7em-none-eabihf.a`  
  Other triples in `cawlib_emb/library/` (`thumbv7em-none-eabi`, `thumbv7m-none-eabi`) are available if you change CPU/FPU settings.

### Build

Requires **GNU Arm Embedded** (`arm-none-eabi-gcc`). From the repo root:

```bash
make -j
```

Artifacts appear under `build/` (`*.elf`, `*.hex`, `*.bin`).

### Runtime notes

- **TIM2** is used for the 1 kHz tick (not the FreeRTOS timer service). Prescaler/ARR match the project clock tree (APB1 timer clock ×2 on STM32H7); recalculate if you change RCC.
- **FDCAN** RX interrupt priority is set so `HAL_FDCAN_RxFifo0Callback` can use FreeRTOS **FromISR** APIs (see `FreeRTOSConfig.h` and `can1_pd0_pd1.c`).
- Read latest telemetry from another task: `motor_feedback_buf_peek_latest()`.

## License

Add your license here. Cube/HAL and CMSIS files follow their respective vendors’ terms.
