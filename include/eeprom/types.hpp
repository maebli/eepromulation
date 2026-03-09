#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <variant>

namespace eeprom {

// ---------------------------------------------------------------------------
// Flash geometry constants
// ---------------------------------------------------------------------------
inline constexpr std::uint32_t kPageSize  = 1024U;  // default logical page size (bytes)
inline constexpr std::uint32_t kPageCount = 2U;     // pages used for emulation
inline constexpr std::uint32_t kWordSize  = 4U;     // bytes per flash word (write granularity)

static_assert(kPageCount <= 256U, "kPageCount must be ≤ 256 — PageIndex is uint8_t");

// ---------------------------------------------------------------------------
// Fundamental typedefs
// ---------------------------------------------------------------------------
using VirtualAddress = std::uint16_t;
using EepromValue    = std::uint16_t;
using FlashWord      = std::uint32_t;
using PageIndex      = std::uint8_t;

// ---------------------------------------------------------------------------
// Sentinel value — 0xFFFF is the erased flash state for a 16-bit slot.
// ---------------------------------------------------------------------------
inline constexpr EepromValue kValueNotPresent = 0xFFFFU;

// ---------------------------------------------------------------------------
// Page lifecycle states — stored as the first FlashWord of each page.
// ---------------------------------------------------------------------------
enum class PageStatus : FlashWord {
  Erased      = 0xFFFF'FFFFU,
  ReceiveData = 0xFFFF'EEEEU,
  Valid       = 0xFFFF'0000U,
};

// ---------------------------------------------------------------------------
// PageImage<PageSize> — RAM struct that mirrors the flash page layout exactly.
//
// Layout (PageSize bytes):
//   bytes 0–3             : PageStatus header
//   bytes 4–(PageSize-1)  : EepromValue array — one slot per virtual address
//
// Slot holds kValueNotPresent (0xFFFF) when never written.
// ---------------------------------------------------------------------------
template <std::uint32_t PageSize>
struct PageImage {
  static constexpr std::uint16_t kMaxAddresses =
      static_cast<std::uint16_t>((PageSize - kWordSize) / sizeof(EepromValue));

  PageStatus                              status;
  std::array<EepromValue, kMaxAddresses>  values;

  void clear() noexcept {
    status = PageStatus::Erased;
    values.fill(kValueNotPresent);
  }

  [[nodiscard]] bool is_present(VirtualAddress addr) const noexcept {
    return values[addr] != kValueNotPresent;
  }
};

static_assert(sizeof(PageImage<1024U>) == 1024U,
              "PageImage<1024> must be exactly 1024 bytes");
static_assert(sizeof(PageImage<2048U>) == 2048U,
              "PageImage<2048> must be exactly 2048 bytes");

// Convenience alias: kMaxVirtualAddresses for the default page size.
inline constexpr std::uint16_t kMaxVirtualAddresses = PageImage<kPageSize>::kMaxAddresses;

// ---------------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------------
enum class FlashError : std::uint8_t {
  EraseFailure,
  WriteFailure,
  ReadFailure,
  InvalidAddress,
  NoSpaceLeft,
  NotFound,
  Corrupted,
  InvalidPageStatus,
};

// ---------------------------------------------------------------------------
// Result<T, E>
// ---------------------------------------------------------------------------
template <typename T, typename E = FlashError>
struct Result {
  std::variant<T, E> storage;

  static constexpr Result ok(T v) {
    return Result{std::variant<T, E>{std::in_place_index<0>, std::move(v)}};
  }
  static constexpr Result err(E e) { return Result{std::variant<T, E>{std::in_place_index<1>, e}}; }

  [[nodiscard]] constexpr bool is_ok()  const noexcept { return storage.index() == 0; }
  [[nodiscard]] constexpr bool is_err() const noexcept { return storage.index() == 1; }

  [[nodiscard]] constexpr T& value() & {
    assert(is_ok());
    return std::get<0>(storage);
  }
  [[nodiscard]] constexpr const T& value() const& {
    assert(is_ok());
    return std::get<0>(storage);
  }
  [[nodiscard]] constexpr T value() && {
    assert(is_ok());
    return std::get<0>(std::move(storage));
  }
  [[nodiscard]] constexpr E error() const { return std::get<1>(storage); }

  [[nodiscard]] constexpr T value_or(T fallback) const {
    return is_ok() ? std::get<0>(storage) : fallback;
  }
};

template <typename E>
struct Result<void, E> {
  std::variant<std::monostate, E> storage;

  static constexpr Result ok() {
    return Result{std::variant<std::monostate, E>{std::in_place_index<0>}};
  }
  static constexpr Result err(E e) {
    return Result{std::variant<std::monostate, E>{std::in_place_index<1>, e}};
  }

  [[nodiscard]] constexpr bool is_ok()  const noexcept { return storage.index() == 0; }
  [[nodiscard]] constexpr bool is_err() const noexcept { return storage.index() == 1; }
  [[nodiscard]] constexpr E error() const { return std::get<1>(storage); }
};

inline Result<PageStatus, FlashError> try_from(FlashWord w) {
  switch (w) {
    case static_cast<FlashWord>(PageStatus::Erased):
      return Result<PageStatus, FlashError>::ok(PageStatus::Erased);
    case static_cast<FlashWord>(PageStatus::ReceiveData):
      return Result<PageStatus, FlashError>::ok(PageStatus::ReceiveData);
    case static_cast<FlashWord>(PageStatus::Valid):
      return Result<PageStatus, FlashError>::ok(PageStatus::Valid);
    default:
      return Result<PageStatus, FlashError>::err(FlashError::InvalidPageStatus);
  }
}

// ---------------------------------------------------------------------------
// overloaded — helper for std::visit with multiple lambda overloads
// ---------------------------------------------------------------------------
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

}  // namespace eeprom
