// emulation_impl.hpp — template method implementations for EepromEmulation

#pragma once

#include "eeprom/emulation.hpp"
#include "eeprom/types.hpp"

#include <span>

namespace eeprom {

// ---------------------------------------------------------------------------
// read_page_status
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::read_page_status(PageIndex page) const -> Result<PageStatus> {
  auto r = hal_.read_word(page, 0);
  if (r.is_err()) {
    return Result<PageStatus>::err(r.error());
  }
  return try_from(r.value());
}

// ---------------------------------------------------------------------------
// write_page_status — single 32-bit status word write (page promotion only)
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::write_page_status(PageIndex page, PageStatus status)
    -> Result<void> {
  return hal_.write_word(page, 0, static_cast<FlashWord>(status));
}

// ---------------------------------------------------------------------------
// read_image — load one flash page into a PageImage struct (word by word)
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::read_image(PageIndex page, Image& out) const -> Result<void> {
  auto* words = reinterpret_cast<FlashWord*>(&out);
  for (std::uint32_t i = 0; i < PageSize / kWordSize; ++i) {
    auto r = hal_.read_word(page, i * kWordSize);
    if (r.is_err()) {
      return Result<void>::err(r.error());
    }
    words[i] = r.value();
  }
  return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// write_image — bulk-write a PageImage to one flash page via HAL write_page
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::write_image(PageIndex page, const Image& img) -> Result<void> {
  const auto* bytes = reinterpret_cast<const std::byte*>(&img);
  return hal_.write_page(page, std::span<const std::byte>{bytes, PageSize});
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::init() -> Result<void> {
  auto s0 = read_page_status(0);
  auto s1 = read_page_status(1);

  return std::visit(
      overloaded{

          [&](PageStatus p0, PageStatus p1) -> Result<void> {
            // Both Erased — first boot
            if (p0 == PageStatus::Erased && p1 == PageStatus::Erased) {
              image_.clear();
              image_.status = PageStatus::Valid;
              active_page_ = 0;
              return write_page_status(0, PageStatus::Valid);
            }
            // Both Valid — interrupted erase after sync, page 0 wins tiebreaker
            if (p0 == PageStatus::Valid && p1 == PageStatus::Valid) {
              image_.clear();
              active_page_ = 0;
              if (auto r = read_image(0, image_); r.is_err()) {
                return r;
              }
              return hal_.erase_page(1);
            }
            // page 0 Valid — normal boot or interrupted sync on page 1
            if (p0 == PageStatus::Valid) {
              active_page_ = 0;
              if (auto r = read_image(0, image_); r.is_err()) {
                return r;
              }
              if (p1 == PageStatus::ReceiveData) {
                return hal_.erase_page(1);
              }
              return Result<void>::ok();
            }
            // page 1 Valid — normal boot or interrupted sync on page 0
            if (p1 == PageStatus::Valid) {
              active_page_ = 1;
              if (auto r = read_image(1, image_); r.is_err()) {
                return r;
              }
              if (p0 == PageStatus::ReceiveData) {
                return hal_.erase_page(0);
              }
              return Result<void>::ok();
            }
            return Result<void>::err(FlashError::Corrupted);
          },

          [](auto&&...) -> Result<void> { return Result<void>::err(FlashError::Corrupted); }

      },
      s0.storage, s1.storage);
}

// ---------------------------------------------------------------------------
// read
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::read(VirtualAddress addr) const -> Result<EepromValue> {
  if (addr >= kMaxAddresses) {
    return Result<EepromValue>::err(FlashError::InvalidAddress);
  }
  return Result<EepromValue>::ok(image_.values[addr]);
}

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::write(VirtualAddress addr, EepromValue value) -> Result<void> {
  if (addr >= kMaxAddresses) {
    return Result<void>::err(FlashError::InvalidAddress);
  }
  image_.values[addr] = value;
  return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// sync
//
// Flow:
//   1. write_image  — bulk-writes full image (status=ReceiveData) to shadow page
//   2. write_word   — single status promotion: ReceiveData → Valid at offset 0
//   3. erase_page   — lazy erase of the old active page
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::sync() -> Result<void> {
  image_.status = PageStatus::ReceiveData;
  if (auto r = write_image(shadow_page(), image_); r.is_err()) {
    return r;
  }
  if (auto r = write_page_status(shadow_page(), PageStatus::Valid); r.is_err()) {
    return r;
  }
  if (auto r = hal_.erase_page(active_page_); r.is_err()) {
    return r;
  }
  active_page_ = shadow_page();
  image_.status = PageStatus::Valid;
  return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// format
// ---------------------------------------------------------------------------
template <FlashHalConcept Hal, std::uint32_t PageSize>
auto EepromEmulation<Hal, PageSize>::format() -> Result<void> {
  if (auto r = hal_.erase_page(0); r.is_err()) {
    return r;
  }
  if (auto r = hal_.erase_page(1); r.is_err()) {
    return r;
  }
  image_.clear();
  active_page_ = 0;
  image_.status = PageStatus::Valid;
  return write_page_status(0, PageStatus::Valid);
}

}  // namespace eeprom
