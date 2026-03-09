// C API glue for the STM32WL example.
//
// Drop this file into your project instead of src/an2594_compat.cpp.
// It provides EE_Configure / EE_Init / EE_ReadVariable / EE_WriteVariable /
// EE_Sync / EE_Format backed by EepromEmulation<Stm32WlFlashHal, 2048>.
//
// IMPORTANT: this is a C++ translation unit.  Your build must:
//   - compile it with arm-none-eabi-g++ (not gcc)
//   - use -std=c++20
//   - link with arm-none-eabi-g++ as the linker driver (pulls in libstdc++)
//
// Flash page placement
// --------------------
// Call EE_Configure(page0, page1) before EE_Init() to place the EEPROM pages
// anywhere in flash.  If not called, the compile-time defaults are used:
//
//   STM32WL55JCxx  256 KB → pages 126, 127  (default)
//   STM32WL55JBxx  128 KB → pages  62,  63
//   STM32WL55JAxx   64 KB → pages  30,  31
//
// Override the defaults at compile time with:
//   -DEEPROM_PAGE0_IDX=62 -DEEPROM_PAGE1_IDX=63
//
// Page indices must match the EEPROM region in your linker script.

#include <optional>

#include "eeprom/an2594_compat.h"
#include "eeprom/emulation.hpp"
#include "eeprom/stm32wl_flash_hal.hpp"

using HalType = eeprom::Stm32WlFlashHal;
using EmuType = eeprom::EepromEmulation<HalType, 2048U>;

#ifndef EEPROM_PAGE0_IDX
#  define EEPROM_PAGE0_IDX 126U
#endif
#ifndef EEPROM_PAGE1_IDX
#  define EEPROM_PAGE1_IDX 127U
#endif

static std::uint32_t       g_page0{EEPROM_PAGE0_IDX};
static std::uint32_t       g_page1{EEPROM_PAGE1_IDX};
static std::optional<HalType> g_hal;
static std::optional<EmuType> g_emu;

// Construct hal + emu on first use (or after EE_Configure resets them).
static EmuType& emu() {
  if (!g_hal) {
    g_hal.emplace(g_page0, g_page1);
    g_emu.emplace(*g_hal);
  }
  return *g_emu;
}

static uint16_t to_ee_error(eeprom::FlashError e) {
  switch (e) {
    case eeprom::FlashError::NoSpaceLeft: return EE_PAGE_FULL;
    default:                              return EE_NO_VALID_PAGE;
  }
}

extern "C" void EE_Configure(uint16_t page0_idx, uint16_t page1_idx) {
  g_page0 = page0_idx;
  g_page1 = page1_idx;
  // Reset emu before hal — emu holds a Hal& that would dangle otherwise.
  g_emu.reset();
  g_hal.reset();
}

extern "C" uint16_t EE_Init(void) {
  auto r = emu().init();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data) {
  auto r = emu().read(static_cast<eeprom::VirtualAddress>(VirtAddress));
  if (!r.is_ok()) { return to_ee_error(r.error()); }
  if (r.value() == eeprom::kValueNotPresent) { return EE_NOT_FOUND; }
  *Data = r.value();
  return EE_OK;
}

extern "C" uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data) {
  auto r = emu().write(static_cast<eeprom::VirtualAddress>(VirtAddress),
                       static_cast<eeprom::EepromValue>(Data));
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_Sync(void) {
  auto r = emu().sync();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_Format(void) {
  auto r = emu().format();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}
