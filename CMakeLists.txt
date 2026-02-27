cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

# We do not support modules at the moment
set(CMAKE_CXX_SCAN_FOR_MODULES FALSE)

if (CMAKE_CXX_STANDARD AND NOT ${CMAKE_CXX_STANDARD} EQUAL 14)
    message(WARNING "Using custom CXX standard (C++${CMAKE_CXX_STANDARD})")
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()

if (CMAKE_C_STANDARD AND NOT ${CMAKE_C_STANDARD} EQUAL 99)
    message(WARNING "Using custom C standard (C${CMAKE_C_STANDARD})")
    set(CMAKE_C_STANDARD_REQUIRED ON)
else ()
    set(CMAKE_C_STANDARD 99)
endif ()

set(BUILD_TARGET_PLATFORM
    "POSIX"
    CACHE STRING "Target Platform")
set(BUILD_TARGET_RTOS
    "FREERTOS"
    CACHE STRING "Target RTOS")

set(BUILD_EXECUTABLE
    ""
    CACHE STRING "Which build to enable: referenceApp, unitTest, s32k566CanApp")

if (BUILD_EXECUTABLE STREQUAL "referenceApp")
    # no action for referenceApp build
elseif (BUILD_EXECUTABLE STREQUAL "unitTest")
    add_compile_definitions(UNIT_TEST=1)
elseif (BUILD_EXECUTABLE STREQUAL "s32k566CanApp")
    add_compile_definitions(S32K566_CAN_APP=1)
endif ()

# Convert target name to lower case to match directory name
string(TOLOWER "${BUILD_TARGET_PLATFORM}" OPENBSW_TARGET)

if (BUILD_EXECUTABLE STREQUAL "referenceApp")
    include(executables/referenceApp/platforms/${OPENBSW_TARGET}/Options.cmake)
elseif (BUILD_EXECUTABLE STREQUAL "s32k566CanApp")
    # Options for S32K566 CAN application are set below after project()
endif ()

# Configure async and Rtos libs depending on target rtos.
if (BUILD_TARGET_RTOS STREQUAL "FREERTOS")
    add_compile_definitions(SUPPORT_FREERTOS)
elseif (BUILD_TARGET_RTOS STREQUAL "THREADX")
    add_compile_definitions(SUPPORT_THREADX)
endif ()

message(STATUS "Target platform: <${BUILD_TARGET_PLATFORM}>")
message(STATUS "Target RTOS: <${BUILD_TARGET_RTOS}>")

# Set preprocessor defines based on cmake options by the same name to configure
# platform features.
get_cmake_property(_all_vars VARIABLES)
foreach (var ${_all_vars})
    if (var MATCHES "^PLATFORM_SUPPORT_")
        if (${var})
            add_compile_definitions(${var}=1)
            message(STATUS "${var} ON")
        endif ()
    endif ()
endforeach ()

project(
    "Eclipse OpenBSW"
    VERSION 0.1.0
    DESCRIPTION
        "BSW workspace with reference application for multiple platforms"
    LANGUAGES C CXX ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

option(BUILD_TRACING "Build CTF tracing" OFF)

option(BUILD_RUST "Build Rust components" OFF)

add_compile_options(
    #[[
        Supressing stringop-overflow and maybe-uninitialized, as they are often
        false-positive.
    ]]
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wno-error=stringop-overflow;-Wno-error=maybe-uninitialized>"
)

# Rust support Note: Only one static Rust library can be linked in the final
# binary, otherwise the linker will complain about duplicate symbol definitions
# from the Rust core crate.
if (BUILD_RUST)
    # Set Rust target for cross-compilation
    if (BUILD_TARGET_PLATFORM STREQUAL "S32K148EVB")
        set(Rust_CARGO_TARGET
            "thumbv7em-none-eabihf"
            CACHE STRING "Rust target triple")
    elseif (BUILD_TARGET_PLATFORM STREQUAL "S32K566EVB")
        # Cortex-M7 with double-precision FPU — same ABI as M4F thumb target
        set(Rust_CARGO_TARGET
            "thumbv7em-none-eabihf"
            CACHE STRING "Rust target triple")
    endif ()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/3rdparty/corrosion)

    corrosion_import_crate(
        MANIFEST_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/executables/referenceApp/rustHelloWorld/Cargo.toml
    )

endif ()

