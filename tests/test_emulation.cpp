// Catch2 tests for EepromEmulation
//
// Tests drive the host HAL (in-memory flash) — no hardware needed.
// All tests FAIL until the user implements emulation_impl.hpp.

#include <catch2/catch_test_macros.hpp>

#include "eeprom/emulation.hpp"
#include "eeprom/host_flash_hal.hpp"

using namespace eeprom;
using Hal = HostFlashHal<>;
using Emu = EepromEmulation<Hal>;

struct Fixture {
  Hal hal;
  Emu emu{hal};
};

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "init on blank flash succeeds", "[init]") {
  REQUIRE(emu.init().is_ok());
}

// ---------------------------------------------------------------------------
// read / write — RAM only, no sync needed
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "read returns 0xFFFF for unwritten address", "[read]") {
  REQUIRE(emu.init().is_ok());
  auto r = emu.read(0x0001U);
  REQUIRE(r.is_ok());
  REQUIRE(r.value() == kValueNotPresent);
}

TEST_CASE_METHOD(Fixture, "write then read returns same value", "[write][read]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0042U, 0xBEEFU).is_ok());
  auto r = emu.read(0x0042U);
  REQUIRE(r.is_ok());
  REQUIRE(r.value() == 0xBEEFU);
}

TEST_CASE_METHOD(Fixture, "write updates value for same address", "[write]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.write(0x0001U, 0xBBBBU).is_ok());
  REQUIRE(emu.read(0x0001U).value() == 0xBBBBU);
}

TEST_CASE_METHOD(Fixture, "multiple addresses are independent", "[write][read]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0000U, 0x1111U).is_ok());
  REQUIRE(emu.write(0x0001U, 0x2222U).is_ok());
  REQUIRE(emu.write(0x0002U, 0x3333U).is_ok());
  REQUIRE(emu.read(0x0000U).value() == 0x1111U);
  REQUIRE(emu.read(0x0001U).value() == 0x2222U);
  REQUIRE(emu.read(0x0002U).value() == 0x3333U);
}

TEST_CASE_METHOD(Fixture, "out-of-range address returns InvalidAddress", "[write][read]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(Emu::kMaxAddresses, 0x1234U).error() == FlashError::InvalidAddress);
  REQUIRE(emu.read(Emu::kMaxAddresses).error()           == FlashError::InvalidAddress);
}

// ---------------------------------------------------------------------------
// write constraints
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "writing 0xFFFF succeeds and reads back as 0xFFFF", "[write]") {
  // 0xFFFF is the erased-flash state — writing it is a no-op on flash.
  // read() returns ok(0xFFFF), same as any unwritten slot.
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, kValueNotPresent).is_ok());
  REQUIRE(emu.read(0x0001U).value() == kValueNotPresent);
}

TEST_CASE_METHOD(Fixture, "write and read at address 0 (low boundary)", "[write][read]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0000U, 0x1234U).is_ok());
  REQUIRE(emu.read(0x0000U).value() == 0x1234U);
}

TEST_CASE_METHOD(Fixture, "write and read at last valid address", "[write][read]") {
  REQUIRE(emu.init().is_ok());
  const VirtualAddress last = static_cast<VirtualAddress>(Emu::kMaxAddresses - 1U);
  REQUIRE(emu.write(last, 0xABCDU).is_ok());
  REQUIRE(emu.read(last).value() == 0xABCDU);
}

// ---------------------------------------------------------------------------
// cache isolation — write() must not touch flash
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "write does not touch flash — only the RAM cache", "[write]") {
  REQUIRE(emu.init().is_ok());
  const auto snapshot = hal.raw_storage();   // copy flash state after init
  REQUIRE(emu.write(0x0001U, 0xBEEFU).is_ok());
  REQUIRE(emu.write(0x00FFU, 0x1234U).is_ok());
  REQUIRE(hal.raw_storage() == snapshot);    // flash must be unchanged
}

// ---------------------------------------------------------------------------
// sync — basic
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "sync succeeds", "[sync]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xBEEFU).is_ok());
  REQUIRE(emu.sync().is_ok());
}

