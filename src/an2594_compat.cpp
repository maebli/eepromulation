// AN2594 C compatibility wrapper
//
// Exposes EE_Init / EE_ReadVariable / EE_WriteVariable with the exact
// signatures from the AN2594 application note, backed by the C++20
// EepromEmulation implementation.
//
// HAL selection at compile time:
//   -DEEPROM_USE_STM32_HAL   → Stm32FlashHal  (ARM build)
//   default                  → HostFlashHal   (host/tests/QEMU)

#include "eeprom/an2594_compat.h"

#include "eeprom/emulation.hpp"

#ifdef EEPROM_USE_STM32_HAL
#  include "eeprom/stm32_flash_hal.hpp"
using HalType = eeprom::Stm32FlashHal;
#else
#  include "eeprom/host_flash_hal.hpp"
using HalType = eeprom::HostFlashHal<>;
#endif

// ---------------------------------------------------------------------------
// Single global instance — matches AN2594's implicit global state model.
// ---------------------------------------------------------------------------
static HalType g_hal;
static eeprom::EepromEmulation<HalType> g_emu{g_hal};

// ---------------------------------------------------------------------------
static uint16_t to_ee_error(eeprom::FlashError e) {
  switch (e) {
    case eeprom::FlashError::NoSpaceLeft: return EE_PAGE_FULL;
    default:                              return EE_NO_VALID_PAGE;
  }
}

// ---------------------------------------------------------------------------
extern "C" uint16_t EE_Init(void) {
  auto r = g_emu.init();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data) {
  auto r = g_emu.read(static_cast<eeprom::VirtualAddress>(VirtAddress));
  if (!r.is_ok()) {
    return to_ee_error(r.error());
  }
  if (r.value() == eeprom::kValueNotPresent) {
    return EE_NOT_FOUND;  // never written, or last written with 0xFFFF sentinel
  }
  *Data = r.value();
  return EE_OK;
}

extern "C" uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data) {
  auto r = g_emu.write(static_cast<eeprom::VirtualAddress>(VirtAddress),
                       static_cast<eeprom::EepromValue>(Data));
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_Sync(void) {
  auto r = g_emu.sync();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

extern "C" uint16_t EE_Format(void) {
  auto r = g_emu.format();
  return r.is_ok() ? EE_OK : to_ee_error(r.error());
}

// EE_Configure is a no-op for the host HAL — page placement is not
// configurable on the in-memory flash backend used for tests and QEMU.
extern "C" void EE_Configure(uint16_t /*page0_idx*/, uint16_t /*page1_idx*/) {}
