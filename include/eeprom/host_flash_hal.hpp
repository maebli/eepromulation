// Host (in-memory) flash HAL — header-only, for tests and QEMU demo.
//
// Template parameter PageSize controls logical page size (default: kPageSize).
// Simulates two flash pages as a contiguous std::array<std::byte>.
//
// Enforces real flash semantics:
//   - Erase sets all bytes in the page to 0xFF.
//   - write_word / write_page may only clear bits (1 → 0).
//   - write_page does a bulk memcpy after a byte-level constraint check.

#pragma once

#include <array>
#include <cstring>
#include <span>

#include "eeprom/hal.hpp"

namespace eeprom {

template <std::uint32_t PageSize = kPageSize>
class HostFlashHal final : public FlashHal {
 public:
  HostFlashHal() { storage_.fill(std::byte{0xFF}); }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto erase_page(PageIndex page) -> Result<void> override {
    if (page >= kPageCount) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    const std::uint32_t base = page * PageSize;
    std::fill(storage_.begin() + base, storage_.begin() + base + PageSize, std::byte{0xFF});
    return Result<void>::ok();
  }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto write_word(PageIndex page, std::uint32_t offset,
                                FlashWord data) -> Result<void> override {
    if (page >= kPageCount || offset % kWordSize != 0U || offset + kWordSize > PageSize) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    const std::uint32_t byte_offset = page * PageSize + offset;

    FlashWord current{};
    std::memcpy(&current, &storage_[byte_offset], kWordSize);

    if ((data & ~current) != 0U) {
      return Result<void>::err(FlashError::WriteFailure);
    }

    std::memcpy(&storage_[byte_offset], &data, kWordSize);
    return Result<void>::ok();
  }

  // -------------------------------------------------------------------------
  // Bulk-write a full page image.  Checks flash constraints byte-by-byte,
  // then copies in one memcpy — models the host as having byte write granularity.
  // -------------------------------------------------------------------------
  [[nodiscard]] auto write_page(PageIndex page,
                                std::span<const std::byte> data) -> Result<void> override {
    if (page >= kPageCount || data.size() > PageSize) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    const std::uint32_t base = page * PageSize;
    for (std::uint32_t i = 0; i < data.size(); ++i) {
      if ((data[i] & ~storage_[base + i]) != std::byte{0}) {
        return Result<void>::err(FlashError::WriteFailure);
      }
    }
    std::memcpy(&storage_[base], data.data(), data.size());
    return Result<void>::ok();
  }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto read_word(PageIndex page,
                               std::uint32_t offset) const -> Result<FlashWord> override {
    if (page >= kPageCount || offset % kWordSize != 0U || offset + kWordSize > PageSize) {
      return Result<FlashWord>::err(FlashError::InvalidAddress);
    }
    const std::uint32_t byte_offset = page * PageSize + offset;
    FlashWord word{};
    std::memcpy(&word, &storage_[byte_offset], kWordSize);
    return Result<FlashWord>::ok(word);
  }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto page_size()  const noexcept -> std::uint32_t override { return PageSize; }
  [[nodiscard]] auto page_count() const noexcept -> std::uint32_t override { return kPageCount; }

  // -------------------------------------------------------------------------
  // Test helper: expose raw storage for inspection / pre-seeding
  // -------------------------------------------------------------------------
  [[nodiscard]] auto raw_storage() noexcept
      -> std::array<std::byte, PageSize * kPageCount>& {
    return storage_;
  }
  [[nodiscard]] auto raw_storage() const noexcept
      -> const std::array<std::byte, PageSize * kPageCount>& {
    return storage_;
  }

 private:
  std::array<std::byte, PageSize * kPageCount> storage_{};
};

}  // namespace eeprom
