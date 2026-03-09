// STM32WL flash HAL for eepromulation
//
// Target: STM32WL55JCxx — 256 KB single-bank flash, 128 pages × 2 KB
// EEPROM emulation uses the last two physical pages (126 and 127).
//
// This HAL is designed for PageSize = 2048:
//   - write_page() packs each pair of consecutive 32-bit words into one
//     64-bit double-word write (required by the STM32WL flash controller).
//   - 512 logical words × 8 physical bytes = 2048 bytes = exactly one 2 KB page.
//   - No offset doubling. Physical and logical byte offsets are identical.
//   - 1022 virtual addresses, 2 physical pages (4 KB) total.
//
// write_word() is called only for the isolated status promotion at the end of
// sync (ReceiveData → Valid at offset 0). It reads back the already-written
// partner word to complete the 64-bit pair without changing any bits.
//
// Reference: RM0453 Rev 4, §3 Flash memory.

#pragma once

#include <cstdint>
#include <span>

#include "eeprom/hal.hpp"

namespace eeprom {

class Stm32WlFlashHal final : public FlashHal {
 public:
  // -------------------------------------------------------------------------
  // Flash geometry
  // -------------------------------------------------------------------------
  static constexpr std::uint32_t kLogicalPageSize = 2048U;  // must match EepromEmulation PageSize
  static constexpr std::uint32_t kPhysPageSize    = 2048U;  // STM32WL erase unit (bytes)
  static constexpr std::uint32_t kFlashBase       = 0x0800'0000U;

  // Physical page indices for the two emulation pages.
  // Defaults to the last two pages of the STM32WL55JCxx (256 KB, 128 pages):
  //   page 126 → 0x080FC000 .. 0x080FDFFF
  //   page 127 → 0x080FE000 .. 0x080FFFFF
  // Override for smaller variants, e.g. STM32WL55JBxx (128 KB, 64 pages):
  //   Stm32WlFlashHal hal{62U, 63U};
  explicit Stm32WlFlashHal(std::uint32_t page0_idx = 126U,
                            std::uint32_t page1_idx = 127U) noexcept
      : phys_page_{page0_idx, page1_idx} {}

  // -------------------------------------------------------------------------
  // FLASH peripheral registers (RM0453 §3.7, base 0x58004000)
  // -------------------------------------------------------------------------
  static constexpr std::uint32_t kRegBase    = 0x5800'4000U;
  static constexpr std::uint32_t kKeyrOffset = 0x008U;
  static constexpr std::uint32_t kSrOffset   = 0x010U;
  static constexpr std::uint32_t kCrOffset   = 0x014U;

  static constexpr std::uint32_t kKey1 = 0x4567'0123U;
  static constexpr std::uint32_t kKey2 = 0xCDEF'89ABU;

  // FLASH_CR bits
  static constexpr std::uint32_t kCrPg       = (1U << 0);
  static constexpr std::uint32_t kCrPer      = (1U << 1);
  static constexpr std::uint32_t kCrPnbShift = 3U;
  static constexpr std::uint32_t kCrStrt     = (1U << 16);
  static constexpr std::uint32_t kCrLock     = (1U << 31);

  // FLASH_SR bits
  static constexpr std::uint32_t kSrEop = (1U << 0);
  static constexpr std::uint32_t kSrErr = (0x3FU << 1);
  static constexpr std::uint32_t kSrBsy = (1U << 16);

  // -------------------------------------------------------------------------
  [[nodiscard]] auto erase_page(PageIndex page) -> Result<void> override {
    if (page >= kPageCount) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    if (!unlock()) {
      return Result<void>::err(FlashError::EraseFailure);
    }
    flash_cr() = kCrPer | (phys_page_[page] << kCrPnbShift) | kCrStrt;
    wait_bsy();
    const bool ok = (flash_sr() & kSrErr) == 0U;
    flash_sr() = kSrEop | kSrErr;
    lock();
    return ok ? Result<void>::ok() : Result<void>::err(FlashError::EraseFailure);
  }

