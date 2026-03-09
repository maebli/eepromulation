#!/usr/bin/env python3
"""Generate dist/eeprom.h and dist/eepromulation.cpp.

Both files together are a drop-in replacement for ST's AN2594 eeprom.h /
eeprom.c pair.  Copy them into any STM32WL project, compile
eepromulation.cpp as C++20, and call EE_Init / EE_ReadVariable /
EE_WriteVariable / EE_Sync / EE_Format from C exactly as before.

Usage:
    python3 tools/amalgamate.py          # writes dist/
    python3 tools/amalgamate.py --check  # exits non-zero if dist/ is stale
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
ROOT    = Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "include" / "eeprom"
SRC     = ROOT / "src"
DIST    = ROOT / "dist"

# Internal headers to amalgamate, in dependency order.
# The expander deduplicates automatically, so listing them explicitly just
# ensures the right top-level comment order.
HEADER_ROOTS = [
    INCLUDE / "types.hpp",
    INCLUDE / "hal.hpp",
    INCLUDE / "emulation.hpp",        # pulls in emulation_impl.hpp at its end
    INCLUDE / "stm32wl_flash_hal.hpp",
]

CONFIG_BLOCK = """\
// =============================================================================
// CONFIGURATION — set page indices to match your device and linker script.
//
//   STM32WL55JCxx  256 KB → EEPROM_PAGE0_IDX=126  EEPROM_PAGE1_IDX=127  (default)
//   STM32WL55JBxx  128 KB → EEPROM_PAGE0_IDX=62   EEPROM_PAGE1_IDX=63
//   STM32WL55JAxx   64 KB → EEPROM_PAGE0_IDX=30   EEPROM_PAGE1_IDX=31
//   Any two pages anywhere → pass their indices
//
// Override at compile time with -DEEPROM_PAGE0_IDX=N -DEEPROM_PAGE1_IDX=M,
// or edit here.  Indices must match the EEPROM region in your linker script.
// =============================================================================
#ifndef EEPROM_PAGE0_IDX
#  define EEPROM_PAGE0_IDX 126U
#endif
#ifndef EEPROM_PAGE1_IDX
#  define EEPROM_PAGE1_IDX 127U
#endif

"""

COMPAT_BLOCK = """
// =============================================================================
// C API — EE_Init / EE_ReadVariable / EE_WriteVariable / EE_Sync / EE_Format
// =============================================================================
#include "eeprom.h"

using _HalType = eeprom::Stm32WlFlashHal;
using _EmuType = eeprom::EepromEmulation<_HalType, 2048U>;

static _HalType _g_hal{EEPROM_PAGE0_IDX, EEPROM_PAGE1_IDX};
static _EmuType _g_emu{_g_hal};

static uint16_t _to_ee_error(eeprom::FlashError e) {
  switch (e) {
    case eeprom::FlashError::NoSpaceLeft: return EE_PAGE_FULL;
    default:                              return EE_NO_VALID_PAGE;
  }
}

extern "C" uint16_t EE_Init(void) {
  auto r = _g_emu.init();
  return r.is_ok() ? EE_OK : _to_ee_error(r.error());
}

extern "C" uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t *Data) {
  auto r = _g_emu.read(static_cast<eeprom::VirtualAddress>(VirtAddress));
  if (!r.is_ok()) return _to_ee_error(r.error());
  if (r.value() == eeprom::kValueNotPresent) return EE_NOT_FOUND;
  *Data = r.value();
  return EE_OK;
}

extern "C" uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data) {
  auto r = _g_emu.write(static_cast<eeprom::VirtualAddress>(VirtAddress),
                        static_cast<eeprom::EepromValue>(Data));
  return r.is_ok() ? EE_OK : _to_ee_error(r.error());
}

extern "C" uint16_t EE_Sync(void) {
  auto r = _g_emu.sync();
  return r.is_ok() ? EE_OK : _to_ee_error(r.error());
}

