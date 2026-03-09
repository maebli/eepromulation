#pragma once

#include <concepts>
#include <cstdint>
#include <span>

#include "eeprom/types.hpp"

namespace eeprom {

// ---------------------------------------------------------------------------
// FlashHal — pure-virtual interface for flash hardware operations
// ---------------------------------------------------------------------------
class FlashHal {
 public:
  virtual ~FlashHal() = default;

  // Erase a single page (sets all bytes to 0xFF).
  [[nodiscard]] virtual auto erase_page(PageIndex page) -> Result<void> = 0;

  // Write a 32-bit word at byte-offset `offset` within page `page`.
  // Used only for the isolated status-word write (page promotion).
  // Flash semantics: bits may only be cleared (1 → 0).
  [[nodiscard]] virtual auto write_word(PageIndex page, std::uint32_t offset,
                                        FlashWord data) -> Result<void> = 0;

  // Bulk-write a full page image.
  // `data` points to exactly page_size() bytes starting from offset 0.
  // The shadow page must already be erased before calling this.
  // HAL implementations decide write granularity (32-bit, 64-bit, etc.).
  [[nodiscard]] virtual auto write_page(PageIndex                    page,
                                        std::span<const std::byte>   data) -> Result<void> = 0;

  // Read a 32-bit word from byte-offset `offset` within page `page`.
  [[nodiscard]] virtual auto read_word(PageIndex page,
                                       std::uint32_t offset) const -> Result<FlashWord> = 0;

  // Return the logical page size in bytes.
  [[nodiscard]] virtual auto page_size()  const noexcept -> std::uint32_t = 0;

  // Return the total number of emulation pages.
  [[nodiscard]] virtual auto page_count() const noexcept -> std::uint32_t = 0;
};

// ---------------------------------------------------------------------------
// FlashHalConcept — constrains EepromEmulation's Hal template parameter
// ---------------------------------------------------------------------------
template <typename T>
concept FlashHalConcept =
    requires(T& hal, const T& chal, PageIndex page, std::uint32_t offset,
             FlashWord word, std::span<const std::byte> page_data) {
      { hal.erase_page(page) }              -> std::same_as<Result<void>>;
      { hal.write_word(page, offset, word) } -> std::same_as<Result<void>>;
      { hal.write_page(page, page_data) }   -> std::same_as<Result<void>>;
      { chal.read_word(page, offset) }      -> std::same_as<Result<FlashWord>>;
      { chal.page_size() }                  -> std::same_as<std::uint32_t>;
      { chal.page_count() }                 -> std::same_as<std::uint32_t>;
    };

static_assert(FlashHalConcept<FlashHal>,
              "FlashHal must satisfy FlashHalConcept — check the interface");

}  // namespace eeprom
