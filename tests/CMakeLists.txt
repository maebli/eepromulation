add_executable(test_emulation
  test_emulation.cpp
)

target_link_libraries(test_emulation PRIVATE
  eeprom_emulation     # pulls in eeprom_headers (include/) transitively
  eeprom_hal_host      # HostFlashHal used in tests
  Catch2::Catch2WithMain
)

target_apply_warnings(test_emulation)

# Register with CTest
include(Catch)
catch_discover_tests(test_emulation)
