include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/ArmNoneEabi-header.cmake")

# NOTE: we only check the C compiler, since CMake deduces the others
# automatically from it (respecting the order in the project call).

if (NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    message(FATAL_ERROR "CC environment variable must be set")
endif ()

set(_C_FLAGS "-funsigned-bitfields")

set(_CXX_FLAGS "-femit-class-debug-always -funsigned-bitfields")

set(_EXE_LINKER_FLAGS "-specs=nano.specs -specs=nosys.specs")

include("${CMAKE_CURRENT_LIST_DIR}/ArmNoneEabi.cmake")
