# S32K566 CAN App Build and Run How-To

This guide documents how to build and validate the `s32k566CanApp` in this repository, with focus on the S32K5 platform implementation under `platforms/S32k5xx`.

## 1. What Gets Built

The executable is `s32k566_can_app`, selected through:

- `-DBUILD_EXECUTABLE=s32k566CanApp`
- `-DBUILD_TARGET_PLATFORM=S32K566EVB`

Build composition is defined in the root [CMakeLists.txt](/home/kj/openbsw/CMakeLists.txt):

- `openbsw_can` (interface headers): `libs/bsp/bspCAN/include`
- `app_can`: [executables/s32k566CanApp/src/can/CanApplication.cpp](/home/kj/openbsw/executables/s32k566CanApp/src/can/CanApplication.cpp)
- `bsp_can`:
- VDK-host build (`PLATFORM_VDK=ON`): [platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp)
- Hardware/ARM build (`PLATFORM_VDK=OFF`): [platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp), [platforms/S32k5xx/startup/startup_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/startup_s32k566.cpp), [platforms/S32k5xx/startup/system_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/system_s32k566.cpp)
- Linker script (hardware build): [platforms/S32k5xx/linker/S32K566_flash.ld](/home/kj/openbsw/platforms/S32k5xx/linker/S32K566_flash.ld)

## 2. Build Modes

There are two different and valid outputs:

1. Host VDK-stub binary (x86_64 on Linux):
- `PLATFORM_VDK=ON`
- Used for local execution and loopback testing without ARM cross-compilation.

2. Hardware/VDK target binary (ARM ELF):
- `PLATFORM_VDK=OFF`
- Built with Arm GNU toolchain and loaded into Synopsys VDK model.

## 3. Prerequisites

Minimum tools:

- `cmake` (3.28+ expected by repo)
- native `gcc/g++` for host build
- `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-size` for ARM build
- `python3` for CAN stimulus script

Optional/runtime tools:

- `virtualizer_exec` (Synopsys Virtualizer)
- S32K566 model binary (`S32K566_VDK.so`)
- `gdb-multiarch` or `arm-none-eabi-gdb`

Quick checks:

```bash
cmake --version
arm-none-eabi-gcc --version
python3 --version
which virtualizer_exec
```

## 4. Host Build (VDK Stub Path, No Synopsys Needed)

Use this for rapid software-level validation.

### 4.1 Configure

```bash
cmake -S . -B build-host \
  -DBUILD_EXECUTABLE=s32k566CanApp \
  -DBUILD_TARGET_PLATFORM=S32K566EVB \
  -DPLATFORM_VDK=ON \
  -DCAN_LOOPBACK=ON \
  -DCMAKE_BUILD_TYPE=Debug
```

Expected configure lines:

- `S32K566 CAN App: VDK simulation build`
- `S32K566 CAN App: FlexCAN loopback enabled`
- `VDK build    : ON`
- `CAN loopback : ON`

### 4.2 Build

```bash
cmake --build build-host --target s32k566_can_app -j$(nproc)
```

### 4.3 Validate binary type

```bash
file build-host/s32k566_can_app
```

Expected:

- `ELF 64-bit ... x86-64 ...`

### 4.4 Run

```bash
./build-host/s32k566_can_app
```

Notes:

- With `CAN_LOOPBACK=ON`, the runtime loopback path in [VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp) bypasses socket connect and self-dispatches frames.
- The current app has no default `printf`, so use debugger breakpoints for visibility.

### 4.5 Debug validation (recommended)

```bash
gdb ./build-host/s32k566_can_app
```

In gdb:

```gdb
break app::CanApplication::sendHeartbeat
break app::CanApplication::canFrameReceived
run
continue
```

If both breakpoints hit repeatedly, Tx and Rx processing paths are active.

## 5. ARM Build (Hardware/Target Path)

This produces an ARM ELF for real hardware or Synopsys VDK target execution.

### 5.1 Configure

```bash
cmake -S . -B build-vdk \
  -DBUILD_EXECUTABLE=s32k566CanApp \
  -DBUILD_TARGET_PLATFORM=S32K566EVB \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DPLATFORM_VDK=OFF \
  -DCAN_LOOPBACK=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

Expected configure lines:

- `S32K566 CAN App: hardware build (arm-none-eabi)`
- `VDK build    : OFF`

### 5.2 Build

```bash
cmake --build build-vdk --target s32k566_can_app -j$(nproc)
```

### 5.3 Validate binary type and size

```bash
file build-vdk/s32k566_can_app
arm-none-eabi-size build-vdk/s32k566_can_app
```

Expected:

- ARM ELF (`ELF 32-bit ... ARM ...`)
- Size output with `text/data/bss`.

## 6. Run on Synopsys VDK (When Installed)

Prerequisite:

- `virtualizer_exec` on `PATH`
- Valid model path to `S32K566_VDK.so`

### 6.1 Launch VDK model (terminal 1)

```bash
virtualizer_exec \
  --model /absolute/path/to/S32K566_VDK.so \
  --elf build-vdk/s32k566_can_app \
  --params vdk/s32k566_params.yaml
```

### 6.2 Connect debugger (terminal 2)

```bash
gdb-multiarch build-vdk/s32k566_can_app
```

Then:

```gdb
set architecture arm
target remote localhost:2345
monitor reset halt
load
break app::CanApplication::canFrameReceived
continue
```

### 6.3 Inject CAN traffic (terminal 3)

```bash
python3 vdk/can_stimulus.py --host 127.0.0.1 --port 9000
```

Ports are from [vdk/s32k566_params.yaml](/home/kj/openbsw/vdk/s32k566_params.yaml):

- CAN: `9000`
- UART: `3000`
- GDB: `2345`

## 7. Common Problems and Fixes

1. `build-vdk is not a directory`
- You configured `build-host` but tried to build `build-vdk`.
- Fix: run the corresponding configure command first, or use the right build dir.

2. `CMAKE_TOOLCHAIN_FILE ... not used by the project`
- Usually means reusing an existing cache where toolchain is already locked.
- Fix: use a fresh build dir:
```bash
rm -rf build-vdk
cmake -S . -B build-vdk ...
```

3. `arm-none-eabi-size: file format not recognized`
- You ran `arm-none-eabi-size` on host x86 binary (`build-host/...`).
- Fix: run it on `build-vdk/s32k566_can_app`.

4. `virtualizer_exec: command not found`
- Synopsys Virtualizer is not installed or not on `PATH`.

5. `connect() — is VDK running?: Connection refused`
- Happens if `PLATFORM_VDK=ON` and loopback is not enabled, or VDK bus not active.
- For local no-VDK tests use `-DCAN_LOOPBACK=ON`.

## 8. File Map (Platform-Centric)

Key S32K5 platform files:

- [platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.h](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.h)
- [platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp)
- [platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.h](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.h)
- [platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp](/home/kj/openbsw/platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp)
- [platforms/S32k5xx/startup/startup_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/startup_s32k566.cpp)
- [platforms/S32k5xx/startup/system_s32k566.cpp](/home/kj/openbsw/platforms/S32k5xx/startup/system_s32k566.cpp)
- [platforms/S32k5xx/include/S32K566_registers.h](/home/kj/openbsw/platforms/S32k5xx/include/S32K566_registers.h)
- [platforms/S32k5xx/linker/S32K566_flash.ld](/home/kj/openbsw/platforms/S32k5xx/linker/S32K566_flash.ld)

