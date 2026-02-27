# Recent Build Fixes

## What was changed

1. Updated case-sensitive platform paths in top-level CMake.
- `platforms/s32k5xx/...` -> `platforms/S32k5xx/...`
- Locations: `OPENBSW_PLATFORM_DIR`, `add_subdirectory(...)`, `bsp_can` source/include paths, linker script path.

2. Fixed case-sensitive BSP include path.
- `libs/bsp/bspCan/include` -> `libs/bsp/bspCAN/include`

3. Updated CAN app entry point for VDK builds.
- In `executables/s32k566CanApp/src/main.cpp`:
  - Added compile-time transceiver selection:
    - `VdkCanTransceiver` when `PLATFORM_VDK=1`
    - `S32K5FlexCanTransceiver` otherwise
  - Added `transceiver.poll()` in the main loop for VDK mode.

## Files modified
- `CMakeLists.txt`
- `executables/s32k566CanApp/src/main.cpp`

## Result
- Configure now succeeds for:
  - `-DBUILD_EXECUTABLE=s32k566CanApp`
  - `-DBUILD_TARGET_PLATFORM=S32K566EVB`
  - `-DPLATFORM_VDK=ON`
  - `-DCAN_LOOPBACK=ON`
- Build now succeeds:
  - `cmake --build build-host -j`