extern "C" uint16_t EE_Format(void) {
  auto r = _g_emu.format();
  return r.is_ok() ? EE_OK : _to_ee_error(r.error());
}
"""

# ---------------------------------------------------------------------------
# Expander
# ---------------------------------------------------------------------------
_seen: set[Path] = set()


def _expand(path: Path) -> list[str]:
    """Return lines of `path` with internal includes expanded inline."""
    path = path.resolve()
    if path in _seen:
        return []
    _seen.add(path)

    out: list[str] = []
    rel = path.relative_to(ROOT)
    out.append(f"// ── begin {rel} {'─' * max(0, 74 - len(str(rel)))}")

    for line in path.read_text().splitlines():
        # Drop #pragma once — deduplication is handled by _seen
        if re.fullmatch(r"\s*#\s*pragma\s+once\s*", line):
            continue

        # Expand internal includes
        m = re.match(r'^\s*#\s*include\s+"eeprom/(.+?)"', line)
        if m:
            inner = INCLUDE / m.group(1)
            if not inner.exists():
                sys.exit(f"ERROR: {path}: cannot find {inner}")
            out.extend(_expand(inner))
            continue

        out.append(line)

    out.append(f"// ── end {rel} {'─' * max(0, 76 - len(str(rel)))}")
    return out


# ---------------------------------------------------------------------------
# Generators
# ---------------------------------------------------------------------------
def build_header() -> str:
    src = (INCLUDE / "an2594_compat.h").resolve().read_text()
    banner = (
        "// eeprom.h — drop-in replacement for ST's AN2594 eeprom.h\n"
        "//\n"
        "// Same return codes, same function names.  Three extensions added:\n"
        "//   EE_Configure(p0, p1) — set flash page indices before EE_Init (optional)\n"
        "//   EE_Sync()            — flush RAM cache to flash (call from PVD ISR or periodically)\n"
        "//   EE_Format()          — erase both pages; emulator is immediately ready afterwards\n"
        "//\n"
        "// Option A — prebuilt library (no C++ needed):\n"
        "//   Copy eeprom.h + libeepromulation_stm32wl.a, link with -leepromulation_stm32wl.\n"
        "//\n"
        "// Option B — compile it yourself:\n"
        "//   Copy eeprom.h + eepromulation.cpp, compile eepromulation.cpp as C++20.\n"
        "//\n"
        "// Virtual address range: 0 – 1021 (1022 total, PageSize = 2048).\n"
    )
    return banner + src


def build_impl() -> str:
    _seen.clear()

    lines: list[str] = []
    lines.append(
        "// eepromulation.cpp — amalgamated C++20 EEPROM emulation for STM32WL\n"
        "//\n"
        "// Generated by tools/amalgamate.py — do not edit by hand.\n"
        "// Regenerate with: python3 tools/amalgamate.py\n"
        "//\n"
        "// Build requirements:\n"
        "//   compiler : arm-none-eabi-g++ (or any C++20-capable cross compiler)\n"
        "//   standard : -std=c++20\n"
        "//   linker   : arm-none-eabi-g++ as linker driver (links libstdc++ automatically)\n"
        "//\n"
        "// In CMake:\n"
        "//   target_sources(your_target PRIVATE eepromulation.cpp)\n"
        "//   set_source_files_properties(eepromulation.cpp PROPERTIES LANGUAGE CXX)\n"
        "//   set_property(TARGET your_target PROPERTY CXX_STANDARD 20)\n"
        "//   set_target_properties(your_target PROPERTIES LINKER_LANGUAGE CXX)\n"
        "//\n"
        "// In a Makefile:\n"
        "//   Compile:  arm-none-eabi-g++ -std=c++20 -c eepromulation.cpp\n"
        "//   Link:     arm-none-eabi-g++ ... eepromulation.o your_objects.o\n"
    )

    lines.append(CONFIG_BLOCK)

    for root in HEADER_ROOTS:
        lines.extend(_expand(root))
        lines.append("")

    lines.append(COMPAT_BLOCK)

    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero if dist/ would change (CI use)")
    args = parser.parse_args()

    header_text = build_header()
    impl_text   = build_impl()

    if args.check:
        stale = False
        for path, text in [(DIST / "eeprom.h",           header_text),
                           (DIST / "eepromulation.cpp",  impl_text)]:
            if not path.exists() or path.read_text() != text:
                print(f"STALE: {path.relative_to(ROOT)}")
                stale = True
        if stale:
            sys.exit(1)
        print("dist/ is up to date")
        return

    DIST.mkdir(exist_ok=True)
    (DIST / "eeprom.h").write_text(header_text)
    (DIST / "eepromulation.cpp").write_text(impl_text)
    print(f"wrote {DIST / 'eeprom.h'}")
    print(f"wrote {DIST / 'eepromulation.cpp'}")


if __name__ == "__main__":
    main()