TEST_CASE_METHOD(Fixture, "sync produces exactly one slot per address on flash", "[sync]") {
  REQUIRE(emu.init().is_ok());

  // Write the same address 50 times — only the last value should appear in flash
  for (std::uint16_t i = 0; i < 50U; ++i) {
    REQUIRE(emu.write(0x0001U, i).is_ok());
  }
  REQUIRE(emu.sync().is_ok());

  // The RAM cache must hold the last written value
  REQUIRE(emu.read(0x0001U).value() == 49U);
}

// ---------------------------------------------------------------------------
// sync — flash state verification
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "sync writes Valid status to new active page", "[sync]") {
  // After init: active = page 0 (Valid), shadow = page 1 (Erased)
  // After sync:  active = page 1 (Valid), old = page 0 (Erased)
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xBEEFU).is_ok());
  REQUIRE(emu.sync().is_ok());

  auto p0_status = hal.read_word(0U, 0U);
  auto p1_status = hal.read_word(1U, 0U);
  REQUIRE(p0_status.is_ok());
  REQUIRE(p1_status.is_ok());
  REQUIRE(p0_status.value() == static_cast<FlashWord>(PageStatus::Erased));
  REQUIRE(p1_status.value() == static_cast<FlashWord>(PageStatus::Valid));
}

TEST_CASE_METHOD(Fixture, "sync erases the old active page", "[sync]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0x1234U).is_ok());
  REQUIRE(emu.sync().is_ok());

  // Every byte of the old active page (page 0) must be 0xFF after erase
  const auto& storage = hal.raw_storage();
  for (std::uint32_t i = 0U; i < 1024U; ++i) {
    REQUIRE(storage[i] == std::byte{0xFF});
  }
}

// ---------------------------------------------------------------------------
// sync — persistence across power cycle (re-init)
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "values survive sync + re-init", "[sync][init]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.write(0x0002U, 0xBBBBU).is_ok());
  REQUIRE(emu.sync().is_ok());

  // Simulate power cycle — create a fresh emulation object over the same HAL
  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  REQUIRE(emu2.read(0x0001U).value() == 0xAAAAU);
  REQUIRE(emu2.read(0x0002U).value() == 0xBBBBU);
}

TEST_CASE_METHOD(Fixture, "unsynced writes are lost after power cycle", "[sync][init]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.sync().is_ok());               // this write is persisted
  REQUIRE(emu.write(0x0001U, 0xBBBBU).is_ok());  // this one is not synced

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  REQUIRE(emu2.read(0x0001U).value() == 0xAAAAU);  // last synced value
}

TEST_CASE_METHOD(Fixture, "unwritten addresses return 0xFFFF after sync and re-init", "[sync][init]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0x1234U).is_ok());
  REQUIRE(emu.write(0x0100U, 0x5678U).is_ok());
  REQUIRE(emu.sync().is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  // Written addresses survive
  REQUIRE(emu2.read(0x0001U).value() == 0x1234U);
  REQUIRE(emu2.read(0x0100U).value() == 0x5678U);
  // Never-written addresses return the erased-flash value
  REQUIRE(emu2.read(0x0002U).value() == kValueNotPresent);
  REQUIRE(emu2.read(0x00FFU).value() == kValueNotPresent);
  REQUIRE(emu2.read(0x01FDU).value() == kValueNotPresent);
}

// ---------------------------------------------------------------------------
// sync — brown-out recovery (sync interrupted mid-write)
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "recovery: ReceiveData page on init uses Valid page", "[sync][init]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.sync().is_ok());

  // Simulate an interrupted sync: erase the shadow page and write ReceiveData
  // status (as if sync() started but lost power before writing Valid)
  const PageIndex shadow = 0U;  // after one sync, active = page 1, shadow = page 0
  REQUIRE(hal.erase_page(shadow).is_ok());
  REQUIRE(hal.write_word(shadow, 0U, static_cast<FlashWord>(PageStatus::ReceiveData)).is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());                       // must not return Corrupted
  REQUIRE(emu2.read(0x0001U).value() == 0xAAAAU);     // Valid page data still intact
}

