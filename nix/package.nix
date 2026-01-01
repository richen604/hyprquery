{
  lib,
  stdenv,
  cmake,
  pkg-config,
  spdlog,
  nlohmann_json,
  cli11,
  hyprlang,
  autoPatchelfHook,
  version,
  src,
}:
stdenv.mkDerivation {
  pname = "hyprquery";
  version = version;

  inherit src;

  nativeBuildInputs = [
    cmake
    pkg-config
    autoPatchelfHook
  ];

  buildInputs = [
    spdlog
    nlohmann_json
    cli11
    hyprlang
  ];

  prePatch = ''
    # Use the nix-optimized CMakeLists.txt
    cp nix/CMakeLists.txt CMakeLists.txt
  '';

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DUSE_SYSTEM_SPDLOG=ON"
    "-DUSE_SYSTEM_HYPRLANG=ON"
  ];

  meta = with lib; {
    description = "A command-line utility for querying configuration values from Hyprland";
    homepage = "https://github.com/HyDE-Project/hyprquery";
    license = licenses.mit;
    maintainers = [ "richen604" ];
    platforms = platforms.linux;
    mainProgram = "hyq";
  };
}
