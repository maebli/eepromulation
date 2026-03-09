add_subdirectory(cpp)
add_subdirectory(c)

# ---------------------------------------------------------------------------
# Distributable static library — drop-in replacement for ST's AN2594 eeprom.c.
#
# Produces libeepromulation_stm32wl.a containing the compiled C API
# (EE_Init / EE_ReadVariable / EE_WriteVariable / EE_Sync / EE_Format).
# Users link against it from pure C — no C++ compiler needed on their side.
#
# Usage:
#   arm-none-eabi-gcc main.c -I<path/to/eeprom.h> \
#       -L<path/to/lib> -leepromulation_stm32wl -o app.elf
# ---------------------------------------------------------------------------
add_library(eepromulation_stm32wl STATIC
  c/stm32wl_compat.cpp
)
target_link_libraries(eepromulation_stm32wl PUBLIC eeprom_headers)

install(TARGETS eepromulation_stm32wl
  ARCHIVE DESTINATION lib
)
install(FILES ${CMAKE_SOURCE_DIR}/include/eeprom/an2594_compat.h
  DESTINATION include
  RENAME eeprom.h
)
