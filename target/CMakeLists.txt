add_executable(demo.elf
  startup.s
  main.cpp
)

target_link_libraries(demo.elf PRIVATE
  eeprom_emulation
  eeprom_headers
)

# Linker script + semihosting
target_link_options(demo.elf PRIVATE
  -T ${CMAKE_CURRENT_SOURCE_DIR}/lm3s6965evb.ld
  --specs=rdimon.specs          # semihosting (printf via QEMU)
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/demo.map
)

target_apply_warnings(demo.elf)

# Print size after build
find_program(ARM_SIZE arm-none-eabi-size)
if(ARM_SIZE)
  add_custom_command(TARGET demo.elf POST_BUILD
    COMMAND ${ARM_SIZE} $<TARGET_FILE:demo.elf>
    COMMENT "ELF section sizes"
    VERBATIM
  )
endif()
