# S32K566 CAN Application Deep Dive

This document explains the `s32k566CanApp` architecture and behavior, with emphasis on the S32K5 platform folder: `platforms/S32k5xx`.

## 1. Scope and Intent

The application is a small CAN demo that:

- sends a heartbeat every 10 ms (`ID=0x100`)
- sends a status/response frame every 100 ms or on command (`ID=0x101`)
- receives command/config frames (`ID=0x200`, `ID=0x201`)
- runs against either:
- real FlexCAN_0 hardware driver (`S32K5FlexCanTransceiver`)
- VDK socket-based transceiver (`VdkCanTransceiver`)

The application logic is platform-agnostic and depends only on `ICanTransceiver`.

## 2. High-Level Architecture

Core modules:

1. Interface layer (shared):
- [CANFrame.h](/home/kj/openbsw/libs/bsp/bspCAN/include/can/canframes/CANFrame.h)
- [ICanTransceiver.h](/home/kj/openbsw/libs/bsp/bspCAN/include/can/transceiver/ICanTransceiver.h)
- [ICANFrameListener.h](/home/kj/openbsw/libs/bsp/bspCAN/include/can/transceiver/ICANFrameListener.h)

2. Application layer:
- [CanApplication.h](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.h)
- [CanApplication.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.cpp)
- [main.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/main.cpp)

3. Platform layer (`platforms/S32k5xx`):
- hardware transceiver: [S32K5FlexCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp)
- VDK transceiver: [VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp)
- startup: [startup_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/startup_s32k566.cpp)
- clocks/init: [system_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/system_s32k566.cpp)
- register map: [S32K566_registers.h](/home/kj/openbsw/platforms/S32k5xx/include/S32K566_registers.h)
- linker: [S32K566_flash.ld](/home/kj/openbsw/platforms/S32k5xx/linker/S32K566_flash.ld)

## 3. Boot and Runtime Flow

### 3.1 Hardware build flow (`PLATFORM_VDK=OFF`)

1. CPU enters `Reset_Handler` in startup.
2. `.data` copied MRAM -> SRAM, `.bss` zeroed.
3. `SystemInit()` configures oscillator/PLL/clock routing.
4. `main()` executes.
5. `main()` creates transceiver + `CanApplication`.
6. `CanApplication::init()` initializes transceiver and registers listener.
7. Super-loop triggers periodic tasks (`task10ms`, `task100ms`).
8. Rx IRQ (`FlexCAN0_ORed_0_15_MB_IRQHandler`) calls `handleRxIrq()`.
9. Received frame is dispatched to listeners -> `CanApplication::canFrameReceived()`.

### 3.2 VDK host flow (`PLATFORM_VDK=ON`)

1. `main()` selects `VdkCanTransceiver`.
2. If `CAN_LOOPBACK=1`, `main()` calls `transceiver.setLoopback(true)`.
3. `CanApplication::init()` calls transceiver `init()`.
4. In normal VDK mode, transceiver opens TCP socket to CAN bus port `9000`.
5. In loopback mode, no socket is opened; written frames are self-dispatched.
6. Super-loop calls `transceiver.poll()` each 10 ms to process incoming socket frames.

## 4. Application-Level Protocol

Defined in [CanApplication.h](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.h):

CAN IDs:

- `0x100` (`TX_HEARTBEAT`)
- `0x101` (`TX_RESPONSE`)
- `0x200` (`RX_COMMAND`)
- `0x201` (`RX_CONFIG`)

Command bytes (first payload byte on `0x200`):

- `0x01` START
- `0x02` STOP
- `0x03` RESET
- `0x04` STATUS

## 5. Function-by-Function Behavior

## 5.1 `CanApplication` behavior

From [CanApplication.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.cpp):

1. `init()`
- calls `_transceiver.init()`
- registers self via `_transceiver.addCANFrameListener(*this)`
- sets `_running=true`

2. `task10ms()`
- returns if not running
- increments `_tick10ms`
- sends heartbeat frame

3. `task100ms()`
- returns if not running
- sends status response with code `0xAA`

4. `sendHeartbeat()`
- sends `ID=0x100`
- payload:
- bytes 0-1: TX count (LE)
- bytes 2-3: RX count (LE)
- byte 4: running flag
- byte 5: tick low byte
- bytes 6-7: `0xBE 0xEF`

5. `sendResponse(statusByte)`
- sends `ID=0x101`
- byte 0 echoes status/cmd
- byte 1 reflects running flag
- bytes 2-5 hold TX counter (big-endian)

6. `canFrameReceived(frame)`
- increments `_rxCount`
- routes by ID:
- `0x200` -> `handleCommand`
- `0x201` -> `handleConfig`

7. `handleCommand(frame)`
- START: set running, send ACK `0x01`
- STOP: clear running, send ACK `0x02`
- RESET: clear counters/tick, running true, send ACK `0x03`
- STATUS: send ACK `0x04`
- unknown: send NACK `0xFF`

8. `handleConfig(frame)`
- placeholder implementation
- requires payload length >= 2
- responds with `0x55`

## 5.2 `main()` behavior

From [main.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/main.cpp):

1. Compile-time transceiver selection:
- `PLATFORM_VDK=1` -> `VdkCanTransceiver`
- else -> `S32K5FlexCanTransceiver`

2. Optional loopback:
- enabled when `CAN_LOOPBACK=1`