if (BUILD_EXECUTABLE STREQUAL "unitTest")
    include(GoogleTest)
    include(CTest)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    enable_testing()

    set(OPENBSW_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/executables/unitTest")
    set(OPENBSW_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platforms/posix")
endif ()

if (BUILD_EXECUTABLE STREQUAL "referenceApp")
    set(OPENBSW_APP_DIR "${CMAKE_CURRENT_LIST_DIR}/executables/referenceApp")
    if (BUILD_TARGET_PLATFORM STREQUAL "S32K148EVB")
        set(OPENBSW_PLATFORM_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/platforms/s32k1xx")
    elseif (BUILD_TARGET_PLATFORM STREQUAL "S32K566EVB")
        set(OPENBSW_PLATFORM_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/platforms/S32k5xx")
    else ()
        set(OPENBSW_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platforms/posix")
    endif ()
endif ()

# ---------------------------------------------------------------------------
# S32K566 CAN Application build
#   cmake -S . -B build-s32k566 \
#         -DBUILD_EXECUTABLE=s32k566CanApp \
#         -DBUILD_TARGET_PLATFORM=S32K566EVB \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake
#
#   VDK (host) build — no cross-compiler needed:
#   cmake -S . -B build-s32k566-vdk \
#         -DBUILD_EXECUTABLE=s32k566CanApp \
#         -DBUILD_TARGET_PLATFORM=S32K566EVB \
#         -DPLATFORM_VDK=ON
# ---------------------------------------------------------------------------
if (BUILD_EXECUTABLE STREQUAL "s32k566CanApp")
    set(OPENBSW_APP_DIR
        "${CMAKE_CURRENT_LIST_DIR}/executables/s32k566CanApp")
    set(OPENBSW_PLATFORM_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/platforms/S32k5xx")

    option(PLATFORM_VDK  "Build for Synopsys VDK virtual platform" OFF)
    option(CAN_LOOPBACK  "Enable FlexCAN loopback for self-test"   OFF)

    if (PLATFORM_VDK)
        add_compile_definitions(PLATFORM_VDK=1)
        message(STATUS "S32K566 CAN App: VDK simulation build")
    else ()
        message(STATUS "S32K566 CAN App: hardware build (arm-none-eabi)")
    endif ()

    if (CAN_LOOPBACK)
        add_compile_definitions(CAN_LOOPBACK=1)
        message(STATUS "S32K566 CAN App: FlexCAN loopback enabled")
    endif ()
endif ()

if (BUILD_TRACING)
    add_compile_definitions(TRACING=1)
    if (TRACING_BUFFER_SIZE)
        add_compile_definitions(TRACING_BUFFER_SIZE=${TRACING_BUFFER_SIZE})
    endif ()
endif ()

set(INCLUDE_OPENBSW_LIBS_BSP
    ON
    CACHE BOOL "Include openbsw/libs/bsp/ in build")

add_subdirectory(platforms EXCLUDE_FROM_ALL)
add_subdirectory(libs EXCLUDE_FROM_ALL)

if (BUILD_EXECUTABLE STREQUAL "unitTest")

    add_subdirectory(executables/unitTest EXCLUDE_FROM_ALL)
    target_include_directories(etl BEFORE
                               INTERFACE executables/unitTest/etl_profile)

    if (OPENBSW_PLATFORM STREQUAL "posix")
        add_subdirectory(libs/bsp/bspCharInputOutput/test)
        add_subdirectory(libs/bsp/bspDynamicClient/test)
        add_subdirectory(libs/bsp/bspInputManager/test)
        add_subdirectory(libs/bsp/bspInterrupts/test)
        add_subdirectory(libs/bsp/bspOutputManager/test)
        add_subdirectory(libs/bsp/bspOutputPwm/test)

        add_subdirectory(libs/bsw/async/test)
        add_subdirectory(libs/bsw/asyncConsole/test)
        add_subdirectory(libs/bsw/asyncFreeRtos/test)
        add_subdirectory(libs/bsw/asyncImpl/examples)
        add_subdirectory(libs/bsw/asyncImpl/test)
        add_subdirectory(libs/bsw/bsp/test)
        add_subdirectory(libs/bsw/cpp2can/test)
        add_subdirectory(libs/bsw/cpp2ethernet/test)
        add_subdirectory(libs/bsw/docan/test)
        add_subdirectory(libs/bsw/doip/test)
        add_subdirectory(libs/bsw/io/examples)
        add_subdirectory(libs/bsw/io/test)
        add_subdirectory(libs/bsw/lifecycle/examples)
        add_subdirectory(libs/bsw/lifecycle/test)
        add_subdirectory(libs/bsw/logger/test)
        add_subdirectory(libs/bsw/lwipSocket/test)
        add_subdirectory(libs/bsw/middleware/test)
        add_subdirectory(libs/bsw/platform/test)
        add_subdirectory(libs/bsw/runtime/test)
        add_subdirectory(libs/bsw/storage/test)
        add_subdirectory(libs/bsw/timer/test)
        add_subdirectory(libs/bsw/transport/test)
        add_subdirectory(libs/bsw/uds/test)
        add_subdirectory(libs/bsw/util/test)

        add_subdirectory(libs/safety/safeMonitor/test)
        add_subdirectory(libs/safety/safeUtils/test)

        add_subdirectory(platforms/posix/unitTest EXCLUDE_FROM_ALL)

        add_subdirectory(platforms/posix/bsp/bspEepromDriver/test)
        add_subdirectory(platforms/posix/bsp/socketCanTransceiver/test)

    elseif (OPENBSW_PLATFORM STREQUAL "s32k1xx")

        add_subdirectory(platforms/s32k1xx/unitTest EXCLUDE_FROM_ALL)

        add_subdirectory(platforms/s32k1xx/bsp/bspCore/test)
        add_subdirectory(platforms/s32k1xx/bsp/bspEepromDriver/test)
        add_subdirectory(platforms/s32k1xx/bsp/bspIo/test)
        add_subdirectory(platforms/s32k1xx/bsp/canflex2Transceiver/test)
        add_subdirectory(platforms/s32k1xx/bsp/bspUart/test)

    elseif (OPENBSW_PLATFORM STREQUAL "s32k5xx")
        # -----------------------------------------------------------------------
        # S32K566 platform unit tests
        # -----------------------------------------------------------------------
        add_subdirectory(platforms/S32k5xx/unitTest EXCLUDE_FROM_ALL)

        add_subdirectory(platforms/S32k5xx/bsp/bspCan/test)

    else ()
        message(
            FATAL_ERROR
                "Invalid value for OPENBSW_PLATFORM: ${OPENBSW_PLATFORM}")
    endif ()

elseif (BUILD_EXECUTABLE STREQUAL "referenceApp")
    target_include_directories(etl BEFORE
                               INTERFACE executables/referenceApp/etl_profile)
    add_subdirectory(executables/referenceApp)

elseif (BUILD_EXECUTABLE STREQUAL "s32k566CanApp")
    # -------------------------------------------------------------------------
    # S32K566 CAN Application
    # -------------------------------------------------------------------------
    # OpenBSW CAN interface headers (CANFrame, ICanTransceiver, ICANFrameListener)
    add_library(openbsw_can INTERFACE)
    target_include_directories(openbsw_can INTERFACE
        libs/bsp/bspCAN/include
    )

    if (PLATFORM_VDK)
        # VDK build: host GCC, TCP-socket-based CAN transceiver
        add_library(bsp_can
            platforms/S32k5xx/bsp/bspCan/VdkCanTransceiver.cpp
        )
        target_include_directories(bsp_can PUBLIC
            platforms/S32k5xx/bsp/bspCan
            platforms/S32k5xx/bsp/bspCan/include
        )
    else ()
        # Hardware build: register-level FlexCAN_0 driver for S32K566
        add_library(bsp_can
            platforms/S32k5xx/bsp/bspCan/S32K5FlexCanTransceiver.cpp
            platforms/S32k5xx/startup/startup_s32k566.cpp
            platforms/S32k5xx/startup/system_s32k566.cpp
        )
        target_include_directories(bsp_can PUBLIC
            platforms/S32k5xx/bsp/bspCan
            platforms/S32k5xx/bsp/bspCan/include
            platforms/S32k5xx/include
        )
        target_link_options(bsp_can INTERFACE
            -T ${CMAKE_CURRENT_SOURCE_DIR}/platforms/S32k5xx/linker/S32K566_flash.ld
            -Wl,--gc-sections
            -Wl,-Map=${CMAKE_BINARY_DIR}/s32k566_can_app.map
        )
    endif ()

    target_link_libraries(bsp_can PUBLIC openbsw_can)

    # Application library
    add_library(app_can
        executables/s32k566CanApp/src/can/CanApplication.cpp
    )
    target_include_directories(app_can PUBLIC
        executables/s32k566CanApp/src
    )
    target_link_libraries(app_can PUBLIC openbsw_can)

    # Final executable
    add_executable(s32k566_can_app
        executables/s32k566CanApp/src/main.cpp
    )
    target_link_libraries(s32k566_can_app
        PRIVATE app_can bsp_can openbsw_can
    )

    message(STATUS "")
    message(STATUS "=== S32K566 CAN Application ===")
    message(STATUS "  VDK build    : ${PLATFORM_VDK}")
    message(STATUS "  CAN loopback : ${CAN_LOOPBACK}")
    message(STATUS "  Output ELF   : s32k566_can_app")
    message(STATUS "================================")
    message(STATUS "")

endif ()