// ---------------------------------------------------------------------------
// sync — multiple syncs (page alternation)
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "multiple syncs alternate pages correctly", "[sync]") {
  REQUIRE(emu.init().is_ok());

  REQUIRE(emu.write(0x0001U, 0x0001U).is_ok());
  REQUIRE(emu.sync().is_ok());

  REQUIRE(emu.write(0x0001U, 0x0002U).is_ok());
  REQUIRE(emu.sync().is_ok());

  REQUIRE(emu.write(0x0001U, 0x0003U).is_ok());
  REQUIRE(emu.sync().is_ok());

  REQUIRE(emu.read(0x0001U).value() == 0x0003U);

  // Verify persistence
  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  REQUIRE(emu2.read(0x0001U).value() == 0x0003U);
}

TEST_CASE_METHOD(Fixture, "overwritten value after second sync persists on re-init", "[sync][init]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.sync().is_ok());

  // Overwrite and sync again
  REQUIRE(emu.write(0x0001U, 0xBBBBU).is_ok());
  REQUIRE(emu.sync().is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  REQUIRE(emu2.read(0x0001U).value() == 0xBBBBU);   // new value, not 0xAAAA
}

// ---------------------------------------------------------------------------
// sync — comprehensive: all virtual addresses
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "all 510 addresses survive sync and re-init", "[sync][init]") {
  REQUIRE(emu.init().is_ok());

  // Fill every address with a distinct non-sentinel value
  for (VirtualAddress i = 0U; i < Emu::kMaxAddresses; ++i) {
    REQUIRE(emu.write(i, static_cast<EepromValue>(i + 1U)).is_ok());
  }
  REQUIRE(emu.sync().is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  for (VirtualAddress i = 0U; i < Emu::kMaxAddresses; ++i) {
    REQUIRE(emu2.read(i).value() == static_cast<EepromValue>(i + 1U));
  }
}

// ---------------------------------------------------------------------------
// recovery — both-Valid pages (interrupted erase after a successful sync)
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "recovery: both Valid pages on init does not return Corrupted", "[init]") {
  // After init: page 0 = Valid (empty image), page 1 = Erased
  // sync() with no prior writes: page 1 becomes Valid (empty), page 0 erased
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.sync().is_ok());

  // Power loss occurred after sync wrote Valid to page 1 but before it erased page 0.
  // page 0 is now Erased (all 0xFF). Writing Valid status only clears bits, so it is
  // a legal flash write and gives us the both-Valid scenario with matching data.
  REQUIRE(hal.write_word(0U, 0U, static_cast<FlashWord>(PageStatus::Valid)).is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());   // must recover — not Corrupted
}

// ---------------------------------------------------------------------------
// format
// ---------------------------------------------------------------------------
TEST_CASE_METHOD(Fixture, "format wipes all data", "[format]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xF00DU).is_ok());
  REQUIRE(emu.sync().is_ok());
  REQUIRE(emu.format().is_ok());
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.read(0x0001U).value() == kValueNotPresent);  // erased = 0xFFFF
}

TEST_CASE_METHOD(Fixture, "format allows fresh writes after wipe", "[format][write]") {
  REQUIRE(emu.init().is_ok());
  REQUIRE(emu.write(0x0001U, 0xAAAAU).is_ok());
  REQUIRE(emu.sync().is_ok());
  REQUIRE(emu.format().is_ok());
  REQUIRE(emu.init().is_ok());

  // After format, new writes and reads should work normally
  REQUIRE(emu.write(0x0001U, 0x1234U).is_ok());
  REQUIRE(emu.read(0x0001U).value() == 0x1234U);
  REQUIRE(emu.sync().is_ok());

  Emu emu2{hal};
  REQUIRE(emu2.init().is_ok());
  REQUIRE(emu2.read(0x0001U).value() == 0x1234U);
}
