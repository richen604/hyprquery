{
  description = "HyprQuery - A command-line utility for querying configuration values from Hyprland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/cad22e7d996aea55ecab064e84834289143e44a0";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:
      let
    system = "x86_64-linux";
        pkgs = nixpkgs.legacyPackages.${system};
        version = builtins.readFile ./VERSION;
      in
      {
        packages.${system} = {
          default = self.packages.${system}.hyprquery;
          hyprquery = pkgs.callPackage ./nix/package.nix {
            inherit version;
            src = ./.;
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Build tools
            cmake
            pkg-config
            clang-tools

            # Dependencies
            spdlog
            nlohmann_json
            cli11
            hyprlang

            # Development tools
            gdb
            valgrind
          ];

          shellHook = ''
            echo "🔧 HyprQuery development environment"
            echo "📦 Available packages: cmake, pkg-config, spdlog, nlohmann_json, cli11, hyprlang"
            echo "🛠️  Available tools: clang-tools, gdb, valgrind"

            # Set up build directory
            if [ ! -d build ]; then
              echo "📁 Creating build directory..."
              mkdir -p build
            fi

            # Export compile commands for clangd
            export CMAKE_EXPORT_COMPILE_COMMANDS=ON
          '';
        };
      };
}

