/* STM32WL eepromulation C example
 *
 * Uses the AN2594-compatible C API: EE_Init / EE_ReadVariable /
 * EE_WriteVariable / EE_Sync.  The C++ glue (stm32wl_compat.cpp) backs
 * these with EepromEmulation<Stm32WlFlashHal, 2048>.
 *
 * Inspect g_result with a debugger at the infinite loop:
 *   0 = pass,  1 = fail
 */

#include <stdint.h>

#include "eeprom/an2594_compat.h"

volatile int g_result = -1;

static int run_test(void) {
  uint16_t val;

  if (EE_Init() != EE_OK) return 0;

  if (EE_WriteVariable(0x0001U, 0xBEEFU) != EE_OK) return 0;
  if (EE_WriteVariable(0x0002U, 0xCAFEU) != EE_OK) return 0;
  if (EE_WriteVariable(0x0003U, 0x1234U) != EE_OK) return 0;

  if (EE_Sync() != EE_OK) return 0;

  if (EE_ReadVariable(0x0001U, &val) != EE_OK || val != 0xBEEFU) return 0;
  if (EE_ReadVariable(0x0002U, &val) != EE_OK || val != 0xCAFEU) return 0;
  if (EE_ReadVariable(0x0003U, &val) != EE_OK || val != 0x1234U) return 0;

  /* Overwrite and second page flip */
  if (EE_WriteVariable(0x0001U, 0xDEADU) != EE_OK) return 0;
  if (EE_Sync() != EE_OK) return 0;
  if (EE_ReadVariable(0x0001U, &val) != EE_OK || val != 0xDEADU) return 0;

  return 1;
}

int main(void) {
  g_result = run_test() ? 0 : 1;
  for (;;) {}
}
