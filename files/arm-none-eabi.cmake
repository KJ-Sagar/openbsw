# =============================================================================
# FILE    : cmake/toolchains/arm-none-eabi.cmake
# PURPOSE : CMake toolchain file for the Arm GNU Toolchain (bare-metal).
#           Targets the S32K566 Cortex-M7 in Thumb-2, hard-float ABI.
#
# Usage:
#   cmake -S . -B build \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake
# =============================================================================

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ---------------------------------------------------------------------------
# Toolchain prefix — adjust if your toolchain is installed elsewhere.
# The Arm GNU Toolchain 13.x names the executables with this prefix.
# ---------------------------------------------------------------------------
set(TOOLCHAIN_PREFIX arm-none-eabi-)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc   REQUIRED)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++   REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc   REQUIRED)
find_program(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
find_program(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---------------------------------------------------------------------------
# CPU / FPU flags common to all targets
# ---------------------------------------------------------------------------
set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)
string(JOIN " " CPU_FLAGS_STR ${CPU_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS_STR} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS_STR} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${CPU_FLAGS_STR} -specs=nosys.specs -specs=nano.specs")

# ---------------------------------------------------------------------------
# Default build-type flags (override per project if needed)
# ---------------------------------------------------------------------------
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG   "-Og -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ---------------------------------------------------------------------------
# Sysroot / search paths (host tools must not be found)
# ---------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# Helper to create a .bin and .hex alongside the ELF
# ---------------------------------------------------------------------------
function(target_generate_bin_hex TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}>
                $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}>
                $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
        COMMENT "Generating HEX and BIN for ${TARGET}"
    )
endfunction()
