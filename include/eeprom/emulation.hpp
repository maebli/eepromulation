#pragma once

#include <cstdint>

#include "eeprom/hal.hpp"
#include "eeprom/types.hpp"

namespace eeprom {

// ---------------------------------------------------------------------------
// EepromEmulation<Hal, PageSize> — write-back RAM cache with atomic sync.
//
// PageSize (bytes) is a compile-time parameter; defaults to kPageSize (1024).
// Larger values give more virtual addresses at the cost of more flash:
//   PageSize = 1024 → 510  virtual addresses
//   PageSize = 2048 → 1022 virtual addresses
//
// The HAL's write_page() decides the physical write granularity (32-bit for
// host/QEMU, 64-bit packed for STM32WL).  write_word() is called only for
// the isolated status-word promotion (ReceiveData → Valid) at the end of sync.
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize = kPageSize>
class EepromEmulation {
 public:
  using Image = PageImage<PageSize>;
  static constexpr std::uint16_t kMaxAddresses = Image::kMaxAddresses;

  explicit EepromEmulation(Hal& hal) noexcept : hal_{hal} {}

  // Must be called once before any read/write/sync.
  [[nodiscard]] auto init() -> Result<void>;

  // O(1) RAM lookup. Returns InvalidAddress if addr >= kMaxAddresses.
  [[nodiscard]] auto read(VirtualAddress addr) const -> Result<EepromValue>;

  // Updates RAM image only — no flash access.
  [[nodiscard]] auto write(VirtualAddress addr, EepromValue value) -> Result<void>;

  // Bulk-flush RAM image to the pre-erased shadow page via HAL write_page,
  // then promote status with a single write_word call.
  [[nodiscard]] auto sync() -> Result<void>;

  // Erase both pages, clear RAM image, reinitialise. Destructive.
  [[nodiscard]] auto format() -> Result<void>;

 private:
  Hal&      hal_;
  Image     image_{};
  PageIndex active_page_{0};

  [[nodiscard]] auto shadow_page() const noexcept -> PageIndex {
    return static_cast<PageIndex>((active_page_ + 1U) % kPageCount);
  }

  [[nodiscard]] auto read_image(PageIndex page, Image& out) const -> Result<void>;
  [[nodiscard]] auto write_image(PageIndex page, const Image& img) -> Result<void>;
  [[nodiscard]] auto read_page_status(PageIndex page) const -> Result<PageStatus>;
  [[nodiscard]] auto write_page_status(PageIndex page, PageStatus status) -> Result<void>;
};

}  // namespace eeprom

#include "eeprom/emulation_impl.hpp"
