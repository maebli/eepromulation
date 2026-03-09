// STM32 FLASH peripheral HAL — method definitions
//
// TODO: implement using STM32 FLASH registers (or STM32 HAL library).
// This file is only compiled in the ARM build (cmake preset: arm-debug/arm-release).

#include "eeprom/stm32_flash_hal.hpp"

namespace eeprom {

auto Stm32FlashHal::erase_page(PageIndex page) -> Result<void> {
  (void)page;
  // TODO: unlock flash, perform page erase, lock flash
  return Result<void>::err(FlashError::EraseFailure);
}

auto Stm32FlashHal::write_word(PageIndex page, std::uint32_t offset,
                               FlashWord data) -> Result<void> {
  (void)page;
  (void)offset;
  (void)data;
  // TODO: unlock flash, program word, lock flash, verify
  return Result<void>::err(FlashError::WriteFailure);
}

auto Stm32FlashHal::read_word(PageIndex page,
                              std::uint32_t offset) const -> Result<FlashWord> {
  (void)page;
  (void)offset;
  // TODO: read via memory-mapped pointer: kPage0BaseAddr + page * kPageSize + offset
  return Result<FlashWord>::err(FlashError::ReadFailure);
}

auto Stm32FlashHal::write_page(PageIndex page,
                               std::span<const std::byte> data) -> Result<void> {
  (void)page;
  (void)data;
  // TODO: iterate data in 8-byte (64-bit) chunks and program each double-word
  return Result<void>::err(FlashError::WriteFailure);
}

}  // namespace eeprom
