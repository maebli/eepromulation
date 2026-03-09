add_executable(stm32wl_c.elf
  ${CMAKE_CURRENT_SOURCE_DIR}/../startup.s
  stm32wl_compat.cpp
  main.c
)

target_link_libraries(stm32wl_c.elf PRIVATE
  eeprom_emulation
  eeprom_headers
)

target_link_options(stm32wl_c.elf PRIVATE
  -T ${CMAKE_CURRENT_SOURCE_DIR}/../stm32wl.ld
  --specs=nosys.specs
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/stm32wl_c.map
)

find_program(ARM_SIZE arm-none-eabi-size)
if(ARM_SIZE)
  add_custom_command(TARGET stm32wl_c.elf POST_BUILD
    COMMAND ${ARM_SIZE} $<TARGET_FILE:stm32wl_c.elf>
    COMMENT "ELF section sizes"
    VERBATIM
  )
endif()
