// STM32WL eepromulation example — minimal bare-metal demo
//
// Uses EepromEmulation<Stm32WlFlashHal, 2048> for 1022 virtual addresses.
// Formats both EEPROM pages, writes a few values, syncs to flash, reads back.
//
// Result in g_result (inspect with a debugger at the infinite loop):
//   0 = pass,  1 = fail

#include "eeprom/emulation.hpp"
#include "eeprom/stm32wl_flash_hal.hpp"

using namespace eeprom;
using Hal = Stm32WlFlashHal;
using Emu = EepromEmulation<Hal, 2048U>;

volatile int g_result = -1;

static bool run_test() {
  Hal hal;
  Emu emu{hal};

  if (!emu.format().is_ok()) return false;
  if (!emu.init().is_ok())   return false;

  if (!emu.write(0x0001U, 0xBEEFU).is_ok()) return false;
  if (!emu.write(0x0002U, 0xCAFEU).is_ok()) return false;
  if (!emu.write(0x0003U, 0x1234U).is_ok()) return false;

  if (!emu.sync().is_ok()) return false;

  if (emu.read(0x0001U).value_or(0) != 0xBEEFU) return false;
  if (emu.read(0x0002U).value_or(0) != 0xCAFEU) return false;
  if (emu.read(0x0003U).value_or(0) != 0x1234U) return false;

  // Overwrite and second page flip
  if (!emu.write(0x0001U, 0xDEADU).is_ok()) return false;
  if (!emu.sync().is_ok()) return false;
  if (emu.read(0x0001U).value_or(0) != 0xDEADU) return false;

  return true;
}

int main() {
  g_result = run_test() ? 0 : 1;
  for (;;) {}
}
