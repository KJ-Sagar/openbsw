# =============================================================================
# FILE    : CMakeLists.txt  (project root)
# PURPOSE : Top-level CMake build for the S32K566 CAN application on OpenBSW.
#
# Quick-start:
#   # Hardware build (requires arm-none-eabi-gcc):
#   cmake -S . -B build \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
#         -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
#
#   # VDK simulation build (host GCC, no cross-compiler needed):
#   cmake -S . -B build-vdk \
#         -DPLATFORM_VDK=ON \
#         -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build-vdk
# =============================================================================

cmake_minimum_required(VERSION 3.22)

project(S32K566_CAN_OpenBSW
    VERSION 1.0.0
    LANGUAGES C CXX ASM
    DESCRIPTION "Simple CAN send/receive application on OpenBSW for NXP S32K566"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(PLATFORM_VDK "Build for Synopsys VDK virtual platform" OFF)
option(CAN_LOOPBACK "Enable FlexCAN loopback for self-test"   OFF)

# ---------------------------------------------------------------------------
# Common compiler warnings (apply to all targets)
# ---------------------------------------------------------------------------
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -ffunction-sections
    -fdata-sections
)

# ---------------------------------------------------------------------------
# OpenBSW CAN interface library
#   Headers only — provides CANFrame, ICanTransceiver, ICANFrameListener
# ---------------------------------------------------------------------------
add_library(openbsw_can INTERFACE)
target_include_directories(openbsw_can INTERFACE
    libs/bsp/bspCan/include
)

# ---------------------------------------------------------------------------
# BSP: S32K566 FlexCAN driver  (or VDK stub)
# ---------------------------------------------------------------------------
if(PLATFORM_VDK)
    # VDK build: use the VDK stub transceiver instead of hardware driver
    add_library(bsp_can
        bsp/vdk/VdkCanTransceiver.cpp
    )
    target_include_directories(bsp_can PUBLIC
        bsp/vdk
        bsp/s32k566/include   # still need register defs for reference
    )
    target_compile_definitions(bsp_can PUBLIC PLATFORM_VDK=1)
else()
    # Hardware build
    add_library(bsp_can
        bsp/s32k566/bspCan/S32K5FlexCanTransceiver.cpp
        bsp/s32k566/startup/startup_s32k566.cpp
        bsp/s32k566/startup/system_s32k566.cpp
    )
    target_include_directories(bsp_can PUBLIC
        bsp/s32k566/bspCan
        bsp/s32k566/include
    )
endif()

target_link_libraries(bsp_can PUBLIC openbsw_can)

# ---------------------------------------------------------------------------
# Application library
# ---------------------------------------------------------------------------
add_library(app_can
    app/src/can/CanApplication.cpp
)
target_include_directories(app_can PUBLIC
    app/src
)
target_link_libraries(app_can PUBLIC openbsw_can)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(s32k566_can_app
    app/src/main.cpp
)

target_link_libraries(s32k566_can_app
    PRIVATE
        app_can
        bsp_can
        openbsw_can
)

if(CAN_LOOPBACK)
    target_compile_definitions(s32k566_can_app PRIVATE CAN_LOOPBACK=1)
endif()

# ---------------------------------------------------------------------------
# Linker script and map file (hardware build only)
# ---------------------------------------------------------------------------
if(NOT PLATFORM_VDK)
    target_link_options(s32k566_can_app PRIVATE
        -T ${CMAKE_SOURCE_DIR}/bsp/s32k566/linker/S32K566_flash.ld
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_BINARY_DIR}/s32k566_can_app.map
        -Wl,--print-memory-usage
    )

    # Generate .hex and .bin alongside the ELF
    include(cmake/toolchains/arm-none-eabi.cmake OPTIONAL)
    if(COMMAND target_generate_bin_hex)
        target_generate_bin_hex(s32k566_can_app)
    endif()
endif()

# ---------------------------------------------------------------------------
# Print summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== S32K566 CAN Build Configuration ===")
message(STATUS "  Platform VDK : ${PLATFORM_VDK}")
message(STATUS "  CAN Loopback : ${CAN_LOOPBACK}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")
message(STATUS "")
