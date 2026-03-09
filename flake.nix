{
  description = "eepromulation — EEPROM emulation in C++20";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        devShells.default = pkgs.mkShell {
          name = "eepromulation";

          packages = with pkgs; [
            # Build system
            cmake
            ninja

            # Host toolchain (clang for compile_commands.json + tests)
            clang
            clang-tools   # clang-tidy, clang-format

            # ARM cross-toolchain
            gcc-arm-embedded

            # QEMU for Cortex-M3 emulation
            qemu

            # Testing
            catch2_3

            # Static analysis
            cppcheck

            # Scripting (amalgamation generator)
            python3

            # Optional extras (gdb omitted — build fails on Darwin with clang 21)
            openocd        # for real hardware
          ];

          shellHook = ''
            echo "eepromulation dev shell"
            echo ""
            echo "  cmake --preset host-debug && cmake --build build/host-debug"
            echo "  ctest --preset host-debug"
            echo ""
            echo "  cmake --preset arm-debug && cmake --build build/arm-debug"
            echo "  nix develop --command qemu-system-arm -M lm3s6965evb -kernel build/arm-debug/target/demo.elf -semihosting -nographic -no-reboot"
          '';
        };
      }
    );
}