  // -------------------------------------------------------------------------
  // write_page — bulk 64-bit packed write of a full PageImage.
  //
  // Iterates the image bytes in 8-byte chunks, writing each as one 64-bit
  // double-word.  For PageSize = 2048: 256 pairs × 8 bytes = 2048 bytes,
  // filling exactly one 2 KB physical page.
  // -------------------------------------------------------------------------
  [[nodiscard]] auto write_page(PageIndex                  page,
                                std::span<const std::byte> data) -> Result<void> override {
    if (page >= kPageCount) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    if (!unlock()) {
      return Result<void>::err(FlashError::WriteFailure);
    }
    flash_cr() = kCrPg;

    const std::uint32_t phys_base = kFlashBase + phys_page_[page] * kPhysPageSize;
    const auto*         words     = reinterpret_cast<const std::uint32_t*>(data.data());
    const std::uint32_t pairs     = static_cast<std::uint32_t>(data.size()) / 8U;

    for (std::uint32_t i = 0; i < pairs; ++i) {
      const std::uint32_t lo  = words[i * 2U];
      const std::uint32_t hi  = words[i * 2U + 1U];
      auto*               ptr = reinterpret_cast<volatile std::uint32_t*>(phys_base + i * 8U);
      ptr[0] = lo;
      ptr[1] = hi;
      wait_bsy();
      if ((flash_sr() & kSrErr) != 0U) {
        flash_sr() = kSrEop | kSrErr;
        flash_cr() &= ~kCrPg;
        lock();
        return Result<void>::err(FlashError::WriteFailure);
      }
      flash_sr() = kSrEop;
    }

    flash_cr() &= ~kCrPg;
    lock();
    return Result<void>::ok();
  }

  // -------------------------------------------------------------------------
  // write_word — isolated status-word promotion (ReceiveData → Valid).
  //
  // Called only by write_page_status at offset 0.  The status word (offset 0)
  // shares a 64-bit physical slot with values[0,1] (offset 4), which was
  // already written by write_page.  Reads back the partner to re-issue the
  // same 64-bit write with only the status bits cleared further.
  // -------------------------------------------------------------------------
  [[nodiscard]] auto write_word(PageIndex page, std::uint32_t offset,
                                FlashWord data) -> Result<void> override {
    if (page >= kPageCount || offset + kWordSize > kLogicalPageSize) {
      return Result<void>::err(FlashError::InvalidAddress);
    }
    const std::uint32_t phys_addr = kFlashBase + phys_page_[page] * kPhysPageSize + offset;

    // Read back the already-written partner word in the same 64-bit slot
    const auto*        ro_ptr  = reinterpret_cast<const volatile std::uint32_t*>(phys_addr + 4U);
    const std::uint32_t partner = *ro_ptr;

    if (!unlock()) {
      return Result<void>::err(FlashError::WriteFailure);
    }
    flash_cr() = kCrPg;

    auto* ptr = reinterpret_cast<volatile std::uint32_t*>(phys_addr);
    ptr[0] = data;     // only clears bits (ReceiveData → Valid)
    ptr[1] = partner;  // same value — no bits change

    wait_bsy();
    const bool ok = (flash_sr() & kSrErr) == 0U;
    flash_sr() = kSrEop | kSrErr;
    flash_cr() &= ~kCrPg;
    lock();
    return ok ? Result<void>::ok() : Result<void>::err(FlashError::WriteFailure);
  }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto read_word(PageIndex page,
                               std::uint32_t offset) const -> Result<FlashWord> override {
    if (page >= kPageCount || offset + kWordSize > kLogicalPageSize) {
      return Result<FlashWord>::err(FlashError::InvalidAddress);
    }
    // Flash is memory-mapped — direct read, 1:1 offset (no doubling)
    const std::uint32_t phys_addr = kFlashBase + phys_page_[page] * kPhysPageSize + offset;
    const auto*         ptr       = reinterpret_cast<const volatile std::uint32_t*>(phys_addr);
    return Result<FlashWord>::ok(*ptr);
  }

  // -------------------------------------------------------------------------
  [[nodiscard]] auto page_size()  const noexcept -> std::uint32_t override {
    return kLogicalPageSize;
  }
  [[nodiscard]] auto page_count() const noexcept -> std::uint32_t override {
    return kPageCount;
  }

 private:
  std::uint32_t phys_page_[kPageCount];

  static auto reg(std::uint32_t offset) noexcept -> volatile std::uint32_t& {
    return *reinterpret_cast<volatile std::uint32_t*>(kRegBase + offset);
  }
  static auto flash_cr()   noexcept -> volatile std::uint32_t& { return reg(kCrOffset);   }
  static auto flash_sr()   noexcept -> volatile std::uint32_t& { return reg(kSrOffset);   }
  static auto flash_keyr() noexcept -> volatile std::uint32_t& { return reg(kKeyrOffset); }

  static bool unlock() noexcept {
    if ((flash_cr() & kCrLock) != 0U) {
      flash_keyr() = kKey1;
      flash_keyr() = kKey2;
    }
    return (flash_cr() & kCrLock) == 0U;
  }

  static void lock()     noexcept { flash_cr() |= kCrLock; }
  static void wait_bsy() noexcept { while ((flash_sr() & kSrBsy) != 0U) {} }
};

}  // namespace eeprom
