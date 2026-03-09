cmake_minimum_required(VERSION 3.25)

project(eepromulation
  LANGUAGES CXX ASM
  VERSION 0.1.0
  DESCRIPTION "EEPROM emulation in C++20 (AN2594 port)"
)

# ---------------------------------------------------------------------------
# C++20 globally
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(HOST_BUILD              "Build for the host (enables tests)"     ON)
option(WARNINGS_AS_ERRORS      "Treat compiler warnings as errors"      OFF)
option(BUILD_EXAMPLE_STM32WL   "Build the STM32WL bare-metal example"  OFF)

# ---------------------------------------------------------------------------
# Shared tooling (warning flags + compile_commands.json)
# ---------------------------------------------------------------------------
include(cmake/warnings.cmake)

# ---------------------------------------------------------------------------
# Public header-only interface library
# ---------------------------------------------------------------------------
add_library(eeprom_headers INTERFACE)
target_include_directories(eeprom_headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# ---------------------------------------------------------------------------
# Emulation implementation library
# ---------------------------------------------------------------------------
add_library(eeprom_emulation STATIC
  src/emulation.cpp
)
target_link_libraries(eeprom_emulation PUBLIC eeprom_headers)
target_apply_warnings(eeprom_emulation)

# ---------------------------------------------------------------------------
# AN2594 C compatibility wrapper (optional — same API as the ST reference)
# ---------------------------------------------------------------------------
add_library(eeprom_an2594_compat STATIC
  src/an2594_compat.cpp
)
target_link_libraries(eeprom_an2594_compat PUBLIC eeprom_emulation)
target_apply_warnings(eeprom_an2594_compat)
# C-style casts in the AN2594 macros are intentional (header must compile as C)
target_compile_options(eeprom_an2594_compat PRIVATE -Wno-old-style-cast)

# ---------------------------------------------------------------------------
# Host HAL (compiled on both host and ARM for QEMU demo)
# ---------------------------------------------------------------------------
add_library(eeprom_hal_host STATIC
  src/hal/host/flash_hal.cpp
)
target_link_libraries(eeprom_hal_host PUBLIC eeprom_headers)
target_apply_warnings(eeprom_hal_host)

# ---------------------------------------------------------------------------
# Catch2 (fetched if not provided by Nix / vcpkg)
# ---------------------------------------------------------------------------
if(HOST_BUILD)
  find_package(Catch2 3 QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
  endif()

  enable_testing()
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# ARM targets — only in cross builds
# ---------------------------------------------------------------------------
if(NOT HOST_BUILD)
  if(BUILD_EXAMPLE_STM32WL)
    add_subdirectory(examples/stm32wl)
  else()
    add_subdirectory(target)   # default: lm3s6965evb QEMU demo
  endif()
endif()

# ---------------------------------------------------------------------------
# clang-tidy custom target (host build only)
# ---------------------------------------------------------------------------
if(HOST_BUILD)
  find_program(CLANG_TIDY clang-tidy)
  if(CLANG_TIDY)
    # The Nix clang wrapper adds implicit include dirs (e.g. libc++ headers) at
    # runtime, but these do not appear in compile_commands.json.  Pass them
    # explicitly so clang-tidy can resolve system headers like <cstdint>.
    set(_ct_extra_args "")
    foreach(_dir IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
      list(APPEND _ct_extra_args "--extra-arg=-isystem${_dir}")
    endforeach()

    add_custom_target(clang-tidy
      COMMAND ${CLANG_TIDY}
        -p ${CMAKE_BINARY_DIR}
        ${_ct_extra_args}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/emulation.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/host/flash_hal.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_emulation.cpp
      COMMENT "Running clang-tidy"
      VERBATIM
    )
  endif()

  find_program(CPPCHECK cppcheck)
  if(CPPCHECK)
    add_custom_target(cppcheck
      COMMAND ${CPPCHECK}
        --enable=all
        --std=c++20
        --suppress=missingIncludeSystem
        --suppress=unusedFunction
        --suppress=unusedPrivateFunction
        --suppress=constParameterReference
        -I ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
      COMMENT "Running cppcheck"
      VERBATIM
    )
  endif()
endif()