3. Super-loop:
- delays 10 ms (busy-wait)
- in VDK mode: calls `poll()` to fetch socket frames
- calls `task10ms()` every loop
- calls `task100ms()` every 10 loops

## 6. S32K5 Hardware Transceiver Internals

Implementation: [S32K5FlexCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp)

### 6.1 Initialization sequence (`init`)

Key steps:

1. Enable peripheral clock (`PCC`, `PCS=SPLLDIV2`).
2. Configure SIUL2 pin mux for CAN0 TX/RX.
3. Wake module, soft reset, enter freeze mode.
4. Set MCR:
- individual mask enabled (`IRMQ`)
- supervisor mode (`SUPV`)
- max MB = 15
- self-reception disabled unless loopback
5. Set `CTRL1` for 500 kbps @ 80 MHz clock.
6. Clear/disable interrupts, invalidate MBs.
7. Configure:
- MB0 as TX
- MB4 as RX (`acceptId=0x200`, mask `0x7FC` to accept `0x200` and `0x201`)
8. Enable MB4 interrupt.
9. Configure NVIC IRQ 109 priority and enable.
10. Exit freeze mode and mark initialized.

### 6.2 Transmission (`write`)

1. Verify initialized and input validity.
2. Check MB0 CODE to avoid collision (`CAN_ERR_TX_BUSY` if transmitting).
3. Write standard ID into bits `[28:18]`.
4. Pack payload bytes into MB words.
5. Set CS CODE `0xC` to trigger transmission.

### 6.3 Reception (`handleRxIrq`)

1. Check MB4 interrupt flag.
2. Read CS/ID/DATA words (locks MB).
3. Read timer to unlock MB.
4. Extract ID and DLC.
5. Reconstruct payload.
6. Build `CANFrame`, dispatch to listeners.
7. Clear MB4 interrupt flag (W1C).

### 6.4 Error handling (`handleErrorIrq`)

- reads ESR1 and writes back to clear flags
- hook exists for future error counters and recovery

## 7. VDK Transceiver Internals

Implementation: [VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp)

Wire frame is fixed 16 bytes:

- `uint32 id`
- `uint8 dlc`
- `uint8 flags` (`FD`, `BRS`, `ExtID`)
- `uint8 pad[2]`
- `uint8 data[8]`

Behavior:

1. `init()`
- if runtime loopback enabled, skip socket setup
- else connect to `127.0.0.1:9000`, set nonblocking

2. `write()`
- if loopback enabled, dispatch directly to listeners
- else serialize and send 16-byte wire frame

3. `poll()`
- nonblocking recv loop
- convert incoming wire frames into `CANFrame`
- dispatch to listeners

## 8. Startup, Clocks, Memory

### 8.1 Startup

[startup_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/startup_s32k566.cpp):

- defines vector table (system exceptions + key FlexCAN IRQs)
- reset handler copies `.data`, clears `.bss`, calls `SystemInit()`, then `main()`

### 8.2 System clock

[system_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/system_s32k566.cpp):

- enables FXOSC (16 MHz)
- configures PLL (VCO 400 MHz, output 200 MHz)
- switches core clock mux to PLL source
- intended peripheral path for FlexCAN clock around SPLLDIV2 domain

### 8.3 Linker memory layout

[S32K566_flash.ld](/home/kj/openbsw/platforms/S32k5xx/linker/S32K566_flash.ld):

- `MRAM`: `0x00400000`, size `4M`
- `SRAM`: `0x20000000`, size `1M`
- vector table and text in MRAM
- `.data` load in MRAM, run in SRAM
- `.bss`, heap, stack in SRAM
- stack size `8 KB`, heap size `4 KB`

## 9. Build-Time Switching and Its Effect

Relevant CMake logic in [CMakeLists.txt](/home/kj/openbsw/CMakeLists.txt):

1. `PLATFORM_VDK=ON`
- compiles `VdkCanTransceiver.cpp`
- defines `PLATFORM_VDK=1`

2. `PLATFORM_VDK=OFF`
- compiles hardware transceiver + startup/system files
- uses linker script from `platforms/S32k5xx/linker`

3. `CAN_LOOPBACK=ON`
- adds compile define `CAN_LOOPBACK=1`
- `main()` enables transceiver loopback at runtime

## 10. Known Behavioral Boundaries

Current implementation limits:

1. Rx filtering is narrow at hardware driver level (configured around command/config IDs).
2. App filter is `AcceptAllFilter` and relies on ID switch inside app logic.
3. No RTOS queueing; app logic currently executes in simple super-loop model.
4. Default app has no diagnostic `printf` output path.
5. VDK script examples assume Synopsys runtime is installed and reachable.

## 11. Fast Navigation Table

- Build wiring: [CMakeLists.txt](/home/kj/openbsw/CMakeLists.txt)
- App entry and scheduler: [main.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/main.cpp)
- App protocol logic: [CanApplication.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.cpp)
- Hardware CAN driver: [S32K5FlexCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp)
- VDK CAN driver: [VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp)
- Startup/clock: [startup_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/startup_s32k566.cpp), [system_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/system_s32k566.cpp)
- Register constants: [S32K566_registers.h](/home/kj/openbsw/platforms/S32k5xx/include/S32K566_registers.h)
- Linker map: [S32K566_flash.ld](/home/kj/openbsw/platforms/S32k5xx/linker/S32K566_flash.ld)

