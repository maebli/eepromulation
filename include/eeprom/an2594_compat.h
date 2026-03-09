#pragma once

#include <stdint.h>

/* Return codes matching AN2594 exactly */
#define EE_OK           ((uint16_t)0x0000)
#define EE_PAGE_FULL    ((uint16_t)0x0080)
#define EE_NO_VALID_PAGE ((uint16_t)0x00AB)
#define EE_NOT_FOUND    ((uint16_t)0x00CD)

#ifdef __cplusplus
extern "C" {
#endif

uint16_t EE_Init(void);

/* Read a virtual address.
 * Returns EE_OK and writes the value to *Data on success.
 * Returns EE_NOT_FOUND (and leaves *Data unchanged) if the address has never
 * been written or was last written with 0xFFFF (the erased-flash sentinel).
 * Callers that need to store 0xFFFF as a real value must use a different
 * out-of-band mechanism to distinguish "never written" from "written 0xFFFF". */
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data);
uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data);

/* Extension — not in original AN2594.
 * Flush RAM cache to flash. Call from a PVD brown-out ISR or at known
 * safe points. Writes only (no erase), safe within a ~few-ms hold-up window. */
uint16_t EE_Sync(void);

/* Extension — not in original AN2594.
 * Erase both pages and reinitialise. All stored values are lost.
 * Use on deliberate factory-reset; not needed on normal power-up
 * (EE_Init handles the first-boot case automatically). */
uint16_t EE_Format(void);

/* Extension — not in original AN2594.
 * Set the physical flash page indices used for EEPROM emulation.
 * Must be called BEFORE EE_Init(). If not called the defaults compiled
 * into the library are used (126 and 127 for the STM32WL55JCxx 256 KB).
 *
 * Common values:
 *   STM32WL55JCxx  256 KB → EE_Configure(126, 127)  (default)
 *   STM32WL55JBxx  128 KB → EE_Configure( 62,  63)
 *   STM32WL55JAxx   64 KB → EE_Configure( 30,  31)
 *
 * Page indices must match the EEPROM region in your linker script. */
void EE_Configure(uint16_t page0_idx, uint16_t page1_idx);

#ifdef __cplusplus
}
#endif
