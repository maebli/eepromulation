// QEMU semihosting demo — lm3s6965evb (Cortex-M3)
//
// Exercises init, read, write, sync (page-flip), boundary addresses,
// invalid-address rejection, and format+reinit.

#include <cstdio>
#include <cstdlib>

#include "eeprom/emulation.hpp"
#include "eeprom/host_flash_hal.hpp"

extern "C" void initialise_monitor_handles(void);

using namespace eeprom;
using Emu = EepromEmulation<HostFlashHal<>>;

static int failures   = 0;
static int section_no = 0;

static void section(const char* title) {
  std::printf("\n[%d] %s\n", ++section_no, title);
}

static void check(bool condition, const char* msg) {
  if (!condition) {
    std::printf("  [FAIL] %s\n", msg);
    ++failures;
  } else {
    std::printf("  [PASS] %s\n", msg);
  }
}

int main() {
  initialise_monitor_handles();
  std::printf("eepromulation QEMU demo\n");
  std::printf("========================\n");

  HostFlashHal hal;
  Emu          emu{hal};

  // ------------------------------------------------------------------
  section("init — first boot, both pages erased");
  check(emu.init().is_ok(), "init() ok");

  // Unwritten address returns kValueNotPresent (0xFFFF)
  auto u = emu.read(0x0010U);
  check(u.is_ok() && u.value() == kValueNotPresent, "unwritten addr == 0xFFFF");

  // ------------------------------------------------------------------
  section("basic write / read");
  check(emu.write(0x0001U, 0xBEEFU).is_ok(), "write 0x0001 = 0xBEEF");
  check(emu.write(0x0002U, 0xF00DU).is_ok(), "write 0x0002 = 0xF00D");
  check(emu.write(0x0003U, 0x5678U).is_ok(), "write 0x0003 = 0x5678");

  check(emu.read(0x0001U).value_or(0) == 0xBEEFU, "read 0x0001 == 0xBEEF");
  check(emu.read(0x0002U).value_or(0) == 0xF00DU, "read 0x0002 == 0xF00D");
  check(emu.read(0x0003U).value_or(0) == 0x5678U, "read 0x0003 == 0x5678");

  // ------------------------------------------------------------------
  section("overwrite in RAM cache");
  check(emu.write(0x0001U, 0xFF00U).is_ok(), "write 0x0001 = 0xFF00");
  check(emu.read(0x0001U).value_or(0) == 0xFF00U, "read 0x0001 == 0xFF00");

  // ------------------------------------------------------------------
  section("sync — first page flip (page 0 → page 1)");
  check(emu.sync().is_ok(), "sync() ok");

  // Data must survive the page flip
  check(emu.read(0x0001U).value_or(0) == 0xFF00U, "read 0x0001 == 0xFF00 after sync");
  check(emu.read(0x0002U).value_or(0) == 0xF00DU, "read 0x0002 == 0xF00D after sync");
  check(emu.read(0x0003U).value_or(0) == 0x5678U, "read 0x0003 == 0x5678 after sync");

  // ------------------------------------------------------------------
  section("write after sync, then second page flip (page 1 → page 0)");
  check(emu.write(0x0004U, 0xCAFEU).is_ok(), "write 0x0004 = 0xCAFE");
  check(emu.write(0x0005U, 0xDEADU).is_ok(), "write 0x0005 = 0xDEAD");
  check(emu.sync().is_ok(), "sync() ok");

  check(emu.read(0x0001U).value_or(0) == 0xFF00U, "read 0x0001 == 0xFF00 after 2nd sync");
  check(emu.read(0x0004U).value_or(0) == 0xCAFEU, "read 0x0004 == 0xCAFE after 2nd sync");
  check(emu.read(0x0005U).value_or(0) == 0xDEADU, "read 0x0005 == 0xDEAD after 2nd sync");

  // ------------------------------------------------------------------
  section("boundary addresses (0 and 509)");
  check(emu.write(0x0000U, 0xAA55U).is_ok(), "write addr 0 = 0xAA55");
  check(emu.write(kMaxVirtualAddresses - 1U, 0x55AAU).is_ok(), "write addr 509 = 0x55AA");
  check(emu.read(0x0000U).value_or(0) == 0xAA55U, "read addr 0 == 0xAA55");
  check(emu.read(kMaxVirtualAddresses - 1U).value_or(0) == 0x55AAU, "read addr 509 == 0x55AA");

  // ------------------------------------------------------------------
  section("invalid address rejected");
  check(emu.write(Emu::kMaxAddresses, 0x1234U).is_err(), "write addr 510 -> err");
  auto inv = emu.read(Emu::kMaxAddresses);
  check(inv.is_err() && inv.error() == FlashError::InvalidAddress,
        "read addr 510 -> InvalidAddress");

  // ------------------------------------------------------------------
  section("fill all 510 addresses and sync");
  for (VirtualAddress a = 0; a < Emu::kMaxAddresses; ++a) {
    emu.write(a, static_cast<EepromValue>(a ^ 0xA5A5U));
  }
  check(emu.sync().is_ok(), "sync() after bulk fill ok");

  bool bulk_ok = true;
  for (VirtualAddress a = 0; a < Emu::kMaxAddresses; ++a) {
    if (emu.read(a).value_or(0) != static_cast<EepromValue>(a ^ 0xA5A5U)) {
      bulk_ok = false;
      break;
    }
  }
  check(bulk_ok, "all 510 addresses read back correctly after sync");

  // ------------------------------------------------------------------
  section("format + reinit — clean slate");
  check(emu.format().is_ok(), "format() ok");
  check(emu.init().is_ok(), "init() after format ok");

  // Everything should be gone
  check(emu.read(0x0001U).value_or(0) == kValueNotPresent, "0x0001 erased after format");
  check(emu.read(0x0004U).value_or(0) == kValueNotPresent, "0x0004 erased after format");

  // ------------------------------------------------------------------
  std::printf("\n========================\n");
  std::printf("Result: %d failure(s)\n", failures);

  std::fflush(nullptr);
  std::exit(failures == 0 ? 0 : 1);
}
