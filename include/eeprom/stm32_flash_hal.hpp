// STM32 FLASH peripheral HAL — stub header
//
// TODO: implement using STM32 FLASH registers (or STM32 HAL library).
// This header is only compiled in the ARM build (cmake preset: arm-debug/arm-release).

#pragma once

#include <span>

#include "eeprom/hal.hpp"

namespace eeprom {

class Stm32FlashHal final : public FlashHal {
 public:
  static constexpr std::uint32_t kPage0BaseAddr = 0x0800'F000U;

  [[nodiscard]] auto erase_page(PageIndex page) -> Result<void> override;

  [[nodiscard]] auto write_word(PageIndex page, std::uint32_t offset,
                                FlashWord data) -> Result<void> override;

  [[nodiscard]] auto write_page(PageIndex page,
                                std::span<const std::byte> data) -> Result<void> override;

  [[nodiscard]] auto read_word(PageIndex page,
                               std::uint32_t offset) const -> Result<FlashWord> override;

  [[nodiscard]] auto page_size()  const noexcept -> std::uint32_t override { return kPageSize; }
  [[nodiscard]] auto page_count() const noexcept -> std::uint32_t override { return kPageCount; }
};

}  // namespace eeprom
